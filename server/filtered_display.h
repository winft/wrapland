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
#pragma once

#include "display.h"

#include <Wrapland/Server/wraplandserver_export.h>
#include <memory>

namespace Wrapland::Server
{

/**
 * Server Implementation that allows one to restrict which globals are available to which clients.
 *
 * Users of this class must implement the virtual @method allowInterface method.
 */
class WRAPLANDSERVER_EXPORT FilteredDisplay : public Display
{
    Q_OBJECT
public:
    FilteredDisplay();
    ~FilteredDisplay() override;

    /**
     * Return whether the @arg client can see the interface with the given @arg interfaceName
     *
     * When false will not see these globals for a given interface in the registry,
     * and any manual attempts to bind will fail
     *
     * @return true if the client should be able to access the global with the following
     * interfaceName
     */
    virtual bool allowInterface(Client* client, QByteArray const& interfaceName) = 0;

private:
    class Private;
    std::unique_ptr<Private> d_ptr;
};

}
