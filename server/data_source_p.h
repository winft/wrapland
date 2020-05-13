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
#include "data_source.h"

#include "wayland/resource.h"

#include <wayland-server.h>

namespace Wrapland
{
namespace Server
{

class DataSource::Private : public Wayland::Resource<DataSource>
{
public:
    Private(Client* client, uint32_t version, uint32_t id, DataSource* q);
    ~Private() override;

    std::vector<std::string> mimeTypes;
    DataDeviceManager::DnDActions supportedDnDActions = DataDeviceManager::DnDAction::None;

    DataSource* q_ptr;

private:
    void offer(std::string mimeType);

    static void offerCallback(wl_client* wlClient, wl_resource* wlResource, const char* mimeType);
    static void
    setActionsCallback(wl_client* wlClient, wl_resource* wlResource, uint32_t dnd_actions);

    const static struct wl_data_source_interface s_interface;
};

}
}
