/****************************************************************************
Copyright 2020  Adrien Faveraux <ad1rie3@hotmail.fr>

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
****************************************************************************/
#pragma once

#include "xdgoutput.h"

#include "client.h"
#include "display.h"
#include "output.h"
#include "wayland/display.h"
#include "wayland/global.h"
#include "wayland/resource.h"

#include <wayland-xdg-output-server-protocol.h>

#include <map>
#include <memory>

namespace Wrapland::Server
{

class D_isplay;
class OutputInterface;
class XdgOutput;

class XdgOutputManager::Private : public Wayland::Global<XdgOutputManager>
{
public:
    Private(D_isplay* display, XdgOutputManager* qptr);
    ~Private() override = default;
    std::map<Output*, XdgOutput*> outputs;

private:
    static void destroyCallback(wl_client* client, wl_resource* resource);
    static void getXdgOutputCallback(wl_client* client,
                                     wl_resource* resource,
                                     uint32_t id,
                                     wl_resource* output);

    static const struct zxdg_output_manager_v1_interface s_interface;
};
class XdgOutput::Private
{
public:
    void resourceConnected(XdgOutputV1* resource);
    void resourceDisconnected(XdgOutputV1* resource);
    QPoint pos;
    QSize size;
    bool dirty = false;
    bool doneOnce = false;
    std::vector<XdgOutputV1*> resources;
};

class XdgOutputV1::Private : public Wayland::Resource<XdgOutputV1>
{
public:
    Private(Client* client, uint32_t version, uint32_t id, XdgOutputV1* q);
    ~Private() override = default;
    void setLogicalSize(const QSize& size);
    void setLogicalPosition(const QPoint& pos);
    void done();

private:
    static const struct zxdg_output_v1_interface s_interface;
};

}
