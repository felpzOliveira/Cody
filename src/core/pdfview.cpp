#include <pdfview.h>
#include <utilities.h>
#include <unordered_map>
#include <file_provider.h>

//#define LRU_CACHE_DEBUG
#include <lru_cache.h>

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

bool PdfView_CanMoveTo(PdfViewState *, vec2f){ return false; }
PdfViewGraphicState PdfView_GetGraphicsState(PdfViewState *){ return PdfViewGraphicState(); }
void PdfView_SetGraphicsState(PdfViewState *, PdfViewGraphicState){}

bool PdfView_ProjectCoordinate(PdfViewState *, vec2f, Geometry *, vec2f &){ return false; }
void PdfView_MouseMotion(PdfViewState *, vec2f, int, Geometry *){}
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
#include <parallel.h>
#include <poppler/cpp/poppler-document.h>
#include <poppler/cpp/poppler-page.h>
#include <poppler/cpp/poppler-page-renderer.h>

struct PdfViewGraphics{
    int width, height;
    vec2f zoomCenter;
    Float zoomLevel;
    bool zooming;
    bool zoomLocked;
    Float xpos, ypos;
    bool pressed;
};

struct PdfPagePixels{
    unsigned char *pixels;
    int width, height;
};

struct PdfViewState{
    poppler::document *document;
    unsigned char *pixels;
    std::string docPath;
    int pageIndex;
    bool changed;
    PdfViewGraphics graphics;
    LRUCache<int, PdfPagePixels> cache;
};

void lru_cache_clear(PdfPagePixels pagePixel){
    if(pagePixel.pixels)
        delete[] pagePixel.pixels;
}

std::string cache_key_dbg(int val){
    return std::to_string(val);
}

std::string cache_item_dbg(PdfPagePixels page){
    return "[ " + std::to_string(page.width) + " " +
            std::to_string(page.height) + " ]";
}

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

        if((*pdfView)->document){
            delete (*pdfView)->document;
            (*pdfView)->document = nullptr;
        }

        (*pdfView)->cache.clear();
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
    view->cache.init(50, lru_cache_clear);
    view->cache.set_dbg_functions(cache_item_dbg, cache_key_dbg);
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

static
PdfPagePixels PdfView_RenderPage(poppler::document *document, int pageIndex, int dpi){
    char *hptr = nullptr;
    int iterator = 0;
    PdfPagePixels resultPage = {nullptr, 0, 0};
    poppler::page_renderer renderer;
    poppler::image::format_enum fmt;
    poppler::image img;
    renderer.set_render_hint(poppler::page_renderer::antialiasing, true);
    renderer.set_render_hint(poppler::page_renderer::text_antialiasing, true);
    renderer.set_image_format(poppler::image::format_enum::format_argb32);
    poppler::page *page = document->create_page(pageIndex);

    if(!page){
        printf("Could not get page { %d }\n", pageIndex);
        goto __ret;
    }

    img = renderer.render_page(page, dpi, dpi);
    resultPage.width = img.width();
    resultPage.height = img.height();

    fmt = img.format();
    if(fmt != poppler::image::format_enum::format_argb32){
        printf("Unsupported format?\n");
        goto __ret;
    }

    resultPage.pixels = new unsigned char[4 * resultPage.width * resultPage.height];
    if(!resultPage.pixels){
        printf("Could not allocate memory for pixels\n");
        goto __ret;
    }

    hptr = (char *)img.const_data() + img.bytes_per_row() * (resultPage.height-1);

    for(int y = 0; y < resultPage.height; y++){
        for(int x = 0; x < resultPage.width; x++){
            const unsigned int pixel = *reinterpret_cast<unsigned int *>(hptr + x * 4);
            unsigned char r = (pixel >> 16) & 0xff;
            unsigned char g = (pixel >> 8) & 0xff;
            unsigned char b = pixel & 0xff;
            resultPage.pixels[4 * iterator + 0] = r;
            resultPage.pixels[4 * iterator + 1] = g;
            resultPage.pixels[4 * iterator + 2] = b;
            resultPage.pixels[4 * iterator + 3] = 255;
            iterator += 1;
        }

        hptr -= img.bytes_per_row();
    }
__ret:
    if(page)
        delete page;
    return resultPage;
}

const char *PdfView_GetImagePage(PdfViewState *pdfView, int pageIndex,
                                 int &width, int &height)
{
    int dpi = 500;
    int pageCount = 0;

    if(!pdfView->document){
        printf("Document not opened\n");
        return nullptr;
    }

    pageCount = pdfView->document->pages();
    if(pageIndex >= pageCount || pageIndex < 0){
        printf("Invalid pageIndex {%d} >= {%d}\n", pageIndex, pdfView->document->pages());
        return nullptr;
    }

    std::optional<PdfPagePixels> pagePixels = pdfView->cache.get(pageIndex);
    if(pagePixels.has_value()){
        PdfPagePixels value = pagePixels.value();
        width = value.width;
        height = value.height;
        pdfView->pixels = value.pixels;
    }else{
        PdfPagePixels result = PdfView_RenderPage(pdfView->document, pageIndex, dpi);
        if(result.pixels){
            width = result.width;
            height = result.height;
            pdfView->pixels = result.pixels;

            pdfView->cache.put(pageIndex, result);
        }
    }

    DispatchExecution([&](HostDispatcher *dispatcher){
        int rangePages[8] = {pageIndex-1, pageIndex+1, pageIndex-2,
                             pageIndex+2, pageIndex-3, pageIndex+3,
                             pageIndex-4, pageIndex+4};
        int local_dpi = dpi;
        int local_count = pageCount;
        poppler::document *local_doc = pdfView->document;
        LRUCache<int, PdfPagePixels> *local_cache = &pdfView->cache;

        dispatcher->DispatchHost();

        for(int i = 0; i < 8; i++){
            int pageNum = rangePages[i];
            if(pageNum >= local_count || pageNum < 0)
                continue;

            std::optional<PdfPagePixels> pPage = local_cache->get(pageNum);
            if(!pPage.has_value()){
                PdfPagePixels pRes = PdfView_RenderPage(local_doc, pageNum, local_dpi);
                if(pRes.pixels){
                    local_cache->put(pageNum, pRes);
                }
            }
        }
    });

    return (const char *)pdfView->pixels;
}

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

    (*pdfView)->cache.clear();

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

