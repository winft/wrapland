/********************************************************************
Copyright 2020  Faveraux Adrien <ad1rie3@hotmail.fr>

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) version 3, or any
later version accepted by the membership of KDE e.V. (or its
successor approved by the membership of KDE e.V.), which shall
act as a proxy defined in Section 6 of version 3 of the license.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#pragma once

#include "display.h"
#include "fakeinput.h"

#include "wayland/global.h"
#include "wayland/resource.h"

#include <vector>

#include <Wrapland/Server/wraplandserver_export.h>
#include <wayland-fake-input-server-protocol.h>

namespace Wrapland::Server
{

constexpr uint32_t FakeInputVersion = 4;
using FakeInputGlobal = Wayland::Global<FakeInput, FakeInputVersion>;
using FakeInputBind = Wayland::Resource<FakeInput, FakeInputGlobal>;

class FakeInput::Private : public FakeInputGlobal
{
public:
    Private(D_isplay* display, FakeInput* q);
    ~Private() override;

    std::vector<FakeInputDevice*> devices;

private:
    void bindInit(FakeInputBind* bind) override;
    void prepareUnbind(FakeInputBind* bind) override;

    static void authenticateCallback(wl_client* wlClient,
                                     wl_resource* wlResource,
                                     const char* application,
                                     const char* reason);
    static void pointerMotionCallback(wl_client* wlClient,
                                      wl_resource* wlResource,
                                      wl_fixed_t delta_x,
                                      wl_fixed_t delta_y);
    static void pointerMotionAbsoluteCallback(wl_client* wlClient,
                                              wl_resource* wlResource,
                                              wl_fixed_t x,
                                              wl_fixed_t y);
    static void
    buttonCallback(wl_client* wlClient, wl_resource* wlResource, uint32_t button, uint32_t state);
    static void
    axisCallback(wl_client* wlClient, wl_resource* wlResource, uint32_t axis, wl_fixed_t value);
    static void touchDownCallback(wl_client* wlClient,
                                  wl_resource* wlResource,
                                  quint32 id,
                                  wl_fixed_t x,
                                  wl_fixed_t y);
    static void touchMotionCallback(wl_client* wlClient,
                                    wl_resource* wlResource,
                                    quint32 id,
                                    wl_fixed_t x,
                                    wl_fixed_t y);
    static void touchUpCallback(wl_client* wlClient, wl_resource* wlResource, quint32 id);
    static void touchCancelCallback(wl_client* wlClient, wl_resource* wlResource);
    static void touchFrameCallback(wl_client* wlClient, wl_resource* wlResource);
    static void keyboardKeyCallback(wl_client* wlClient,
                                    wl_resource* wlResource,
                                    uint32_t button,
                                    uint32_t state);

    static FakeInputDevice* device(wl_resource* wlResource);

    static const struct org_kde_kwin_fake_input_interface s_interface;
    static QList<quint32> touchIds;
};

class FakeInputDevice::Private
{
public:
    Private(wl_resource* wlResource, FakeInput* interface);

    wl_resource* resource;
    FakeInput* interface;
    bool authenticated = false;
};
}
