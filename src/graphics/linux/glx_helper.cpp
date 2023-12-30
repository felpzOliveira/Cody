#include <glx_helper.h>
#include <types.h>
#include <dlfcn.h>
#include <utilities.h>
#include <limits.h>

#define LOAD_SYM(x, type) do{ x = (type)dlsym(glxHelper.handle, #x); AssertA(x != NULL, "Failed to load symbol"); }while(0);

#define LOAD_SYM_GLX(x, type) do { x = (type)findAddressGLX(#x); AssertA(x != NULL, "Failed to load GLX symbol"); }while(0);

#define SetAttrib(a, v){\
AssertA(((size_t) index + 1) < sizeof(attribs) / sizeof(attribs[0]), "Invalid attrib set"); \
attribs[index++] = a; \
attribs[index++] = v; \
}

extern LibHelperX11 x11Helper;

LibHelperGLX glxHelper;

int GetGLXFBConfigAttrib(GLXFBConfig fbconfig, int attrib, LibHelperX11 *x11){
    int value = 0;
    glXGetFBConfigAttrib(x11->display, fbconfig, attrib, &value);
    return value;
}

void TerminateGLX(){
    dlclose(glxHelper.handle);
}

void MakeContextCurrentGLX(void *vWin){
    WindowX11 *window = (WindowX11 *)vWin;
    if(window){
        if(!glXMakeCurrent(x11Helper.display, window->glx.window,
                           window->glx.handle))
        {
            AssertA(0, "Failed glXMakeCurrent");
        }
    }else{
        if(!glXMakeCurrent(x11Helper.display, None, NULL)){
            AssertA(0, "Failed empty glXMakeCurrent");
        }
    }
}

void SwapBufferGLX(void *vWin){
    WindowX11 *window = (WindowX11 *)vWin;
    if(window){
        glXSwapBuffers(x11Helper.display, window->glx.window);
    }
}

void DestroyContextGLX(void *vWin){
    WindowX11 *window = (WindowX11 *)vWin;
    if(window){
        if(window->glx.window){
            glXDestroyWindow(x11Helper.display, window->glx.window);
            window->glx.window = None;
        }

        if(window->glx.handle){
            glXDestroyContext(x11Helper.display, window->glx.handle);
            window->glx.handle = NULL;
        }
    }
}

void *findAddressGLX(const char *name){
    if(glXGetProcAddress){
        return (void *)glXGetProcAddress((const GLubyte *)name);
    }else if(glXGetProcAddressARB){
        return (void *)glXGetProcAddressARB((const GLubyte *)name);
    }else{
        return dlsym(glxHelper.handle, name);
    }
}

int SupportedExtGLX(const char *name, LibHelperX11 *x11){
    const char *extensions = glXQueryExtensionsString(x11->display, x11->screen);
    if(extensions){
        if(ExtensionStringContains(name, extensions)){
            return 1;
        }
    }
    return 0;
}

Framebuffer *ChooseClosest(Framebuffer *desired, Framebuffer *alternatives, uint count){
    uint i = 0;
    uint missing = 0, leastMissing = UINT_MAX;
    uint colorDiff = 0, leastColorDiff = UINT_MAX;
    uint extraDiff = 0, leastExtraDiff = UINT_MAX;
    Framebuffer *current = NULL;
    Framebuffer *closest = NULL;

    for(i = 0; i < count; i++){
        current = &alternatives[i];
        if(desired->stereo > 0 && current->stereo == 0) continue;
        if(desired->doublebuffer != current->doublebuffer) continue;

        // Things that are _missing_ from desired configuration
        missing = 0;
        if(desired->alphaBits > 0 && current->alphaBits == 0) missing++;
        if(desired->depthBits > 0 && current->depthBits == 0) missing++;
        if(desired->stencilBits > 0 && current->stencilBits == 0) missing++;
        if(desired->auxBuffers > 0 && current->auxBuffers < desired->auxBuffers)
            missing += desired->auxBuffers - current->auxBuffers;
        if(desired->samples > 0 && current->samples == 0) missing++;
        if(desired->transparent != current->transparent) missing++;

        // Things that are _different_ in color channels
        colorDiff = 0;
        if(desired->redBits != -1)
            colorDiff += ((desired->redBits - current->redBits) *
                          (desired->redBits - current->redBits));

        if(desired->greenBits != -1)
            colorDiff += ((desired->greenBits - current->greenBits) *
                          (desired->greenBits - current->greenBits));

        if(desired->blueBits != -1)
            colorDiff += ((desired->blueBits - current->blueBits) *
                          (desired->blueBits - current->blueBits));

        extraDiff = 0;
        if(desired->alphaBits != -1)
            extraDiff += ((desired->alphaBits - current->alphaBits) *
                          (desired->alphaBits - current->alphaBits));

        if(desired->depthBits != -1)
            extraDiff += ((desired->depthBits - current->depthBits) *
                          (desired->depthBits - current->depthBits));

        if(desired->stencilBits != -1)
            extraDiff += ((desired->stencilBits - current->stencilBits) *
                          (desired->stencilBits - current->stencilBits));

        if(desired->accumRedBits != -1)
            extraDiff += ((desired->accumRedBits - current->accumRedBits) *
                          (desired->accumRedBits - current->accumRedBits));

        if(desired->accumGreenBits != -1)
            extraDiff += ((desired->accumGreenBits - current->accumGreenBits) *
                          (desired->accumGreenBits - current->accumGreenBits));

        if(desired->accumBlueBits != -1)
            extraDiff += ((desired->accumBlueBits - current->accumBlueBits) *
                          (desired->accumBlueBits - current->accumBlueBits));

        if(desired->accumAlphaBits != -1)
            extraDiff += ((desired->accumAlphaBits - current->accumAlphaBits) *
                          (desired->accumAlphaBits - current->accumAlphaBits));

        if(desired->samples != -1)
            extraDiff += ((desired->samples - current->samples) *
                          (desired->samples - current->samples));

        if(desired->sRGB && !current->sRGB) extraDiff++;

        if(missing < leastMissing){
            closest = current;
        }else if(missing == leastMissing){
            if((colorDiff < leastColorDiff) ||
               (colorDiff == leastColorDiff && extraDiff < leastExtraDiff)){
                closest = current;
            }
        }

        if(current == closest){
            leastMissing = missing;
            leastColorDiff = colorDiff;
            leastExtraDiff = extraDiff;
        }
    }

    return closest;
}

int ChooseGLXFBConfig(Framebuffer *desired, LibHelperX11 *x11,
                      GLXFBConfig *result)
{
    GLXFBConfig *nativeConfigs;
    Framebuffer *usableFbs;
    Framebuffer *closest = NULL;
    int nativeCount, usableCount;

    nativeConfigs = glXGetFBConfigs(x11->display, x11->screen, &nativeCount);
    AssertA(nativeConfigs != NULL && nativeCount > 0, "Failed glXGetFBConfigs");

    usableFbs = (Framebuffer *)calloc(nativeCount, sizeof(Framebuffer));
    usableCount = 0;

    for(int i = 0; i < nativeCount; i++){
        const GLXFBConfig n = nativeConfigs[i];
        Framebuffer *u = &usableFbs[usableCount];

        if(!(GetGLXFBConfigAttrib(n, GLX_RENDER_TYPE, x11) & GLX_RGBA_BIT)) continue;
        if(!(GetGLXFBConfigAttrib(n, GLX_DRAWABLE_TYPE, x11) & GLX_WINDOW_BIT)) continue;

        u->redBits = GetGLXFBConfigAttrib(n, GLX_RED_SIZE, x11);
        u->greenBits = GetGLXFBConfigAttrib(n, GLX_GREEN_SIZE, x11);
        u->blueBits = GetGLXFBConfigAttrib(n, GLX_BLUE_SIZE, x11);

        u->alphaBits = GetGLXFBConfigAttrib(n, GLX_ALPHA_SIZE, x11);
        u->depthBits = GetGLXFBConfigAttrib(n, GLX_DEPTH_SIZE, x11);
        u->stencilBits = GetGLXFBConfigAttrib(n, GLX_STENCIL_SIZE, x11);

        u->accumRedBits = GetGLXFBConfigAttrib(n, GLX_ACCUM_RED_SIZE, x11);
        u->accumGreenBits = GetGLXFBConfigAttrib(n, GLX_ACCUM_GREEN_SIZE, x11);
        u->accumBlueBits = GetGLXFBConfigAttrib(n, GLX_ACCUM_BLUE_SIZE, x11);
        u->accumAlphaBits = GetGLXFBConfigAttrib(n, GLX_ACCUM_ALPHA_SIZE, x11);
        u->auxBuffers = GetGLXFBConfigAttrib(n, GLX_AUX_BUFFERS, x11);

        if(GetGLXFBConfigAttrib(n, GLX_STEREO, x11))
            u->stereo = 1;
        if(GetGLXFBConfigAttrib(n, GLX_DOUBLEBUFFER, x11))
            u->doublebuffer = 1;

        if(glxHelper.ARB_multisample)
            u->samples = GetGLXFBConfigAttrib(n, GLX_SAMPLES, x11);

        if (glxHelper.ARB_framebuffer_sRGB || glxHelper.EXT_framebuffer_sRGB)
            u->sRGB = GetGLXFBConfigAttrib(n, GLX_FRAMEBUFFER_SRGB_CAPABLE_ARB, x11);

        u->handle = (uintptr_t) n;
        usableCount++;
    }

    closest = ChooseClosest(desired, usableFbs, usableCount);
    AssertA(closest != NULL, "Failed to find a Framebuffer configuration");

    *result = (GLXFBConfig) closest->handle;
    XFree(nativeConfigs);
    free(usableFbs);
    return 1;
}

void SwapIntervalGLX(void *vWin, int interval){
    WindowX11 *window = (WindowX11 *)vWin;
    if(glxHelper.EXT_swap_control){
        glXSwapIntervalEXT(x11Helper.display, window->glx.window,
                           interval);
    }else if(glxHelper.MESA_swap_control){
        glXSwapIntervalMESA(interval);
    }else if(glxHelper.SGI_swap_control){
        if(interval > 0){
            glxHelper.SwapIntervalSGI(interval);
        }
    }
}

int CreateContextGLX(ContextGL *context, Framebuffer *desired, LibHelperX11 *x11){
    GLXFBConfig native = NULL;
    WindowX11 *window = (WindowX11 *)context->window;
    int attribs[40];
    GLXContext share = NULL;

    if(context->share){
        WindowX11 *shared = (WindowX11 *)context->share;
        share = shared->glx.handle;
    }

    (void)ChooseGLXFBConfig(desired, x11, &native);

    if(context->forward){
        AssertA(glxHelper.ARB_create_context, "Cannot create forward compat context");
    }

    if(context->profile){
        AssertA(glxHelper.ARB_create_context && glxHelper.ARB_create_context_profile,
                "Profile is not supported");
    }

    if(glxHelper.ARB_create_context){
        int index = 0, mask = 0, flags = 0;
        if(context->forward){
            flags |= GLX_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB;
        }

        if(context->profile == OPENGL_CORE_PROFILE){
            mask |= GLX_CONTEXT_CORE_PROFILE_BIT_ARB;
        }else if(context->profile == OPENGL_COMPAT_PROFILE){
            mask |= GLX_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB;
        }

        if(context->major != 1 || context->minor != 0){
            SetAttrib(GLX_CONTEXT_MAJOR_VERSION_ARB, context->major);
            SetAttrib(GLX_CONTEXT_MINOR_VERSION_ARB, context->minor);
        }

        if(mask){
            SetAttrib(GLX_CONTEXT_PROFILE_MASK_ARB, mask);
        }

        if(flags){
            SetAttrib(GLX_CONTEXT_FLAGS_ARB, flags);
        }

        SetAttrib(None, None);
        window->glx.handle = glXCreateContextAttribsARB(x11->display, native, share,
                                                        1, attribs);

        AssertA(window->glx.handle, "Failed to create handle with glXCreateContextAttribsARB");

        window->glx.window = glXCreateWindow(x11->display, native, window->handle, NULL);
        AssertA(window->glx.window, "Failed glXCreateWindow");

        window->glx.makeCurrent = MakeContextCurrentGLX;
        window->glx.swapBuffers = SwapBufferGLX;
        window->glx.destroy = DestroyContextGLX;
        window->glx.getProcAddress = NULL; //TODO
        window->glx.swapInterval = SwapIntervalGLX;
    }else{
        AssertA(0, "Unsupported context creation");
    }

    return 1;
}

int ChooseVisualGLX(Framebuffer *desired, LibHelperX11 *x11, Visual **visual, int *depth){
    XVisualInfo *result;
    GLXFBConfig fbConfig;

    (void)ChooseGLXFBConfig(desired, x11, &fbConfig);

    result = glXGetVisualFromFBConfig(x11->display, fbConfig);
    AssertA(result != NULL, "Failed glXGetVisualFromFBConfig");

    *visual = result->visual;
    *depth = result->depth;

    XFree(result);
    return 1;
}

//TODO: Inspect all libraries and check all versions?
int InitGLX(LibHelperX11 *x11){
    const char *glxNames[] = {
        "libGL-1.so",
        "libGL.so.1",
        "libGL.so",
        NULL
    };

    if(glxHelper.handle){
        // already inited
        return 1;
    }

    for(int i = 0; glxNames[i]; i++){
        glxHelper.handle = dlopen(glxNames[i], RTLD_LAZY | RTLD_LOCAL);
        if(glxHelper.handle != NULL){
            break;
        }
    }

    AssertA(glxHelper.handle != NULL, "Failed to load GLX");
    if(glxHelper.handle == NULL){
        printf("NO GL library loaded\n");
        exit(0);
    }
    LOAD_SYM(glXGetFBConfigs, PFNGLXGETFBCONFIGSPROC);
    LOAD_SYM(glXGetFBConfigAttrib, PFNGLXGETFBCONFIGATTRIBPROC);
    LOAD_SYM(glXGetClientString, PFNGLXGETCLIENTSTRINGPROC);
    LOAD_SYM(glXQueryExtension, PFNGLXQUERYEXTENSIONPROC);
    LOAD_SYM(glXQueryVersion, PFNGLXQUERYVERSIONPROC);
    LOAD_SYM(glXDestroyContext, PFNGLXDESTROYCONTEXTPROC);
    LOAD_SYM(glXMakeCurrent, PFNGLXMAKECURRENTPROC);
    LOAD_SYM(glXSwapBuffers, PFNGLXSWAPBUFFERSPROC);
    LOAD_SYM(glXQueryExtensionsString, PFNGLXQUERYEXTENSIONSSTRINGPROC);
    LOAD_SYM(glXCreateNewContext, PFNGLXCREATENEWCONTEXTPROC);
    LOAD_SYM(glXGetVisualFromFBConfig, PFNGLXGETVISUALFROMFBCONFIGPROC);
    LOAD_SYM(glXCreateWindow, PFNGLXCREATEWINDOWPROC);
    LOAD_SYM(glXDestroyWindow, PFNGLXDESTROYWINDOWPROC);
    LOAD_SYM(glXGetProcAddress, PFNGLXGETPROCADDRESSPROC);
    LOAD_SYM(glXGetProcAddressARB, PFNGLXGETPROCADDRESSPROC);

    AssertA(glXQueryExtension(x11->display, &glxHelper.errorBase, &glxHelper.eventBase),
            "Failed to query GLX extensions");

    AssertA(glXQueryVersion(x11->display, &glxHelper.major, &glxHelper.minor),
            "Failed to query GLX version");

    //printf("GLX version: %d.%d\n", glxHelper.major, glxHelper.minor);

#if 0
    AssertA(!(glxHelper.major == 1 && glxHelper.minor < 3),
            "GLX version 1.3 is required");
#endif

    if(SupportedExtGLX("GLX_EXT_swap_control", x11)){
        LOAD_SYM_GLX(glXSwapIntervalEXT, PFNGLXSWAPINTERVALEXTPROC);
        if(glXSwapIntervalEXT) glxHelper.EXT_swap_control = 1;
    }

    if(SupportedExtGLX("GLX_SGI_swap_control", x11)){
        LOAD_SYM_GLX(glXSwapIntervalSGI, PFNGLXSWAPINTERVALSGIPROC);
        if(glXSwapIntervalSGI) glxHelper.SGI_swap_control = 1;
    }

    if(SupportedExtGLX("GLX_MESA_swap_control", x11)){
        LOAD_SYM_GLX(glXSwapIntervalMESA, PFNGLXSWAPINTERVALMESAPROC);
        if(glXSwapIntervalMESA) glxHelper.MESA_swap_control = 1;
    }

    if(SupportedExtGLX("GLX_ARB_multisample", x11)){
        glxHelper.ARB_multisample = 1;
    }

    if(SupportedExtGLX("GLX_ARB_framebuffer_sRGB", x11)){
        glxHelper.ARB_framebuffer_sRGB = 1;
    }

    if(SupportedExtGLX("GLX_EXT_framebuffer_sRGB", x11)){
        glxHelper.EXT_framebuffer_sRGB = 1;
    }

    if(SupportedExtGLX("GLX_ARB_create_context", x11)){
        LOAD_SYM_GLX(glXCreateContextAttribsARB, PFNGLXCREATECONTEXTATTRIBSARBPROC);
        if(glXCreateContextAttribsARB) glxHelper.ARB_create_context = 1;
    }

    if(SupportedExtGLX("GLX_ARB_create_context_robustness", x11)){
        glxHelper.ARB_create_context_robustness = 1;
    }

    if(SupportedExtGLX("GLX_ARB_create_context_profile", x11)){
        glxHelper.ARB_create_context_profile = 1;
    }

    if(SupportedExtGLX("GLX_EXT_create_context_es2_profile", x11)){
        glxHelper.EXT_create_context_es2_profile = 1;
    }

    if(SupportedExtGLX("GLX_ARB_create_context_no_error", x11)){
        glxHelper.ARB_create_context_no_error = 1;
    }

    if(SupportedExtGLX("GLX_ARB_context_flush_control", x11)){
        glxHelper.ARB_context_flush_control = 1;
    }

    return 1;
}
