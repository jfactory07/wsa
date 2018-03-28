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

#pragma once

#include <stdio.h>
#include <assert.h>
#include <stdbool.h>
#include <wayland-drm-client-protocol.h>

#define EXPORT
#define MAX_SURFACE_NUMBER  16384
#define MAX_INSTANCE_NUMBER 16384

typedef struct waylandWsaInstance
{
    bool                             created;          // whether the instance has been created in this slot.
    bool                             initialized;      // whether the instance has been initialized.
    struct wl_display*               pDisplay;
    struct wl_surface*               pSurface;
    struct wl_display*               pDisplayWrapper;
    struct wl_surface*               pSurfaceWrapper;
    struct wl_event_queue*           pQueue;
    struct wl_drm*                   pDrm;
    struct wl_drm*                   pDrmWrapper;
    uint32                           capabilities;
    struct wl_callback*              pFrame;
    bool                             frameCompleted;  // whether the last frame has been completed.
}WaylandWsaInstance;

typedef struct waylandWsaSurface
{
    struct wl_buffer*                pSurface;
    bool                             busy;            // whether the buffer is busy
}WaylandWsaSurface;

