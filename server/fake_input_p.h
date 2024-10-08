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
#include "fake_input.h"

#include "wayland/global.h"
#include "wayland/resource.h"

#include <vector>

#include <Wrapland/Server/wraplandserver_export.h>
#include <wayland-fake-input-server-protocol.h>

namespace Wrapland::Server
{

constexpr uint32_t FakeInputVersion = 4;
using FakeInputGlobal = Wayland::Global<FakeInput, FakeInputVersion>;

class FakeInput::Private : public FakeInputGlobal
{
public:
    Private(Display* display, FakeInput* q_ptr);
    ~Private() override;

    std::vector<FakeInputDevice*> devices;

private:
    void bindInit(FakeInputGlobal::bind_t* bind) override;
    void prepareUnbind(FakeInputGlobal::bind_t* bind) override;

    static void authenticateCallback(FakeInputGlobal::bind_t* bind,
                                     char const* application,
                                     char const* reason);
    static void
    pointerMotionCallback(FakeInputGlobal::bind_t* bind, wl_fixed_t delta_x, wl_fixed_t delta_y);
    static void pointerMotionAbsoluteCallback(FakeInputGlobal::bind_t* bind,
                                              wl_fixed_t pos_x,
                                              wl_fixed_t pos_y);
    static void buttonCallback(FakeInputGlobal::bind_t* bind, uint32_t button, uint32_t state);
    static void axisCallback(FakeInputGlobal::bind_t* bind, uint32_t axis, wl_fixed_t value);
    static void touchDownCallback(FakeInputGlobal::bind_t* bind,
                                  quint32 id,
                                  wl_fixed_t pos_x,
                                  wl_fixed_t pos_y);
    static void touchMotionCallback(FakeInputGlobal::bind_t* bind,
                                    quint32 id,
                                    wl_fixed_t pos_x,
                                    wl_fixed_t pos_y);
    static void touchUpCallback(FakeInputGlobal::bind_t* bind, quint32 id);
    static void touchCancelCallback(FakeInputGlobal::bind_t* bind);
    static void touchFrameCallback(FakeInputGlobal::bind_t* bind);
    static void keyboardKeyCallback(FakeInputGlobal::bind_t* bind, uint32_t button, uint32_t state);

    static FakeInputDevice* device(wl_resource* wlResource);
    FakeInputDevice* device(FakeInputGlobal::bind_t* bind) const;

    static const struct org_kde_kwin_fake_input_interface s_interface;
    QList<quint32> touchIds;
};

class FakeInputDevice::Private
{
public:
    explicit Private(FakeInputGlobal::bind_t* bind);

    FakeInputGlobal::bind_t* bind;
    bool authenticated = false;
};

}
