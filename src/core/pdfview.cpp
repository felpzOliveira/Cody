#include <pdfview.h>
#include <utilities.h>
#include <unordered_map>
#include <file_provider.h>

//#define LOG_PDF_OPEN(ptr) printf("[PDF] Open (%p)\n", ptr)
//#define LOG_PDF_CLOSE(ptr) printf("[PDF] Close (%p)\n", ptr)
#define LOG_PDF_OPEN(ptr)
#define LOG_PDF_CLOSE(ptr)

#if !defined(POPPLER_CPP_ENABLED) && !defined(POPPLER_GLIB_ENABLED)
struct PdfViewState{
    int dummy;
};

bool PdfView_IsEnabled(){ return false; }
bool PdfView_OpenDocument(PdfViewState **, const char *, uint,
                          const char *, uint){ return false; }
void PdfView_CloseDocument(PdfViewState **){}
int PdfView_GetNumPages(PdfViewState *){ return -1; }
void PdfView_OpenPage(PdfViewState *, int){}
void PdfView_NextPage(PdfViewState *){}
void PdfView_ClearPendingFlag(PdfViewState *){}
bool PdfView_Changed(PdfViewState *){ return false; }
void PdfView_PreviousPage(PdfViewState *){}
const char *PdfView_GetCurrentPage(PdfViewState *, int &, int &){ return nullptr; }
const char *PdfView_GetImagePage(PdfViewState *, int,
                                 int &, int &){ return nullptr; }

bool PdfView_ProjectCoordinate(PdfViewState *, vec2f, Geometry *, vec2f &){ return false; }
void PdfView_MouseMotion(PdfViewState *, vec2f, Geometry *){}
vec2f PdfView_GetZoomCenter(PdfViewState *){ return vec2f(0.5, 0.5); }
Float PdfView_GetZoomLevel(PdfViewState *){ return 1.0f; }
void PdfView_SetZoomCenter(PdfViewState *, vec2f){}
void PdfView_EnableZoom(PdfViewState *){}
void PdfView_UnlockZoom(PdfViewState *){}
void PdfView_LockZoom(PdfViewState *){}
void PdfView_DisableZoom(PdfViewState *){}
void PdfView_IncreaseZoomLevel(PdfViewState *){}
void PdfView_DecreaseZoomLevel(PdfViewState *){}
bool PdfView_IsZoomLocked(PdfViewState *){ return false; }
void PdfView_ResetZoom(PdfViewState *){}
bool PdfView_Reload(PdfViewState **){ return false; }

#else
#include <iostream>

struct PdfViewGraphics{
    int width, height;
    vec2f zoomCenter;
    Float zoomLevel;
    bool zooming;
    bool zoomLocked;
    Float xpos, ypos;
    bool pressed;
};

#if defined(DPOPPLER_GLIB_ENABLED)
#include <poppler/glib/poppler.h>
#include <cairo.h>

// TODO: how about caching some pages in a map so we can easily access them?

struct PdfViewState{
    PopplerDocument *document;
    unsigned char *pixels;
    char *pdfBytes;
    std::string docPath;
    int pageIndex;
    bool changed;
    PdfViewGraphics graphics;
};

static
void PdfView_Debug(PdfViewState *pdfView){
    printf(" --- PdfView (%p)\n", pdfView);
    printf("Doc: %p\n", pdfView->document);
    printf("Bytes: %p\n", pdfView->pdfBytes);
    printf("Pixels: %p\n", pdfView->pixels);
    printf("PageIndex: %d\n", pdfView->pageIndex);
    printf("Width: %d\n", pdfView->graphics.width);
    printf("Height: %d\n", pdfView->graphics.height);
    printf("Changed: %d\n", pdfView->changed);
    printf("------------------------\n");
}

bool PdfView_IsEnabled(){
    return true;
}

static
void PdfView_Cleanup(PdfViewState **pdfView){
    if(pdfView && *pdfView){
        LOG_PDF_CLOSE((*pdfView)->document);
        if((*pdfView)->document){
            g_object_unref((*pdfView)->document);
            (*pdfView)->document = nullptr;
        }

        if((*pdfView)->pixels){
            delete[] (*pdfView)->pixels;
            (*pdfView)->pixels = nullptr;
        }

        if((*pdfView)->pdfBytes){
            AllocatorFree((*pdfView)->pdfBytes);
            (*pdfView)->pdfBytes = nullptr;
        }
    }
}

void PdfView_CloseDocument(PdfViewState **pdfView){
    if(pdfView && *pdfView){
        PdfView_Cleanup(pdfView);

        delete *pdfView;
        *pdfView = nullptr;
    }
}

bool PdfView_OpenDocument(PdfViewState **pdfView, const char *pdf, uint len,
                          const char *path, uint pathLen)
{
    PopplerDocument *doc = nullptr;
    PdfViewState *view = nullptr;
    GError *error = nullptr;
    // NOTE: We need to keep a copy of the bytes alive or we cant render
    char *pdfCopy = AllocatorGetN(char, len);

    Memcpy(pdfCopy, (char *)pdf, len);

    doc = poppler_document_new_from_data(pdfCopy, len, nullptr, &error);
    if(!doc){
        std::cout << "Error: Could not open PDF file. " <<
                (error ? error->message : "Unknown error") << std::endl;
        if (error) g_error_free(error);
        AllocatorFree(pdfCopy);
        return false;
    }

    if(*pdfView){
        PdfView_Cleanup(pdfView);
        view = *pdfView;
    }else{
        view = new PdfViewState;
    }

    view->docPath = std::string(path, pathLen);
    view->document = doc;
    view->pdfBytes = pdfCopy;
    view->pixels = nullptr;
    view->pageIndex = 0;
    view->changed = false;
    view->graphics.width = 0;
    view->graphics.height = 0;
    view->graphics.zooming = true;
    view->graphics.zoomLocked = false;
    view->graphics.xpos = 0;
    view->graphics.ypos = 0;
    view->graphics.pressed = false;
    view->graphics.zoomLevel = 1.0f;
    view->graphics.zoomCenter = vec2f(0.5, 0.5);
    *pdfView = view;
    if (error) g_error_free(error);

    LOG_PDF_OPEN(view->document);
    return true;
}

int PdfView_GetNumPages(PdfViewState *pdfView){
    if(!pdfView->document)
        return -1;

    return poppler_document_get_n_pages(pdfView->document);
}

void PdfView_OpenPage(PdfViewState *pdfView, int pageIndex){
    const char *ptr = PdfView_GetImagePage(pdfView, pageIndex, pdfView->graphics.width,
                                           pdfView->graphics.height);
    if(ptr){
        pdfView->pageIndex = pageIndex;
        pdfView->changed = true;
    }else{
        printf("Failed to open page\n");
    }
}

void PdfView_PreviousPage(PdfViewState *pdfView){
    int totalPages = PdfView_GetNumPages(pdfView);
    if(totalPages < 0){
        printf("Failed to get page count\n");
        return;
    }

    if(pdfView->pageIndex > 0){
        pdfView->pageIndex -= 1;
        (void)PdfView_GetImagePage(pdfView, pdfView->pageIndex,
                                   pdfView->graphics.width, pdfView->graphics.height);
        pdfView->changed = true;
    }
}

void PdfView_ClearPendingFlag(PdfViewState *pdfView){
    pdfView->changed = false;
}

bool PdfView_Changed(PdfViewState *pdfView){
    return pdfView->changed;
}

void PdfView_NextPage(PdfViewState *pdfView){
    int totalPages = PdfView_GetNumPages(pdfView);
    if(totalPages < 0){
        printf("Failed to get page count\n");
        return;
    }

    if(totalPages > pdfView->pageIndex+1){
        pdfView->pageIndex += 1;
        (void)PdfView_GetImagePage(pdfView, pdfView->pageIndex,
                                   pdfView->graphics.width, pdfView->graphics.height);
        pdfView->changed = true;
    }
}

const char *PdfView_GetCurrentPage(PdfViewState *pdfView, int &width, int &height){
    width = pdfView->graphics.width;
    height = pdfView->graphics.height;
    return (const char *)pdfView->pixels;
}

const char* get_action_type_string(PopplerActionType type) {
    switch (type) {
        case POPPLER_ACTION_NONE: return "None";
        case POPPLER_ACTION_GOTO_DEST: return "Go to destination";
        case POPPLER_ACTION_GOTO_REMOTE: return "Go to remote";
        case POPPLER_ACTION_LAUNCH: return "Launch";
        case POPPLER_ACTION_URI: return "URI";
        case POPPLER_ACTION_NAMED: return "Named";
        case POPPLER_ACTION_MOVIE: return "Movie";
        case POPPLER_ACTION_RENDITION: return "Rendition";
        case POPPLER_ACTION_OCG_STATE: return "OCG State";
        case POPPLER_ACTION_JAVASCRIPT: return "JavaScript";
        default: return "Unknown";
    }
}

const char *get_goto_dest_type_string(PopplerDestType type){
#define LIN_RET(x) case(x): return #x;
    switch(type){
        LIN_RET(POPPLER_DEST_UNKNOWN)
        LIN_RET(POPPLER_DEST_XYZ)
        LIN_RET(POPPLER_DEST_FIT)
        LIN_RET(POPPLER_DEST_FITH)
        LIN_RET(POPPLER_DEST_FITV)
        LIN_RET(POPPLER_DEST_FITR)
        LIN_RET(POPPLER_DEST_FITB)
        LIN_RET(POPPLER_DEST_FITBH)
        LIN_RET(POPPLER_DEST_FITBV)
        LIN_RET(POPPLER_DEST_NAMED)
        default: return "Unknown";
    }
#undef LIN_RET
}

int GetDestPage(PopplerDocument *document, PopplerDest *dest){
    printf("Type= %s\n", get_goto_dest_type_string(dest->type));
    return dest->page_num;
}

const char *PdfView_GetImagePage(PdfViewState *pdfView, int pageIndex,
                                 int &width, int &height)
{
    cairo_surface_t *surface = nullptr;
    PopplerPage *page = nullptr;
    cairo_t *cr = nullptr;
    double dWidth = 0, dHeight = 0;
    unsigned char *pixels = nullptr;
    int stride = 0;
    int iterator = 0;
    char *hptr = nullptr;
    float scale = 4.0f;

    GList *links = nullptr;

    if(!pdfView->document)
        return nullptr;

    if(poppler_document_get_n_pages(pdfView->document) <= pageIndex)
        return nullptr;

    // TODO: Do we need to free this?
    page = poppler_document_get_page(pdfView->document, pageIndex);
    if(!page){
        printf("Could not get page %d\n", pageIndex);
        return nullptr;
    }

    if(pdfView->pixels){
        delete[] pdfView->pixels;
        pdfView->pixels = nullptr;
    }
#if 0
    links = poppler_page_get_link_mapping(page);
    if(!links){
        printf("NO LINKS IN PAGE\n");
    }else{
        for(GList *l = links; l != nullptr; l = l->next){
            PopplerLinkMapping *link_mapping = (PopplerLinkMapping *)l->data;
            PopplerAction *action = link_mapping->action;
            if(action->type == POPPLER_ACTION_GOTO_DEST){
                PopplerActionGotoDest *goto_dest = (PopplerActionGotoDest *)action;
                int dest_page = GetDestPage(pdfView->document, goto_dest->dest);

                PopplerDest *dest =
                    poppler_document_find_dest(pdfView->document,
                                        goto_dest->dest->named_dest);

                //poppler_named_dest_to_bytestring();

                printf("Link for page: %d (%s) ==> (%d)\n", dest_page,
                       goto_dest->dest->named_dest, dest->page_num);
            }

            //poppler_action_free(action);
            //g_free(link_mapping);
        }
    }
#endif
    poppler_page_get_size(page, &dWidth, &dHeight);
    width  = static_cast<int>(dWidth) * scale;
    height = static_cast<int>(dHeight) * scale;

    surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);

    if(!surface){
        printf("Could not create surface\n");
        goto __ret;
    }

    cr = cairo_create(surface);
    if(!cr){
        printf("cairo_create failed\n");
        goto __ret;
    }

    // White background
    cairo_scale(cr, scale, scale);
    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    cairo_paint(cr);

    poppler_page_render(page, cr);

    pixels = cairo_image_surface_get_data(surface);
    stride = cairo_image_surface_get_stride(surface);

    pdfView->pixels = new unsigned char[stride * height];
    if(!pdfView->pixels){
        printf("Could not allocate memory\n");
        goto __ret;
    }

    hptr = (char *)pixels + stride * (height-1);
    for(int y = 0; y < height; y++){
        for(int x = 0; x < width; x++){
            const unsigned int pixel = *reinterpret_cast<unsigned int *>(hptr + x * 4);
            unsigned char r = (pixel >> 16) & 0xff;
            unsigned char g = (pixel >> 8) & 0xff;
            unsigned char b = pixel & 0xff;
            pdfView->pixels[4 * iterator + 0] = r;
            pdfView->pixels[4 * iterator + 1] = g;
            pdfView->pixels[4 * iterator + 2] = b;
            pdfView->pixels[4 * iterator + 3] = 255;
            iterator += 1;
        }

        hptr -= stride;
    }

__ret:
    if(cr) cairo_destroy(cr);
    if(surface) cairo_surface_destroy(surface);
    return (const char *)pdfView->pixels;
}


#else
#include <poppler/cpp/poppler-document.h>
#include <poppler/cpp/poppler-page.h>
#include <poppler/cpp/poppler-page-renderer.h>

struct PdfViewState{
    poppler::document *document;
    poppler::page *page;
    poppler::image img;
    unsigned char *pixels;
    std::string docPath;
    int pageIndex;
    bool changed;
    PdfViewGraphics graphics;
};

bool PdfView_IsEnabled(){ return true; }

void PdfView_ClearPendingFlag(PdfViewState *pdfView){
    pdfView->changed = false;
}

bool PdfView_Changed(PdfViewState *pdfView){
    return pdfView->changed;
}

static
void PdfView_Cleanup(PdfViewState **pdfView){
    if(pdfView && *pdfView){
        LOG_PDF_CLOSE((*pdfView)->document);
        if((*pdfView)->page){
            delete (*pdfView)->page;
            (*pdfView)->page = nullptr;
        }

        if((*pdfView)->document){
            delete (*pdfView)->document;
            (*pdfView)->document = nullptr;
        }

        if((*pdfView)->pixels){
            delete[] (*pdfView)->pixels;
            (*pdfView)->pixels = nullptr;
        }
    }
}

void PdfView_CloseDocument(PdfViewState **pdfView){
    if(pdfView && *pdfView){
        PdfView_Cleanup(pdfView);

        delete *pdfView;
        *pdfView = nullptr;
    }
}

bool PdfView_OpenDocument(PdfViewState **pdfView, const char *pdf, uint len,
                          const char *path, uint pathLen)
{
    PdfViewState *view = nullptr;
    std::vector<char> raw_data(pdf, pdf + len);
    poppler::document *doc = poppler::document::load_from_data(&raw_data);
    if(!doc){
        printf("Could not open PDF file\n");
        return false;
    }

    if(*pdfView){
        PdfView_Cleanup(pdfView);
        view = *pdfView;
    }else{
        view = new PdfViewState;
    }

    view->docPath = std::string(path, pathLen);
    view->document = doc;
    view->page = nullptr;
    view->pixels = nullptr;
    view->pageIndex = 0;
    view->changed = false;
    view->graphics.width = 0;
    view->graphics.height = 0;
    view->graphics.zooming = true;
    view->graphics.zoomLocked = false;
    view->graphics.xpos = 0;
    view->graphics.ypos = 0;
    view->graphics.pressed = false;
    view->graphics.zoomLevel = 1.0f;
    view->graphics.zoomCenter = vec2f(0.5, 0.5);
    *pdfView = view;

    LOG_PDF_OPEN(view->document);
    return true;
}

int PdfView_GetNumPages(PdfViewState *pdfView){
    if(!pdfView->document)
        return -1;

    return pdfView->document->pages();
}

void PdfView_OpenPage(PdfViewState *pdfView, int pageIndex){
    const char *ptr = PdfView_GetImagePage(pdfView, pageIndex, pdfView->graphics.width,
                                           pdfView->graphics.height);
    if(ptr){
        pdfView->pageIndex = pageIndex;
        pdfView->changed = true;
    }else{
        printf("Failed to open page\n");
    }
}

void PdfView_NextPage(PdfViewState *pdfView){
    int totalPages = PdfView_GetNumPages(pdfView);
    if(totalPages < 0){
        printf("Failed to get pages\n");
        return;
    }

    if(totalPages > pdfView->pageIndex+1){
        pdfView->pageIndex += 1;
        (void)PdfView_GetImagePage(pdfView, pdfView->pageIndex,
                                   pdfView->graphics.width, pdfView->graphics.height);
        pdfView->changed = true;
    }
}

void PdfView_PreviousPage(PdfViewState *pdfView){
    int totalPages = PdfView_GetNumPages(pdfView);
    if(totalPages < 0){
        printf("Failed to get pages\n");
        return;
    }

    if(pdfView->pageIndex > 0){
        pdfView->pageIndex -= 1;
        (void)PdfView_GetImagePage(pdfView, pdfView->pageIndex,
                                   pdfView->graphics.width, pdfView->graphics.height);
        pdfView->changed = true;
    }
}

const char *PdfView_GetCurrentPage(PdfViewState *pdfView, int &width, int &height){
    width  = pdfView->graphics.width;
    height = pdfView->graphics.height;
    return (const char *)pdfView->pixels;
}

const char *PdfView_GetImagePage(PdfViewState *pdfView, int pageIndex,
                                 int &width, int &height)
{
    int dpi = 500;
    if(!pdfView->document){
        printf("Document not opened\n");
        return nullptr;
    }

    if(pageIndex >= pdfView->document->pages() || pageIndex < 0){
        printf("Invalid pageIndex {%d} >= {%d}\n", pageIndex, pdfView->document->pages());
        return nullptr;
    }
    poppler::page_renderer renderer;
    renderer.set_render_hint(poppler::page_renderer::antialiasing, true);
    renderer.set_render_hint(poppler::page_renderer::text_antialiasing, true);

    if(pdfView->page){
        delete pdfView->page;
        pdfView->page = nullptr;
    }

    if(pdfView->pixels){
        delete[] pdfView->pixels;
        pdfView->pixels = nullptr;
    }

    pdfView->page = pdfView->document->create_page(pageIndex);
    if(!pdfView->page){
        printf("Could not create page {%d}\n", pageIndex);
        return nullptr;
    }

    renderer.set_image_format(poppler::image::format_enum::format_argb32);
    pdfView->img = renderer.render_page(pdfView->page, dpi, dpi);
    width = pdfView->img.width();
    height = pdfView->img.height();

    poppler::image::format_enum fmt = pdfView->img.format();
    if(fmt != poppler::image::format_enum::format_argb32){
        printf("Unsupported format?\n");
        exit(0);
    }

    pdfView->pixels = new unsigned char[4 * width * height];
    char *hptr = (char *)pdfView->img.const_data() +
        pdfView->img.bytes_per_row() * (height-1);

    int iterator = 0;
    for(int y = 0; y < height; y++){
        for(int x = 0; x < width; x++){
            const unsigned int pixel = *reinterpret_cast<unsigned int *>(hptr + x * 4);
            unsigned char r = (pixel >> 16) & 0xff;
            unsigned char g = (pixel >> 8) & 0xff;
            unsigned char b = pixel & 0xff;
            pdfView->pixels[4 * iterator + 0] = r;
            pdfView->pixels[4 * iterator + 1] = g;
            pdfView->pixels[4 * iterator + 2] = b;
            pdfView->pixels[4 * iterator + 3] = 255;
            iterator += 1;
        }

        hptr -= pdfView->img.bytes_per_row();
    }

    return (const char *)pdfView->pixels;
}

#endif

PdfViewGraphicState PdfView_GetGraphicsState(PdfViewState *pdfView){
    PdfViewGraphicState state = {
        .zoomCenter = pdfView->graphics.zoomCenter,
        .zoomLevel = pdfView->graphics.zoomLevel,
        .pageIndex = pdfView->pageIndex
    };

    return state;
}

void PdfView_SetGraphicsState(PdfViewState *pdfView, PdfViewGraphicState state){
    pdfView->graphics.zoomCenter = state.zoomCenter;
    pdfView->graphics.zoomLevel = state.zoomLevel;
    pdfView->pageIndex = state.pageIndex;

    PdfView_OpenPage(pdfView, pdfView->pageIndex);
}

bool PdfView_Reload(PdfViewState **pdfView){
    if(!*pdfView)
        return false;

    if(!(*pdfView)->document){
        printf("No document is opened\n");
        return false;
    }

    std::string path = (*pdfView)->docPath;
    if(path.size() < 1){
        printf("Unknown path\n");
        return false;
    }

    int fileType = -1;
    int ret = FileProvider_Load((char *)path.c_str(), path.size(),
                                fileType, nullptr, true);
    if(ret != FILE_LOAD_REQUIRES_VIEWER){
        printf("Failed to load, provider did not gave viewer\n");
        return false;
    }

    uint pdfLen = 0;
    char *pdf = FileProvider_GetTemporary(&pdfLen);
    if(!(pdf && pdfLen > 0)){
        printf("Could not get file contents\n");
        return false;
    }

    return PdfView_OpenDocument(pdfView, pdf, pdfLen, path.c_str(), path.size());
}

void PdfView_EnableZoom(PdfViewState *pdfView){
    pdfView->graphics.zooming = true;
}

void PdfView_UnlockZoom(PdfViewState *pdfView){
    pdfView->graphics.zoomLocked = false;
}

void PdfView_LockZoom(PdfViewState *pdfView){
    pdfView->graphics.zoomLocked = true;
}

void PdfView_DisableZoom(PdfViewState *pdfView){
    pdfView->graphics.zooming = false;
    PdfView_ResetZoom(pdfView);
}

void PdfView_IncreaseZoomLevel(PdfViewState *pdfView){
    pdfView->graphics.zoomLevel += 0.25f;
}

void PdfView_DecreaseZoomLevel(PdfViewState *pdfView){
    pdfView->graphics.zoomLevel = Max(pdfView->graphics.zoomLevel - 0.25f, 1.0f);
}

bool PdfView_IsZoomLocked(PdfViewState *pdfView){
    return pdfView->graphics.zoomLocked;
}

bool PdfView_CanMoveTo(PdfViewState *pdfView, vec2f center){
    vec2f lower(0.f, 0.f);
    vec2f upper(1.f, 1.f);
    Float d = 1.f / pdfView->graphics.zoomLevel;
    vec2f center_proj = center - vec2f(d * 0.5f);
    vec2f lower_proj = center_proj + lower * d;
    vec2f upper_proj = center_proj + upper * d;
    return
        !((lower_proj.x < 0 || lower_proj.x > 1 ||
           lower_proj.y < 0 || lower_proj.y > 1) ||
          (upper_proj.x < 0 || upper_proj.x > 1 ||
           upper_proj.y < 0 || upper_proj.y > 1));
}

void PdfView_MouseMotion(PdfViewState *pdfView, vec2f mouse,
                         int pressed, Geometry *geometry)
{
    vec2f lower = geometry->lower;
    vec2f upper = geometry->upper;
    double tx = (double)(mouse.x - lower.x) / (double)(upper.x - lower.x);
    double ty = (double)(mouse.y - lower.y) / (double)(upper.y - lower.y);

    if(pdfView->graphics.pressed){
        double dx = tx - pdfView->graphics.xpos;
        double dy = ty - pdfView->graphics.ypos;
        vec2f center = pdfView->graphics.zoomCenter - vec2f(dx, dy);
        if(PdfView_CanMoveTo(pdfView, center)){
            pdfView->graphics.zoomCenter = center;
        }
    }

    pdfView->graphics.xpos = tx;
    pdfView->graphics.ypos = ty;
    pdfView->graphics.pressed = pressed;
}

void PdfView_ResetZoom(PdfViewState *pdfView){
    pdfView->graphics.zoomCenter = vec2f(0.5, 0.5);
    pdfView->graphics.zoomLevel = 1.0f;
}

vec2f PdfView_GetZoomCenter(PdfViewState *pdfView){
    return pdfView->graphics.zoomCenter;
}

void PdfView_SetZoomCenter(PdfViewState *pdfView, vec2f center){
    pdfView->graphics.zoomCenter = vec2f(Clamp(center.x, 0.f, 1.f),
                                         Clamp(center.y, 0.f, 1.f));
}

Float PdfView_GetZoomLevel(PdfViewState *pdfView){
    return pdfView->graphics.zoomLevel;
}

#endif

/*

    find_library(POPPLER_GLIB_LIB NAMES poppler-glib)
    find_library(CAIRO_LIB NAMES cairo)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GLIB glib-2.0)
    pkg_check_modules(GIO gio-2.0)
    pkg_check_modules(GOBJECT gobject-2.0)
    pkg_check_modules(CAIRO cairo)
    if(POPPLER_GLIB_LIB AND GLIB_FOUND AND GOBJECT_FOUND AND CAIRO_FOUND AND GIO_FOUND)
        if(CAIRO_LIB)
            add_definitions(-DPOPPLER_GLIB_ENABLED)
            include_directories(${GLIB_INCLUDE_DIRS})
            include_directories(${GIO_INCLUDE_DIRS})
            include_directories(${GOBJECT_INCLUDE_DIRS})
            include_directories(${CAIRO_INCLUDE_DIRS})
            link_directories(${GLIB_LIBRARY_DIRS})
            link_directories(${GIO_LIBRARY_DIRS})
            link_directories(${GOBJECT_LIBRARY_DIRS})
            link_directories(${CAIRO_LIBRARY_DIRS})
            list(APPEND CODY_LIBRARIES ${POPPLER_GLIB_LIB} ${CAIRO_LIB} ${GIO_LIBRARIES})
        else()
            message(STATUS "Cairo not found, disabling PDF view")
        endif()
    else()
        message(STATUS "Poppler not found, disabling PDF view")
    endif()

*/

/*
    find_library(POPPLER_CPP_LIB NAMES poppler-cpp)
    if(POPPLER_CPP_LIB)
        add_definitions(-DPOPPLER_CPP_ENABLED)
        list(APPEND CODY_LIBRARIES ${POPPLER_CPP_LIB})
    else()
        message(STATUS "Poppler not found, disabling PDF view")
    endif()

*/
