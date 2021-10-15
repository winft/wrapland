/****************************************************************************
Copyright 2020  Faveraux Adrien <ad1rie3@hotmail.fr>

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

#include <QObject>
#include <memory>

#include <Wrapland/Server/wraplandserver_export.h>

namespace Wrapland::Server
{
class Display;
class Surface;
class Seat;
class Client;
class KeyboardShortcutsInhibitorV1;

class WRAPLANDSERVER_EXPORT KeyboardShortcutsInhibitManagerV1 : public QObject
{
    Q_OBJECT

public:
    explicit KeyboardShortcutsInhibitManagerV1(Display* display);
    ~KeyboardShortcutsInhibitManagerV1() override;

    KeyboardShortcutsInhibitorV1* findInhibitor(Surface* surface, Seat* seat) const;

Q_SIGNALS:
    void inhibitorCreated(KeyboardShortcutsInhibitorV1* inhibitor);

private:
    friend class KeyboardShortcutsInhibitorV1;

    void removeInhibitor(Surface* surface, Seat* seat);

    class Private;
    std::unique_ptr<Private> d_ptr;
};

class WRAPLANDSERVER_EXPORT KeyboardShortcutsInhibitorV1 : public QObject
{
    Q_OBJECT

public:
    ~KeyboardShortcutsInhibitorV1() override;

    Surface* surface() const;
    Seat* seat() const;
    void setActive(bool active);
    bool isActive() const;

Q_SIGNALS:
    void resourceDestroyed();

private:
    friend class KeyboardShortcutsInhibitManagerV1;
    KeyboardShortcutsInhibitorV1(Client* client,
                                 uint32_t version,
                                 uint32_t id,
                                 Surface* surface,
                                 Seat* seat);

    class Private;
    Private* d_ptr;
};
}
