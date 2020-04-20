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

#pragma once


#include "keystate.h"
#include "wayland/global.h"
#include "wayland/resource.h"

namespace Wrapland
{
namespace Server
{

class KeyState;
class KeyState::Private : public Wayland::Global<KeyState>
{
public:
    Private(D_isplay *d, KeyState *q);

private:
    void bind(Wayland::Client * client, uint32_t version, uint32_t id) override;
    void unbind(Wayland::Resource *resource);
    void fetchStatesCallback(struct Wayland::Client */*client*/, struct Wayland::Resource *resource);
    
    static const quint32 s_version = 1;
    QVector<Wayland::Resource*> m_resources;
    QVector<State> m_keyStates = QVector<State>(3, Unlocked);
    static const struct org_kde_kwin_keystate_interface s_interface;
};



}
}

