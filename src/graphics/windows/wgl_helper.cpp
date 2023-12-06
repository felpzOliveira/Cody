#include <glad/glad.h>
#include <wgl_helper.h>
#include <win_display.h>
#include <Windows.h>
#include <stdio.h>
#include <assert.h>

#if 0
    #define STARTED() printf(" > Calling %s\n", __FUNCTION__)
    #define FINISHED() printf(" > Finished %s\n", __FUNCTION__)
#else
    #define STARTED()
    #define FINISHED()
#endif
#define ERROR_MSG(...) do{printf(" * ERROR: "); printf(__VA_ARGS__); printf("\n"); }while(0)

extern LibHelperWin32 win32Helper;
wglLibrary wglHelper;

static int findPixelFormatAttribValue(const int* attribs,
    int attribCount,
    const int* values,
    int attrib)
{
    int i;

    for (i = 0; i < attribCount; i++)
    {
        if (attribs[i] == attrib)
            return values[i];
    }

    ERROR_MSG("WGL: Unknown pixel format attribute requested");
    return 0;
}

#define addAttrib(a) \
{ \
    assert((size_t) attribCount < sizeof(attribs) / sizeof(attribs[0])); \
    attribs[attribCount++] = a; \
}
#define findAttribValue(a) \
    findPixelFormatAttribValue(attribs, attribCount, values, a)

#define setAttrib(a, v) \
{ \
    assert(((size_t) index + 1) < sizeof(attribs) / sizeof(attribs[0])); \
    attribs[index++] = a; \
    attribs[index++] = v; \
}

BOOL IsWindowsVersionOrGreaterWin32(WORD major, WORD minor, WORD sp);

bool StringInExtensionString(const char* string, const char* extensions){
    const char* start = extensions;

    for (;;)
    {
        const char* where;
        const char* terminator;

        where = strstr(start, string);
        if (!where)
            return false;

        terminator = where + strlen(string);
        if (where == start || *(where - 1) == ' ')
        {
            if (*terminator == ' ' || *terminator == '\0')
                break;
        }

        start = terminator;
    }

    return true;
}

static int extensionSupportedWGL(const char* extension){
    const char* extensions = NULL;

    if (wglHelper.GetExtensionsStringARB)
        extensions = wglGetExtensionsStringARB(wglGetCurrentDC());
    else if (wglHelper.GetExtensionsStringEXT)
        extensions = wglGetExtensionsStringEXT();

    if (!extensions)
        return false;

    return StringInExtensionString(extension, extensions);
}

bool InitWGL(){
    STARTED();
    PIXELFORMATDESCRIPTOR pfd;
    HGLRC prc, rc;
    HDC pdc, dc;

    if (wglHelper.instance)
        return true;

    wglHelper.instance = LoadLibraryA("opengl32.dll");
    if (!wglHelper.instance)
    {
        ERROR_MSG("WGL: Failed to load opengl32.dll");
        return false;
    }

    wglHelper.CreateContext = (PFN_wglCreateContext)
        GetProcAddress(wglHelper.instance, "wglCreateContext");
    wglHelper.DeleteContext = (PFN_wglDeleteContext)
        GetProcAddress(wglHelper.instance, "wglDeleteContext");
    wglHelper.GetProcAddress = (PFN_wglGetProcAddress)
        GetProcAddress(wglHelper.instance, "wglGetProcAddress");
    wglHelper.GetCurrentDC = (PFN_wglGetCurrentDC)
        GetProcAddress(wglHelper.instance, "wglGetCurrentDC");
    wglHelper.GetCurrentContext = (PFN_wglGetCurrentContext)
        GetProcAddress(wglHelper.instance, "wglGetCurrentContext");
    wglHelper.MakeCurrent = (PFN_wglMakeCurrent)
        GetProcAddress(wglHelper.instance, "wglMakeCurrent");
    wglHelper.ShareLists = (PFN_wglShareLists)
        GetProcAddress(wglHelper.instance, "wglShareLists");

    // NOTE: A dummy context has to be created for opengl32.dll to load the
    //       OpenGL ICD, from which we can then query WGL extensions
    // NOTE: This code will accept the Microsoft GDI ICD; accelerated context
    //       creation failure occurs during manual pixel format enumeration

    dc = GetDC(win32Helper.helperWindowHandle);

    ZeroMemory(&pfd, sizeof(pfd));
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 24;

    if (!SetPixelFormat(dc, ChoosePixelFormat(dc, &pfd), &pfd))
    {
        ERROR_MSG("WGL: Failed to set pixel format for dummy context");
        return false;
    }

    rc = wglCreateContext(dc);
    if (!rc)
    {
        ERROR_MSG("WGL: Failed to create dummy context");
        return false;
    }

    pdc = wglGetCurrentDC();
    prc = wglGetCurrentContext();

    if (!wglMakeCurrent(dc, rc))
    {
        ERROR_MSG("WGL: Failed to make dummy context current");
        wglMakeCurrent(pdc, prc);
        wglDeleteContext(rc);
        return false;
    }

    // NOTE: Functions must be loaded first as they're needed to retrieve the
    //       extension string that tells us whether the functions are supported
    wglHelper.GetExtensionsStringEXT = (PFNWGLGETEXTENSIONSSTRINGEXTPROC)
        wglGetProcAddress("wglGetExtensionsStringEXT");
    wglHelper.GetExtensionsStringARB = (PFNWGLGETEXTENSIONSSTRINGARBPROC)
        wglGetProcAddress("wglGetExtensionsStringARB");
    wglHelper.CreateContextAttribsARB = (PFNWGLCREATECONTEXTATTRIBSARBPROC)
        wglGetProcAddress("wglCreateContextAttribsARB");
    wglHelper.SwapIntervalEXT = (PFNWGLSWAPINTERVALEXTPROC)
        wglGetProcAddress("wglSwapIntervalEXT");
    wglHelper.GetPixelFormatAttribivARB = (PFNWGLGETPIXELFORMATATTRIBIVARBPROC)
        wglGetProcAddress("wglGetPixelFormatAttribivARB");

    // NOTE: WGL_ARB_extensions_string and WGL_EXT_extensions_string are not
    //       checked below as we are already using them
    wglHelper.ARB_multisample =
        extensionSupportedWGL("WGL_ARB_multisample");
    wglHelper.ARB_framebuffer_sRGB =
        extensionSupportedWGL("WGL_ARB_framebuffer_sRGB");
    wglHelper.EXT_framebuffer_sRGB =
        extensionSupportedWGL("WGL_EXT_framebuffer_sRGB");
    wglHelper.ARB_create_context =
        extensionSupportedWGL("WGL_ARB_create_context");
    wglHelper.ARB_create_context_profile =
        extensionSupportedWGL("WGL_ARB_create_context_profile");
    wglHelper.EXT_create_context_es2_profile =
        extensionSupportedWGL("WGL_EXT_create_context_es2_profile");
    wglHelper.ARB_create_context_robustness =
        extensionSupportedWGL("WGL_ARB_create_context_robustness");
    wglHelper.ARB_create_context_no_error =
        extensionSupportedWGL("WGL_ARB_create_context_no_error");
    wglHelper.EXT_swap_control =
        extensionSupportedWGL("WGL_EXT_swap_control");
    wglHelper.EXT_colorspace =
        extensionSupportedWGL("WGL_EXT_colorspace");
    wglHelper.ARB_pixel_format =
        extensionSupportedWGL("WGL_ARB_pixel_format");
    wglHelper.ARB_context_flush_control =
        extensionSupportedWGL("WGL_ARB_context_flush_control");

    wglMakeCurrent(pdc, prc);
    wglDeleteContext(rc);

    FINISHED();
    return true;
}

const Framebuffer* ChooseFBConfig(const Framebuffer* desired,
                                       const Framebuffer* alternatives, unsigned int count)
{
    unsigned int i;
    unsigned int missing, leastMissing = UINT_MAX;
    unsigned int colorDiff, leastColorDiff = UINT_MAX;
    unsigned int extraDiff, leastExtraDiff = UINT_MAX;
    const Framebuffer* current;
    const Framebuffer* closest = NULL;

    for (i = 0; i < count; i++)
    {
        current = alternatives + i;

        if (desired->stereo > 0 && current->stereo == 0)
        {
            // Stereo is a hard constraint
            continue;
        }

        // Count number of missing buffers
        {
            missing = 0;

            if (desired->alphaBits > 0 && current->alphaBits == 0)
                missing++;

            if (desired->depthBits > 0 && current->depthBits == 0)
                missing++;

            if (desired->stencilBits > 0 && current->stencilBits == 0)
                missing++;

            if (desired->auxBuffers > 0 &&
                current->auxBuffers < desired->auxBuffers)
            {
                missing += desired->auxBuffers - current->auxBuffers;
            }

            if (desired->samples > 0 && current->samples == 0)
            {
                // Technically, several multisampling buffers could be
                // involved, but that's a lower level implementation detail and
                // not important to us here, so we count them as one
                missing++;
            }

            if (desired->transparent != current->transparent)
                missing++;
        }

        // These polynomials make many small channel size differences matter
        // less than one large channel size difference

        // Calculate color channel size difference value
        {
            colorDiff = 0;

            if (desired->redBits != 0)
            {
                colorDiff += (desired->redBits - current->redBits) *
                    (desired->redBits - current->redBits);
            }

            if (desired->greenBits != 0)
            {
                colorDiff += (desired->greenBits - current->greenBits) *
                    (desired->greenBits - current->greenBits);
            }

            if (desired->blueBits != 0)
            {
                colorDiff += (desired->blueBits - current->blueBits) *
                    (desired->blueBits - current->blueBits);
            }
        }

        // Calculate non-color channel size difference value
        {
            extraDiff = 0;

            if (desired->alphaBits != 0)
            {
                extraDiff += (desired->alphaBits - current->alphaBits) *
                    (desired->alphaBits - current->alphaBits);
            }

            if (desired->depthBits != 0)
            {
                extraDiff += (desired->depthBits - current->depthBits) *
                    (desired->depthBits - current->depthBits);
            }

            if (desired->stencilBits != 0)
            {
                extraDiff += (desired->stencilBits - current->stencilBits) *
                    (desired->stencilBits - current->stencilBits);
            }

            if (desired->accumRedBits != 0)
            {
                extraDiff += (desired->accumRedBits - current->accumRedBits) *
                    (desired->accumRedBits - current->accumRedBits);
            }

            if (desired->accumGreenBits != 0)
            {
                extraDiff += (desired->accumGreenBits - current->accumGreenBits) *
                    (desired->accumGreenBits - current->accumGreenBits);
            }

            if (desired->accumBlueBits != 0)
            {
                extraDiff += (desired->accumBlueBits - current->accumBlueBits) *
                    (desired->accumBlueBits - current->accumBlueBits);
            }

            if (desired->accumAlphaBits != 0)
            {
                extraDiff += (desired->accumAlphaBits - current->accumAlphaBits) *
                    (desired->accumAlphaBits - current->accumAlphaBits);
            }

            if (desired->samples != 0)
            {
                extraDiff += (desired->samples - current->samples) *
                    (desired->samples - current->samples);
            }

            if (desired->sRGB && !current->sRGB)
                extraDiff++;
        }

        // Figure out if the current one is better than the best one found so far
        // Least number of missing buffers is the most important heuristic,
        // then color buffer size match and lastly size match for other buffers

        if (missing < leastMissing)
            closest = current;
        else if (missing == leastMissing)
        {
            if ((colorDiff < leastColorDiff) ||
                (colorDiff == leastColorDiff && extraDiff < leastExtraDiff))
            {
                closest = current;
            }
        }

        if (current == closest)
        {
            leastMissing = missing;
            leastColorDiff = colorDiff;
            leastExtraDiff = extraDiff;
        }
    }

    return closest;
}

static int choosePixelFormat(WindowWin32* window,
    const ContextGL* ctxconfig,
    const Framebuffer* fbconfig)
{
    Framebuffer* usableConfigs;
    const Framebuffer* closest;
    int i, pixelFormat, nativeCount, usableCount = 0, attribCount = 0;
    int attribs[40];
    int values[sizeof(attribs) / sizeof(attribs[0])];

    if (wglHelper.ARB_pixel_format)
    {
        const int attrib = WGL_NUMBER_PIXEL_FORMATS_ARB;

        if (!wglGetPixelFormatAttribivARB(window->wgl.dc, 1, 0, 1, &attrib, &nativeCount))
        {
            ERROR_MSG("WGL: Failed to retrieve pixel format attribute");
            return 0;
        }

        addAttrib(WGL_SUPPORT_OPENGL_ARB);
        addAttrib(WGL_DRAW_TO_WINDOW_ARB);
        addAttrib(WGL_PIXEL_TYPE_ARB);
        addAttrib(WGL_ACCELERATION_ARB);
        addAttrib(WGL_RED_BITS_ARB);
        addAttrib(WGL_RED_SHIFT_ARB);
        addAttrib(WGL_GREEN_BITS_ARB);
        addAttrib(WGL_GREEN_SHIFT_ARB);
        addAttrib(WGL_BLUE_BITS_ARB);
        addAttrib(WGL_BLUE_SHIFT_ARB);
        addAttrib(WGL_ALPHA_BITS_ARB);
        addAttrib(WGL_ALPHA_SHIFT_ARB);
        addAttrib(WGL_DEPTH_BITS_ARB);
        addAttrib(WGL_STENCIL_BITS_ARB);
        addAttrib(WGL_ACCUM_BITS_ARB);
        addAttrib(WGL_ACCUM_RED_BITS_ARB);
        addAttrib(WGL_ACCUM_GREEN_BITS_ARB);
        addAttrib(WGL_ACCUM_BLUE_BITS_ARB);
        addAttrib(WGL_ACCUM_ALPHA_BITS_ARB);
        addAttrib(WGL_AUX_BUFFERS_ARB);
        addAttrib(WGL_STEREO_ARB);
        addAttrib(WGL_DOUBLE_BUFFER_ARB);

        if (wglHelper.ARB_multisample)
            addAttrib(WGL_SAMPLES_ARB);

        if(wglHelper.ARB_framebuffer_sRGB || wglHelper.EXT_framebuffer_sRGB)
            addAttrib(WGL_FRAMEBUFFER_SRGB_CAPABLE_ARB);
    }
    else
    {
        nativeCount = DescribePixelFormat(window->wgl.dc, 1,
                            sizeof(PIXELFORMATDESCRIPTOR), NULL);
    }

    usableConfigs = (Framebuffer*) calloc(nativeCount, sizeof(Framebuffer));

    for (i = 0; i < nativeCount; i++)
    {
        Framebuffer* u = usableConfigs + usableCount;
        pixelFormat = i + 1;

        if (wglHelper.ARB_pixel_format)
        {
            // Get pixel format attributes through "modern" extension

            if (!wglGetPixelFormatAttribivARB(window->wgl.dc,
                pixelFormat, 0,
                attribCount,
                attribs, values))
            {
                ERROR_MSG("WGL: Failed to retrieve pixel format attributes");

                free(usableConfigs);
                return 0;
            }

            if (!findAttribValue(WGL_SUPPORT_OPENGL_ARB) ||
                !findAttribValue(WGL_DRAW_TO_WINDOW_ARB))
            {
                continue;
            }

            if (findAttribValue(WGL_PIXEL_TYPE_ARB) != WGL_TYPE_RGBA_ARB)
                continue;

            if (findAttribValue(WGL_ACCELERATION_ARB) == WGL_NO_ACCELERATION_ARB)
                continue;

            if (findAttribValue(WGL_DOUBLE_BUFFER_ARB) != fbconfig->doublebuffer)
                continue;

            u->redBits = findAttribValue(WGL_RED_BITS_ARB);
            u->greenBits = findAttribValue(WGL_GREEN_BITS_ARB);
            u->blueBits = findAttribValue(WGL_BLUE_BITS_ARB);
            u->alphaBits = findAttribValue(WGL_ALPHA_BITS_ARB);

            u->depthBits = findAttribValue(WGL_DEPTH_BITS_ARB);
            u->stencilBits = findAttribValue(WGL_STENCIL_BITS_ARB);

            u->accumRedBits = findAttribValue(WGL_ACCUM_RED_BITS_ARB);
            u->accumGreenBits = findAttribValue(WGL_ACCUM_GREEN_BITS_ARB);
            u->accumBlueBits = findAttribValue(WGL_ACCUM_BLUE_BITS_ARB);
            u->accumAlphaBits = findAttribValue(WGL_ACCUM_ALPHA_BITS_ARB);

            u->auxBuffers = findAttribValue(WGL_AUX_BUFFERS_ARB);

            if (findAttribValue(WGL_STEREO_ARB))
                u->stereo = true;

            if (wglHelper.ARB_multisample)
                u->samples = findAttribValue(WGL_SAMPLES_ARB);

            if (wglHelper.ARB_framebuffer_sRGB ||
                wglHelper.EXT_framebuffer_sRGB)
            {
                if (findAttribValue(WGL_FRAMEBUFFER_SRGB_CAPABLE_ARB))
                    u->sRGB = true;
            }
        }
        else
        {
            // Get pixel format attributes through legacy PFDs

            PIXELFORMATDESCRIPTOR pfd;

            if (!DescribePixelFormat(window->wgl.dc,
                pixelFormat,
                sizeof(PIXELFORMATDESCRIPTOR),
                &pfd))
            {
                ERROR_MSG("WGL: Failed to describe pixel format");

                free(usableConfigs);
                return 0;
            }

            if (!(pfd.dwFlags & PFD_DRAW_TO_WINDOW) ||
                !(pfd.dwFlags & PFD_SUPPORT_OPENGL))
            {
                continue;
            }

            if (!(pfd.dwFlags & PFD_GENERIC_ACCELERATED) &&
                (pfd.dwFlags & PFD_GENERIC_FORMAT))
            {
                continue;
            }

            if (pfd.iPixelType != PFD_TYPE_RGBA)
                continue;

            if (!!(pfd.dwFlags & PFD_DOUBLEBUFFER) != fbconfig->doublebuffer)
                continue;

            u->redBits = pfd.cRedBits;
            u->greenBits = pfd.cGreenBits;
            u->blueBits = pfd.cBlueBits;
            u->alphaBits = pfd.cAlphaBits;

            u->depthBits = pfd.cDepthBits;
            u->stencilBits = pfd.cStencilBits;

            u->accumRedBits = pfd.cAccumRedBits;
            u->accumGreenBits = pfd.cAccumGreenBits;
            u->accumBlueBits = pfd.cAccumBlueBits;
            u->accumAlphaBits = pfd.cAccumAlphaBits;

            u->auxBuffers = pfd.cAuxBuffers;

            if (pfd.dwFlags & PFD_STEREO)
                u->stereo = true;
        }

        u->handle = pixelFormat;
        usableCount++;
    }

    if (!usableCount)
    {
        ERROR_MSG("WGL: The driver does not appear to support OpenGL");

        free(usableConfigs);
        return 0;
    }

    closest = ChooseFBConfig(fbconfig, usableConfigs, usableCount);
    if (!closest)
    {
        ERROR_MSG("WGL: Failed to find a suitable pixel format");

        free(usableConfigs);
        return 0;
    }

    pixelFormat = (int)closest->handle;
    free(usableConfigs);

    return pixelFormat;
}

static void makeContextCurrentWGL(WindowWin32* window){
    if (window)
    {
        wglMakeCurrent(window->wgl.dc, window->wgl.handle);
    }
    else
    {
        if (!wglMakeCurrent(NULL, NULL))
        {
            ERROR_MSG("WGL: Failed to clear current context");
        }
    }
}

static void swapBuffersWGL(WindowWin32* window){
#if 1
    if (IsWindowsVistaOrGreater())
    {
        // DWM Composition is always enabled on Win8+
        BOOL enabled = IsWindows8OrGreater();

        // HACK: Use DwmFlush when desktop composition is enabled
        if (enabled ||
            (SUCCEEDED(DwmIsCompositionEnabled(&enabled)) && enabled))
        {
            int count = abs(window->wgl.interval);
            while (count--)
                DwmFlush();
        }
    }
#endif
#if 0
    BOOL enabled = TRUE;
    if ((SUCCEEDED(DwmIsCompositionEnabled(&enabled)) && enabled))
    {
        int count = abs(window->wgl.interval);
        while (count--)
            DwmFlush();
    }
#endif
    SwapBuffers(window->wgl.dc);
}

static void swapIntervalWGL(WindowWin32 *window, int interval)
{
    window->wgl.interval = interval;

    if (wglHelper.EXT_swap_control){
        wglSwapIntervalEXT(interval);
    }
}

static glproc getProcAddressWGL(const char* procname)
{
    const glproc proc = (glproc)wglGetProcAddress(procname);
    if (proc)
        return proc;

    return (glproc)GetProcAddress(wglHelper.instance, procname);
}

static void destroyContextWGL(WindowWin32* window)
{
    if (window->wgl.handle)
    {
        wglDeleteContext(window->wgl.handle);
        window->wgl.handle = NULL;
    }
}

void MakeContextWGL(WindowWin32 *window){
    makeContextCurrentWGL(window);
}

void SwapBuffersWGL(WindowWin32* window){
    swapBuffersWGL(window);
}

bool CreateContextWGL(WindowWin32* window, const ContextGL* ctxconfig, const Framebuffer* fbconfig){
    int attribs[40];
    int pixelFormat;
    PIXELFORMATDESCRIPTOR pfd;
    HGLRC share = NULL;

    //if (ctxconfig->share)
      //  share = ctxconfig->share->wgl.handle;

    window->wgl.dc = GetDC(window->handle);
    if (!window->wgl.dc){
        ERROR_MSG("WGL: Failed to retrieve DC for window");
        return false;
    }

    pixelFormat = choosePixelFormat(window, ctxconfig, fbconfig);
    if (!pixelFormat)
        return false;

    if (!DescribePixelFormat(window->wgl.dc,
        pixelFormat, sizeof(pfd), &pfd))
    {
        ERROR_MSG("WGL: Failed to retrieve PFD for selected pixel format");
        return false;
    }

    if (!SetPixelFormat(window->wgl.dc, pixelFormat, &pfd))
    {
        ERROR_MSG("WGL: Failed to set selected pixel format");
        return false;
    }

    if (ctxconfig->forward)
    {
        if (!wglHelper.ARB_create_context)
        {
            ERROR_MSG("WGL: A forward compatible OpenGL context requested but WGL_ARB_create_context is unavailable");
            return false;
        }
    }

    if (ctxconfig->profile)
    {
        if (!wglHelper.ARB_create_context_profile){
            ERROR_MSG("WGL: OpenGL profile requested but WGL_ARB_create_context_profile is unavailable");
            return false;
        }
    }


    if (wglHelper.ARB_create_context)
    {
        int index = 0, mask = 0, flags = 0;
        if (ctxconfig->forward)
            flags |= WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB;

        if (ctxconfig->profile == OPENGL_CORE_PROFILE)
            mask |= WGL_CONTEXT_CORE_PROFILE_BIT_ARB;
        else if (ctxconfig->profile == OPENGL_COMPAT_PROFILE)
            mask |= WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB;


        //if (ctxconfig->debug)
          //  flags |= WGL_CONTEXT_DEBUG_BIT_ARB;
#if 0
        if (ctxconfig->robustness)
        {
            if (wglHelper.ARB_create_context_robustness)
            {
                if (ctxconfig->robustness == GLFW_NO_RESET_NOTIFICATION)
                {
                    setAttrib(WGL_CONTEXT_RESET_NOTIFICATION_STRATEGY_ARB,
                        WGL_NO_RESET_NOTIFICATION_ARB);
                }
                else if (ctxconfig->robustness == GLFW_LOSE_CONTEXT_ON_RESET)
                {
                    setAttrib(WGL_CONTEXT_RESET_NOTIFICATION_STRATEGY_ARB,
                        WGL_LOSE_CONTEXT_ON_RESET_ARB);
                }

                flags |= WGL_CONTEXT_ROBUST_ACCESS_BIT_ARB;
            }
        }

        if (ctxconfig->release)
        {
            if (wglHelper.ARB_context_flush_control)
            {
                if (ctxconfig->release == GLFW_RELEASE_BEHAVIOR_NONE)
                {
                    setAttrib(WGL_CONTEXT_RELEASE_BEHAVIOR_ARB,
                        WGL_CONTEXT_RELEASE_BEHAVIOR_NONE_ARB);
                }
                else if (ctxconfig->release == GLFW_RELEASE_BEHAVIOR_FLUSH)
                {
                    setAttrib(WGL_CONTEXT_RELEASE_BEHAVIOR_ARB,
                        WGL_CONTEXT_RELEASE_BEHAVIOR_FLUSH_ARB);
                }
            }
        }

        if (ctxconfig->noerror)
        {
            if (wglHelper.ARB_create_context_no_error)
                setAttrib(WGL_CONTEXT_OPENGL_NO_ERROR_ARB, true);
        }
#endif
        // NOTE: Only request an explicitly versioned context when necessary, as
        //       explicitly requesting version 1.0 does not always return the
        //       highest version supported by the driver
        if (ctxconfig->major != 1 || ctxconfig->minor != 0)
        {
            setAttrib(WGL_CONTEXT_MAJOR_VERSION_ARB, ctxconfig->major);
            setAttrib(WGL_CONTEXT_MINOR_VERSION_ARB, ctxconfig->minor);
        }

        if (flags)
            setAttrib(WGL_CONTEXT_FLAGS_ARB, flags);

        if (mask)
            setAttrib(WGL_CONTEXT_PROFILE_MASK_ARB, mask);

        setAttrib(0, 0);

        window->wgl.handle =
            wglCreateContextAttribsARB(window->wgl.dc, share, attribs);
        if (!window->wgl.handle)
        {
            const DWORD error = GetLastError();

            if (error == (0xc0070000 | ERROR_INVALID_VERSION_ARB))
            {
                ERROR_MSG("WGL: Driver does not support OpenGL version %i.%i",
                    ctxconfig->major, ctxconfig->minor);
            }
            else if (error == (0xc0070000 | ERROR_INVALID_PROFILE_ARB))
            {
                ERROR_MSG("WGL: Driver does not support the requested OpenGL profile");
            }
            else if (error == (0xc0070000 | ERROR_INCOMPATIBLE_DEVICE_CONTEXTS_ARB))
            {
                ERROR_MSG("WGL: The share context is not compatible with the requested context");
            }
            else
            {
                ERROR_MSG("WGL: Failed to create OpenGL context");
            }

            return false;
        }
    }
    else
    {
        window->wgl.handle = wglCreateContext(window->wgl.dc);
        if (!window->wgl.handle)
        {
            ERROR_MSG("WGL: Failed to create OpenGL context");
            return false;
        }

        if (share)
        {
            if (!wglShareLists(share, window->wgl.handle))
            {
                ERROR_MSG("WGL: Failed to enable sharing with specified OpenGL context");
                return false;
            }
        }
    }

    window->wgl.makeCurrent = makeContextCurrentWGL;
    window->wgl.swapBuffers = swapBuffersWGL;
    window->wgl.swapInterval = swapIntervalWGL;
    window->wgl.extensionSupported = extensionSupportedWGL;
    window->wgl.getProcAddress = getProcAddressWGL;
    window->wgl.destroy = destroyContextWGL;

    return true;
}

void TerminateWGL(){
    if(wglHelper.instance)
        FreeLibrary(wglHelper.instance);
}