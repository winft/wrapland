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

#include <QObject>
#include <QRegion>

#include <Wrapland/Server/wraplandserver_export.h>

namespace Wrapland::Server
{
class Client;
class Compositor;

class WRAPLANDSERVER_EXPORT Region : public QObject
{
    Q_OBJECT
public:
    QRegion region() const;
    Client* client() const;

Q_SIGNALS:
    void regionChanged(const QRegion&);
    void resourceDestroyed();

private:
    friend class Compositor;
    Region(Client* client, uint32_t version, uint32_t id);

    class Private;
    Private* d_ptr;
};

}

Q_DECLARE_METATYPE(Wrapland::Server::Region*)
