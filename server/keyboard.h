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

#include <Wrapland/Server/wraplandserver_export.h>

namespace Wrapland
{
namespace Server
{
class Client;
class Seat;

class Surface;

class WRAPLANDSERVER_EXPORT Keyboard : public QObject
{
    Q_OBJECT
public:
    ~Keyboard() override;

    Client* client() const;
    Surface* focusedSurface() const;

Q_SIGNALS:
    void resourceDestroyed();

private:
    void setFocusedSurface(quint32 serial, Surface* surface);
    void setKeymap(int fd, quint32 size);
    void updateModifiers(quint32 serial,
                         quint32 depressed,
                         quint32 latched,
                         quint32 locked,
                         quint32 group);
    void keyPressed(quint32 serial, quint32 key);
    void keyReleased(quint32 serial, quint32 key);
    void repeatInfo(qint32 charactersPerSecond, qint32 delay);

    friend class Seat;
    Keyboard(Client* client, uint32_t version, uint32_t id, Seat* seat);

    class Private;
    Private* d_ptr;
};

}
}

Q_DECLARE_METATYPE(Wrapland::Server::Keyboard*)