/****************************************************************************
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
****************************************************************************/
#pragma once

#include "relative_pointer_v1.h"

#include "wayland/global.h"
#include "wayland/resource.h"

#include <wayland-relativepointer-unstable-v1-server-protocol.h>

namespace Wrapland
{
namespace Server
{

class RelativePointerManagerV1::Private : public Wayland::Global<RelativePointerManagerV1>
{
public:
    Private(RelativePointerManagerV1* q, D_isplay* display);

private:
    static void relativePointerCallback(wl_client* wlClient,
                                        wl_resource* wlResource,
                                        uint32_t id,
                                        wl_resource* wlPointer);

    static const struct zwp_relative_pointer_manager_v1_interface s_interface;
};

class RelativePointerV1 : public QObject
{
    Q_OBJECT
public:
    explicit RelativePointerV1(Client* client, uint32_t version, uint32_t id);
    ~RelativePointerV1() override;

    void
    relativeMotion(quint64 microseconds, const QSizeF& delta, const QSizeF& deltaNonAccelerated);

Q_SIGNALS:
    void resourceDestroyed();

private:
    class Private;
    Private* d_ptr;
};

class RelativePointerV1::Private : public Wayland::Resource<RelativePointerV1>
{
public:
    Private(Client* client, uint32_t version, uint32_t id, RelativePointerV1* q);
    ~Private() override;

private:
    static const struct zwp_relative_pointer_v1_interface s_interface;
};

}
}
