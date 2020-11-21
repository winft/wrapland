/********************************************************************
Copyright 2019 Aleix Pol Gonzalez <aleixpol@kde.org>

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
#ifndef WAYLAND_KEYSTATE_H
#define WAYLAND_KEYSTATE_H

#include <QObject>
// STD
#include <memory>

#include <Wrapland/Client/wraplandclient_export.h>

struct org_kde_kwin_keystate;

namespace Wrapland
{
namespace Client
{
class EventQueue;

class WRAPLANDCLIENT_EXPORT Keystate : public QObject
{
    Q_OBJECT
public:
    enum class Key {
        CapsLock = 0,
        NumLock = 1,
        ScrollLock = 2,
    };
    Q_ENUM(Key);
    enum State {
        Unlocked = 0,
        Latched = 1,
        Locked = 2,
    };
    Q_ENUM(State)

    Keystate(QObject* parent);
    ~Keystate() override;

    void setEventQueue(EventQueue* queue);

    /**
     * Releases the org_kde_kwin_keystate interface.
     * After the interface has been released the Keystate instance is no
     * longer valid and can be setup with another org_kde_kwin_keystate interface.
     **/
    void release();

    void setup(org_kde_kwin_keystate* keystate);

    void fetchStates();

Q_SIGNALS:
    /**
     * State of the @p key changed to @p state
     */
    void stateChanged(Key key, State state);

    /**
     * The corresponding global for this interface on the Registry got removed.
     **/
    void removed();

private:
    class Private;
    std::unique_ptr<Private> d;
};

}
}

#endif
