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
#include "keyboard_shortcuts_inhibit_p.h"

namespace Wrapland::Client
{

KeyboardShortcutsInhibitManagerV1::KeyboardShortcutsInhibitManagerV1(QObject* parent)
    : QObject(parent)
    , d_ptr(new Private)
{
}

void KeyboardShortcutsInhibitManagerV1::Private::setup(
    zwp_keyboard_shortcuts_inhibit_manager_v1* arg)
{
    Q_ASSERT(arg);
    Q_ASSERT(!idleinhibitmanager);
    idleinhibitmanager.setup(arg);
}

KeyboardShortcutsInhibitManagerV1::~KeyboardShortcutsInhibitManagerV1()
{
    release();
}

void KeyboardShortcutsInhibitManagerV1::setup(
    zwp_keyboard_shortcuts_inhibit_manager_v1* idleinhibitmanager)
{
    d_ptr->setup(idleinhibitmanager);
}

void KeyboardShortcutsInhibitManagerV1::release()
{
    d_ptr->idleinhibitmanager.release();
}

KeyboardShortcutsInhibitManagerV1::operator zwp_keyboard_shortcuts_inhibit_manager_v1*()
{
    return d_ptr->idleinhibitmanager;
}

KeyboardShortcutsInhibitManagerV1::operator zwp_keyboard_shortcuts_inhibit_manager_v1*() const
{
    return d_ptr->idleinhibitmanager;
}

bool KeyboardShortcutsInhibitManagerV1::isValid() const
{
    return d_ptr->idleinhibitmanager.isValid();
}

void KeyboardShortcutsInhibitManagerV1::setEventQueue(EventQueue* queue)
{
    d_ptr->queue = queue;
}

EventQueue* KeyboardShortcutsInhibitManagerV1::eventQueue()
{
    return d_ptr->queue;
}

KeyboardShortcutsInhibitorV1*
KeyboardShortcutsInhibitManagerV1::inhibitShortcuts(Surface* surface, Seat* seat, QObject* parent)
{
    Q_ASSERT(isValid());
    auto p = new KeyboardShortcutsInhibitorV1(parent);
    auto w = zwp_keyboard_shortcuts_inhibit_manager_v1_inhibit_shortcuts(
        d_ptr->idleinhibitmanager, *surface, *seat);
    if (d_ptr->queue) {
        d_ptr->queue->addProxy(w);
    }
    p->setup(w);
    Q_EMIT inhibitorCreated();
    return p;
}

zwp_keyboard_shortcuts_inhibitor_v1_listener const KeyboardShortcutsInhibitorV1::Private::s_listener
    = {
        ActiveCallback,
        InactiveCallback,
};

KeyboardShortcutsInhibitorV1::Private::Private(KeyboardShortcutsInhibitorV1* q)
    : q(q)
{
}

void KeyboardShortcutsInhibitorV1::Private::ActiveCallback(
    void* data,
    zwp_keyboard_shortcuts_inhibitor_v1* zwp_keyboard_shortcuts_inhibitor_v1)
{
    Q_UNUSED(zwp_keyboard_shortcuts_inhibitor_v1)
    auto priv = reinterpret_cast<Private*>(data);
    priv->inhibitor_active = true;
    Q_EMIT priv->q->inhibitorActive();
}

void KeyboardShortcutsInhibitorV1::Private::InactiveCallback(
    void* data,
    zwp_keyboard_shortcuts_inhibitor_v1* zwp_keyboard_shortcuts_inhibitor_v1)
{
    Q_UNUSED(zwp_keyboard_shortcuts_inhibitor_v1)
    auto priv = reinterpret_cast<Private*>(data);
    priv->inhibitor_active = false;
    Q_EMIT priv->q->inhibitorInactive();
}

KeyboardShortcutsInhibitorV1::KeyboardShortcutsInhibitorV1(QObject* parent)
    : QObject(parent)
    , d_ptr(new Private(this))
{
}

void KeyboardShortcutsInhibitorV1::Private::setup(zwp_keyboard_shortcuts_inhibitor_v1* arg)
{
    Q_ASSERT(arg);
    Q_ASSERT(!idleinhibitor);
    idleinhibitor.setup(arg);
    zwp_keyboard_shortcuts_inhibitor_v1_add_listener(arg, &s_listener, this);
}

KeyboardShortcutsInhibitorV1::~KeyboardShortcutsInhibitorV1()
{
    release();
}

bool KeyboardShortcutsInhibitorV1::isActive()
{
    return d_ptr->inhibitor_active;
}

void KeyboardShortcutsInhibitorV1::setup(zwp_keyboard_shortcuts_inhibitor_v1* idleinhibitor)
{
    d_ptr->setup(idleinhibitor);
}

void KeyboardShortcutsInhibitorV1::release()
{
    d_ptr->idleinhibitor.release();
}

KeyboardShortcutsInhibitorV1::operator zwp_keyboard_shortcuts_inhibitor_v1*()
{
    return d_ptr->idleinhibitor;
}

KeyboardShortcutsInhibitorV1::operator zwp_keyboard_shortcuts_inhibitor_v1*() const
{
    return d_ptr->idleinhibitor;
}

bool KeyboardShortcutsInhibitorV1::isValid() const
{
    return d_ptr->idleinhibitor.isValid();
}

}
