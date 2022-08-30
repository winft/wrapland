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

#include "display.h"
#include "wl_output.h"
#include "xdg_output.h"

#include "wayland/client.h"
#include "wayland/display.h"
#include "wayland/global.h"
#include "wayland/resource.h"

#include <wayland-xdg-output-server-protocol.h>

#include <QPoint>
#include <QSize>
#include <map>
#include <memory>

namespace Wrapland::Server
{

class Display;
class XdgOutput;
class XdgOutputV1;

constexpr uint32_t XdgOutputManagerVersion = 3;
using XdgOutputManagerGlobal = Wayland::Global<XdgOutputManager, XdgOutputManagerVersion>;
using XdgOutputManagerBind = Wayland::Bind<XdgOutputManagerGlobal>;

class XdgOutputManager::Private : public XdgOutputManagerGlobal
{
public:
    Private(Display* display, XdgOutputManager* q_ptr);

    std::map<Output*, XdgOutput*> outputs;

private:
    static void getXdgOutputCallback(XdgOutputManagerBind* bind, uint32_t id, wl_resource* output);

    static const struct zxdg_output_manager_v1_interface s_interface;
};
class XdgOutput::Private
{
public:
    Private(Output* output, Display* display, XdgOutput* q_ptr);

    bool broadcast();
    void done();

    void resourceConnected(XdgOutputV1* resource);
    void resourceDisconnected(XdgOutputV1* resource);

private:
    Output* output;
    XdgOutputManager* manager;
    std::vector<XdgOutputV1*> resources;
    bool sent_once{false};
};

class XdgOutputV1 : public QObject
{
    Q_OBJECT
public:
    XdgOutputV1(Client* client, uint32_t version, uint32_t id);

    void send_logical_position(QPointF const& pos) const;
    void send_logical_size(QSizeF const& size) const;
    void send_name(std::string const& name) const;
    void send_description(std::string const& description) const;
    void done() const;

    class Private;
    Private* d_ptr;

Q_SIGNALS:
    void resourceDestroyed();
};

class XdgOutputV1::Private : public Wayland::Resource<XdgOutputV1>
{
public:
    Private(Client* client, uint32_t version, uint32_t id, XdgOutputV1* q_ptr);

private:
    static const struct zxdg_output_v1_interface s_interface;
};

}
