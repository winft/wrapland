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
#include "client.h"
#include "client_p.h"

#include "wayland/client.h"

#include "display.h"

#include <wayland-server.h>

namespace Wrapland::Server
{

Client::Client(wl_client* wlClient, Display* display)
    : QObject(display)
    , d_ptr(new Private(wlClient, this, display))

{
}

Client::~Client() = default;

void Client::flush()
{
    d_ptr->flush();
}

void Client::destroy()
{
    d_ptr->destroy();
}

wl_resource* Client::getResource(quint32 id)
{
    return d_ptr->getResource(id);
}

wl_client* Client::native() const
{
    return d_ptr->native();
}

Display* Client::display()
{
    return d_ptr->m_display;
}

gid_t Client::groupId() const
{
    return d_ptr->groupId();
}

pid_t Client::processId() const
{
    return d_ptr->processId();
}

uid_t Client::userId() const
{
    return d_ptr->userId();
}

std::string Client::executablePath() const
{
    return d_ptr->executablePath();
}

}
