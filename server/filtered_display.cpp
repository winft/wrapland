/********************************************************************
Copyright 2017  David Edmundson <davidedmundson@kde.org>
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
#include "filtered_display.h"

#include <wayland-server.h>

#include <QByteArray>

namespace Wrapland::Server
{

class FilteredDisplay::Private
{
public:
    Private(FilteredDisplay* q);

    static bool filterCallback(const wl_client* wlClient, const wl_global* wlGlobal, void* data);

private:
    FilteredDisplay* q_ptr;
};

FilteredDisplay::Private::Private(FilteredDisplay* q)
    : q_ptr(q)
{
}

bool FilteredDisplay::Private::filterCallback(const wl_client* wlClient,
                                              const wl_global* wlGlobal,
                                              void* data)
{
    auto priv = static_cast<FilteredDisplay::Private*>(data);

    auto client = priv->q_ptr->getClient(const_cast<wl_client*>(wlClient));
    auto interface = wl_global_get_interface(wlGlobal);
    auto name = QByteArray::fromRawData(interface->name, strlen(interface->name));

    return priv->q_ptr->allowInterface(client, name);
}

FilteredDisplay::FilteredDisplay(QObject* parent)
    : D_isplay(parent)
    , d_ptr{new Private(this)}
{
    connect(this, &D_isplay::started, [this]() {
        wl_display_set_global_filter(native(), Private::filterCallback, d_ptr.get());
    });
}

FilteredDisplay::~FilteredDisplay() = default;

}
