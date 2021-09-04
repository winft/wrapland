/********************************************************************
Copyright © 2020, 2021 Roman Gilg <subdiff@gmail.com>

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
#include "data_source.h"

#include "wayland/resource.h"

#include <wayland-server.h>

namespace Wrapland::Server
{
class data_source_res;

class data_source::Private
{
public:
    explicit Private(data_source* q_ptr);

    std::vector<std::string> mimeTypes;
    dnd_actions supportedDnDActions{dnd_action::none};

    data_source_res* res{nullptr};
    data_source* q_ptr;
};

class data_source_res_impl : public Wayland::Resource<data_source_res>
{
public:
    data_source_res_impl(Client* client, uint32_t version, uint32_t id, data_source_res* q);

    data_source_res* q_ptr;

private:
    static void offer_callback(wl_client* wlClient, wl_resource* wlResource, char const* mimeType);
    static void
    setActionsCallback(wl_client* wlClient, wl_resource* wlResource, uint32_t dnd_actions);

    static struct wl_data_source_interface const s_interface;
};

class data_source_res : public QObject
{
    Q_OBJECT
public:
    data_source_res(Client* client, uint32_t version, uint32_t id);

    void accept(std::string const& mimeType) const;
    void request_data(std::string const& mimeType, qint32 fd) const;
    void cancel() const;

    void send_dnd_drop_performed() const;
    void send_dnd_finished() const;
    void send_action(dnd_action action) const;

    data_source* src() const;
    data_source::Private* src_priv() const;

    std::unique_ptr<data_source> pub_src;
    data_source_res_impl* impl;

Q_SIGNALS:
    void resourceDestroyed();
};

}
