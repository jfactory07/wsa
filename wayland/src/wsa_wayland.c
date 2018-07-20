/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2018 Advanced Micro Devices, Inc. All Rights Reserved.
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in all
 *  copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *  SOFTWARE.
 *
 **********************************************************************************************************************/

#include "wsa.h"
#include "wsa_wayland_priv.h"
#include "string.h"
#include "unistd.h"
#include "pthread.h"

// include wayland-drm protocol definition from MESA
#include "wayland-drm-protocol.c"

// ===============================================================
// static variable section

// surface list, all surfaces are stored in this buffer.
static WaylandWsaSurface s_surfaceList[MAX_SURFACE_NUMBER] = {{0}};
static int32 s_surfaceSlot = 0;
// instance list.
static WaylandWsaInstance s_instanceList[MAX_INSTANCE_NUMBER] = {{0}};
static int32 s_instanceSlot = 0;
// mutex for surface and instance manage.
static pthread_mutex_t s_mutex = PTHREAD_MUTEX_INITIALIZER;

static void FrameHandleDone(
    void*  pData,
    struct wl_callback *pCallback,
    uint32 serial)
{
    bool* done = (bool*)pData;
    *done = true;
    wl_callback_destroy(pCallback);
}

// frame callback, to check whether the frame is done.
static const struct wl_callback_listener s_frameListener =
{
    FrameHandleDone,
};

static void BufferHandleRelease(
    void*             pData,
    struct wl_buffer* pBuffer)
{
    bool *busy = (bool*)pData;
    *busy = false;
}

// buffer callback, to check whether the buffer is released by the server.
static const struct wl_buffer_listener s_bufferListener =
{
    BufferHandleRelease,
};

static void DrmHandleDevice(
    void*          pData,
    struct wl_drm* pDrm,
    const char*    pName)
{
}

static void DrmHandleFormat(
    void*          pData,
    struct wl_drm* pDrm,
    uint32         wl_format)
{
}

static void DrmHandleAuthenticated(
    void*          pData,
    struct wl_drm* pDrm)
{
}

static void DrmHandleCapabilities(
    void*          pData,
    struct wl_drm* pDrm,
    uint32         capabilities)
{
    uint32 *cap = (uint32*)pData;
    *cap = capabilities;
}

//wayland drm callback
static const struct wl_drm_listener s_drmListener =
{
    DrmHandleDevice,
    DrmHandleFormat,
    DrmHandleAuthenticated,
    DrmHandleCapabilities,
};

static void RegistryHandleGlobal(
    void*               pData,
    struct wl_registry* pRegistry,
    uint32              name,
    const char*         pInterface,
    uint32              version)
{
    WaylandWsaInstance* ws = (WaylandWsaInstance*)pData;

    if (strcmp(pInterface, "wl_drm") == 0)
    {
        WSA_ASSERT(ws->pDrm == NULL);
        WSA_ASSERT(version >= 2);
        ws->pDrm = (struct wl_drm*)wl_registry_bind(pRegistry, name, &wl_drm_interface, 2);
        if (ws->pDrm)
        {
            wl_drm_add_listener(ws->pDrm, &s_drmListener, &(ws->capabilities));
        }
    }
}

static void RegistryHandleGlobalRemove(
    void*               pData,
    struct wl_registry* pRegistry,
    uint32              name)
{
}

static const struct wl_registry_listener s_registryListener =
{
    RegistryHandleGlobal,
    RegistryHandleGlobalRemove,
};


//====================interface========================================

// Query wsa interface version
static uint32 QueryWaylandWsaVer(void)
{
    return WSA_INTERFACE_VER;
}

// Create a window system agent(WSA) for wayland.
// Wsa handle is returned by hWsa.
static WsaError CreateWaylandWsa(
    int32* pWsa)
{
    WSA_ASSERT(pWsa);
    *pWsa = -1;
    pthread_mutex_lock(&s_mutex);
    int32 last = s_instanceSlot - 1;

    if (last < 0)
    {
        last = MAX_INSTANCE_NUMBER - 1;
    }

    for(; s_instanceSlot != last; s_instanceSlot++)
    {
        if (s_instanceSlot >= MAX_INSTANCE_NUMBER)
        {
            s_instanceSlot = 0;
        }
        if (!(s_instanceList[s_instanceSlot].created))
        {
            s_instanceList[s_instanceSlot].created = true;
            *pWsa = s_instanceSlot;
            s_instanceSlot++;
            pthread_mutex_unlock(&s_mutex);
            return Success;
        }
    }

    pthread_mutex_unlock(&s_mutex);
    return NotEnoughResource;
}

// Initialize a window system agent(WSA) for wayland.
static WsaError InitializeWaylandWsa(
    int32 hWsa,
    void* pDisplay,
    void* pSurface)
{
    WSA_ASSERT((hWsa >= 0) && (hWsa < MAX_INSTANCE_NUMBER));
    WSA_ASSERT(pDisplay && pSurface);

    WsaError ret = Success;
    struct wl_registry *registry = NULL;

    pthread_mutex_lock(&s_mutex);

    s_instanceList[hWsa].pQueue = wl_display_create_queue(pDisplay);
    if (!s_instanceList[hWsa].pQueue)
    {
        ret = NotEnoughResource;
    }

    if (ret == Success)
    {
        s_instanceList[hWsa].pDisplayWrapper = (struct wl_display*)wl_proxy_create_wrapper(pDisplay);
        if (!s_instanceList[hWsa].pDisplayWrapper)
        {
            ret = NotEnoughResource;
        }
    }

    if (ret == Success)
    {
        wl_proxy_set_queue((struct wl_proxy*)(s_instanceList[hWsa].pDisplayWrapper), s_instanceList[hWsa].pQueue);
        registry = wl_display_get_registry(s_instanceList[hWsa].pDisplayWrapper);
        if (!registry)
        {
            ret = UnknownFailure;
        }
    }

    if (ret == Success)
    {
        wl_registry_add_listener(registry, &s_registryListener, s_instanceList + hWsa);
        wl_display_roundtrip_queue(pDisplay, s_instanceList[hWsa].pQueue);
        if (!(s_instanceList[hWsa].pDrm))
        {
            ret = UnknownFailure;
        }
    }

    if (ret == Success)
    {
        s_instanceList[hWsa].pDrmWrapper = (struct wl_drm*)wl_proxy_create_wrapper(s_instanceList[hWsa].pDrm);
        if (!(s_instanceList[hWsa].pDrmWrapper))
        {
            ret = UnknownFailure;
        }
    }

    if (ret == Success)
    {
        wl_display_roundtrip_queue(pDisplay, s_instanceList[hWsa].pQueue);
        if (!(s_instanceList[hWsa].capabilities & WL_DRM_CAPABILITY_PRIME))
        {
            ret = UnknownFailure;
        }
    }

    if (ret == Success)
    {
        s_instanceList[hWsa].pSurfaceWrapper = (struct wl_surface*)wl_proxy_create_wrapper(pSurface);
        if (!(s_instanceList[hWsa].pSurfaceWrapper))
        {
            ret = UnknownFailure;
        }
    }

    if (ret == Success)
    {
        wl_proxy_set_queue((struct wl_proxy*)(s_instanceList[hWsa].pSurfaceWrapper), s_instanceList[hWsa].pQueue);
        s_instanceList[hWsa].pDisplay = pDisplay;
        s_instanceList[hWsa].pSurface = pSurface;
        s_instanceList[hWsa].initialized = true;
    }

    if (registry)
    {
        wl_registry_destroy(registry);
    }

    s_instanceList[hWsa].frameCompleted= true;

    pthread_mutex_unlock(&s_mutex);

    return ret;
}

// Destroy WSA.
static void DestroyWaylandWsa(
    int32 hWsa)
{
    WSA_ASSERT((hWsa >= 0) && (hWsa < MAX_INSTANCE_NUMBER));

    pthread_mutex_lock(&s_mutex);

    if (s_instanceList[hWsa].pDrm)
    {
        wl_drm_destroy(s_instanceList[hWsa].pDrm);
    }

    if (s_instanceList[hWsa].pSurfaceWrapper)
    {
        wl_proxy_wrapper_destroy(s_instanceList[hWsa].pSurfaceWrapper);
    }

    if (s_instanceList[hWsa].pDisplayWrapper)
    {
        wl_proxy_wrapper_destroy(s_instanceList[hWsa].pDisplayWrapper);
    }

    if (s_instanceList[hWsa].pQueue)
    {
        wl_event_queue_destroy(s_instanceList[hWsa].pQueue);
    }

    if (s_instanceList[hWsa].pDrmWrapper)
    {
        wl_proxy_wrapper_destroy(s_instanceList[hWsa].pDrmWrapper);
    }

    if (s_instanceList[hWsa].pFrame)
    {
        wl_callback_destroy(s_instanceList[hWsa].pFrame);
    }

    memset(s_instanceList + hWsa, 0, sizeof(WaylandWsaInstance));

    pthread_mutex_unlock(&s_mutex);
}

// Create a presentable image.
// Image handle is returned by pImage.
static WsaError WaylandCreateImage(
    int32      hWsa,
    int32      fd,
    uint32     width,
    uint32     height,
    WsaFormat  format,
    uint32     stride,
    int32*     pImage)
{
    WSA_ASSERT((hWsa >= 0) && (hWsa < MAX_INSTANCE_NUMBER) && s_instanceList[hWsa].initialized);
    WSA_ASSERT((width != 0) && (height != 0) && (stride != 0) && (fd != -1) && pImage);

    struct wl_buffer* buffer;
    buffer = wl_drm_create_prime_buffer(s_instanceList[hWsa].pDrmWrapper, fd, width, height, format, 0, stride, 0, 0, 0, 0);
    if (!buffer)
    {
        close(fd);
        return UnknownFailure;
    }

    pthread_mutex_lock(&s_mutex);
    int32 last = s_surfaceSlot - 1;
    if (last < 0)
    {
        last = MAX_SURFACE_NUMBER - 1;
    }
    for(; s_surfaceSlot != last; s_surfaceSlot++)
    {
        if (s_surfaceSlot >= MAX_SURFACE_NUMBER)
        {
            s_surfaceSlot = 0;
        }
        if (!(s_surfaceList[s_surfaceSlot].pSurface))
        {
            s_surfaceList[s_surfaceSlot].pSurface = buffer;
            s_surfaceList[s_surfaceSlot].busy = false;
            close(fd);
            *pImage = s_surfaceSlot;
            wl_buffer_add_listener(buffer, &s_bufferListener, &(s_surfaceList[s_surfaceSlot].busy));
            s_surfaceSlot++;
            pthread_mutex_unlock(&s_mutex);
            return Success;
        }
    }
    pthread_mutex_unlock(&s_mutex);
    return NotEnoughResource;
}

// Destroy an image.
static void WaylandDestroyImage(
    int32 hImage)
{
    WSA_ASSERT((hImage >= 0) && (hImage < MAX_SURFACE_NUMBER));

    pthread_mutex_lock(&s_mutex);
    if (s_surfaceList[hImage].pSurface)
    {
        wl_buffer_destroy(s_surfaceList[hImage].pSurface);
        s_surfaceList[hImage].pSurface = NULL;
    }
    pthread_mutex_unlock(&s_mutex);
}

// Present.
static WsaError WaylandPresent(
    int32          hWsa,
    int32          hImage,
    WsaRegionList* presentRegions)
{
    WSA_ASSERT((hWsa >= 0) && (hWsa < MAX_INSTANCE_NUMBER) && s_instanceList[hWsa].initialized);
    WSA_ASSERT((hImage < MAX_SURFACE_NUMBER) && (s_surfaceList[hImage].pSurface != NULL));

    pthread_mutex_lock(&s_mutex);

    s_surfaceList[hImage].busy = true;
    wl_surface_attach(s_instanceList[hWsa].pSurfaceWrapper, s_surfaceList[hImage].pSurface, 0, 0);
    if (!presentRegions)
    {
        wl_surface_damage(s_instanceList[hWsa].pSurfaceWrapper, 0, 0, INT32_MAX, INT32_MAX);
    }
    else
    {
        // Not supported yet.
    }
    s_instanceList[hWsa].pFrame = wl_surface_frame(s_instanceList[hWsa].pSurfaceWrapper);
    wl_callback_add_listener(s_instanceList[hWsa].pFrame, &s_frameListener, &(s_instanceList[hWsa].frameCompleted));
    s_instanceList[hWsa].frameCompleted = false;
    wl_surface_commit(s_instanceList[hWsa].pSurfaceWrapper);
    wl_display_flush(s_instanceList[hWsa].pDisplay);

    pthread_mutex_unlock(&s_mutex);

    return Success;
}

// Return when the last image has been presented.
static WsaError WaylandWaitForLastImagePresented(
    int32 hWsa)
{
    WSA_ASSERT((hWsa >= 0) && (hWsa < MAX_INSTANCE_NUMBER) && s_instanceList[hWsa].initialized);

    int32 ret = 0;

// functions like wl_display_dispatch_queue are not safe to be called in different threads at the same time,
// the event from server might be obtained by other thread(like PAL does), and it will cause the current thead
// stuck in the poll, so here, we need to ensure when this function is called, no other thread can get the events.
    pthread_mutex_lock(&s_mutex);
    while(!s_instanceList[hWsa].frameCompleted)
    {
        ret = wl_display_dispatch_queue(s_instanceList[hWsa].pDisplay, s_instanceList[hWsa].pQueue);
        if (ret < 0)
        {
            pthread_mutex_unlock(&s_mutex);
            return UnknownFailure;
        }
    }
    pthread_mutex_unlock(&s_mutex);

    return Success;
}

// Check whether the image is avaiable (not used by the server side).
static WsaError WaylandImageAvailable(
    int32 hWsa,
    int32 hImage)
{
    WSA_ASSERT((hWsa >= 0) && (hWsa < MAX_INSTANCE_NUMBER) && s_instanceList[hWsa].initialized);
    WSA_ASSERT((hImage >= 0) && (hImage < MAX_SURFACE_NUMBER));

    if (s_surfaceList[hImage].busy == false)
    {
        return Success;
    }

// the same reason as in WaylandWaitForLastImagePresented for why we need to lock here.
    pthread_mutex_lock(&s_mutex);
    // firstly handle pending events in the queue.
    int ret = wl_display_dispatch_queue_pending(s_instanceList[hWsa].pDisplay, s_instanceList[hWsa].pQueue);
    if (ret < 0)
    {
        pthread_mutex_unlock(&s_mutex);
        return UnknownFailure;
    }

     // secondly, clear all requests and handle pending events
    ret = wl_display_roundtrip_queue(s_instanceList[hWsa].pDisplay, s_instanceList[hWsa].pQueue);
    if (ret < 0)
    {
        pthread_mutex_unlock(&s_mutex);
        return UnknownFailure;
    }
    ret = wl_display_dispatch_queue_pending(s_instanceList[hWsa].pDisplay, s_instanceList[hWsa].pQueue);
    if (ret < 0)
    {
        pthread_mutex_unlock(&s_mutex);
        return UnknownFailure;
    }
    if (s_surfaceList[hImage].busy == false)
    {
        pthread_mutex_unlock(&s_mutex);
        return Success;
    }
    pthread_mutex_unlock(&s_mutex);

    return ResourceBusy;
}

// Get window size.
static WsaError WaylandGetWindowGeometry(
    void*   pDisplay,
    void*   pSurface,
    uint32* pWidth,
    uint32* pHeight)
{
    WSA_ASSERT(pWidth && pHeight);

    *pWidth = *pHeight = 0xFFFFFFFF;

    return Success;
}

// Check whether presentation is supported
static WsaError WaylandPresentationSupported(
    void*   pDisplay,
    void*   pData)
{
    return Success;
}

// ==============================================
// Wsa wayland interface variable, exported as interface
EXPORT WsaInterface WaylandWsaInterface =
{
    .pfnQueryVersion              = QueryWaylandWsaVer,
    .pfnCreateWsa                 = CreateWaylandWsa,
    .pfnInitialize                = InitializeWaylandWsa,
    .pfnDestroyWsa                = DestroyWaylandWsa,
    .pfnCreateImage               = WaylandCreateImage,
    .pfnDestroyImage              = WaylandDestroyImage,
    .pfnPresent                   = WaylandPresent,
    .pfnWaitForLastImagePresented = WaylandWaitForLastImagePresented,
    .pfnImageAvailable            = WaylandImageAvailable,
    .pfnGetWindowGeometry         = WaylandGetWindowGeometry,
    .pfnPresentationSupported     = WaylandPresentationSupported,
};
// ==============================================
