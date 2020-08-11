/********************************************************************
Copyright © 2020 Roman Gilg <subdiff@gmail.com>

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

#include "client.h"
#include "display.h"
#include "wayland/display.h"

#include <EGL/egl.h>

namespace Wrapland::Server
{

class Private : public Wayland::Display
{
public:
    explicit Private(Server::Display* display);

    Client* createClientHandle(wl_client* wlClient);
    Wayland::Client* castClientImpl(Server::Client* client) override;

    std::vector<WlOutput*> outputs;
    std::vector<OutputDeviceV1*> outputDevices;
    std::vector<Seat*> seats;

    std::unique_ptr<XdgOutputManager> xdg_output_manager;
    EGLDisplay eglDisplay = EGL_NO_DISPLAY;

    static Private* castDisplay(Server::Display* display);

private:
    friend class Wayland::Display;
    Server::Display* q_ptr;
};

}
