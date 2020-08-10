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

#include "dpms.h"
#include "wayland/global.h"
#include "wayland/resource.h"

#include <wayland-dpms-server-protocol.h>

namespace Wrapland
{
namespace Server
{

class WlOutput;

class DpmsManager::Private : public Wayland::Global<DpmsManager>
{
public:
    Private(Display* display, DpmsManager* q);

private:
    static void
    getDpmsCallback(wl_client* wlClient, wl_resource* wlResource, uint32_t id, wl_resource* output);

    static const struct org_kde_kwin_dpms_manager_interface s_interface;
};

using Sender = std::function<void(wl_resource*)>;

class Dpms : public QObject
{
    Q_OBJECT
public:
    Dpms(Client* client, uint32_t version, uint32_t id, WlOutput* output);

    void sendSupported();
    void sendMode();
    void sendDone();

Q_SIGNALS:
    void resourceDestroyed();

private:
    class Private;
    Private* d_ptr;
};

class Dpms::Private : public Wayland::Resource<Dpms>
{
public:
    Private(Client* client, uint32_t version, uint32_t id, WlOutput* output, Dpms* q);

    WlOutput* output;

private:
    static void setCallback(wl_client* client, wl_resource* wlResource, uint32_t mode);
    static const struct org_kde_kwin_dpms_interface s_interface;
};

}
}
