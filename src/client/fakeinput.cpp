/********************************************************************
Copyright 2015  Martin Gräßlin <mgraesslin@kde.org>

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
#include "fakeinput.h"
#include "event_queue.h"
#include "seat.h"
#include "wayland_pointer_p.h"
#include <QPointF>
#include <QSizeF>

#include <config-wrapland.h>
#if HAVE_LINUX_INPUT_H
#include <linux/input.h>
#endif

#include <wayland-fake-input-client-protocol.h>

namespace Wrapland
{
namespace Client
{

class Q_DECL_HIDDEN FakeInput::Private
{
public:
    WaylandPointer<org_kde_kwin_fake_input, org_kde_kwin_fake_input_destroy> manager;
    EventQueue* queue = nullptr;

    void sendPointerButtonState(Qt::MouseButton button, quint32 state);
};

FakeInput::FakeInput(QObject* parent)
    : QObject(parent)
    , d(new Private)
{
}

FakeInput::~FakeInput()
{
    release();
}

void FakeInput::release()
{
    d->manager.release();
}

bool FakeInput::isValid() const
{
    return d->manager.isValid();
}

void FakeInput::setup(org_kde_kwin_fake_input* manager)
{
    Q_ASSERT(manager);
    Q_ASSERT(!d->manager.isValid());
    d->manager.setup(manager);
}

EventQueue* FakeInput::eventQueue()
{
    return d->queue;
}

void FakeInput::setEventQueue(EventQueue* queue)
{
    d->queue = queue;
}

void FakeInput::authenticate(QString const& applicationName, QString const& reason)
{
    Q_ASSERT(d->manager.isValid());
    org_kde_kwin_fake_input_authenticate(
        d->manager, applicationName.toUtf8().constData(), reason.toUtf8().constData());
}

void FakeInput::requestPointerMove(QSizeF const& delta)
{
    Q_ASSERT(d->manager.isValid());
    org_kde_kwin_fake_input_pointer_motion(
        d->manager, wl_fixed_from_double(delta.width()), wl_fixed_from_double(delta.height()));
}

void FakeInput::requestPointerMoveAbsolute(QPointF const& pos)
{
    Q_ASSERT(d->manager.isValid());
    if (wl_proxy_get_version(d->manager)
        < ORG_KDE_KWIN_FAKE_INPUT_POINTER_MOTION_ABSOLUTE_SINCE_VERSION) {
        return;
    }

    org_kde_kwin_fake_input_pointer_motion_absolute(
        d->manager, wl_fixed_from_double(pos.x()), wl_fixed_from_double(pos.y()));
}

void FakeInput::Private::sendPointerButtonState(Qt::MouseButton button, quint32 state)
{
#if HAVE_LINUX_INPUT_H
    Q_ASSERT(manager.isValid());
    uint32_t b = 0;
    switch (button) {
    case Qt::LeftButton:
        b = BTN_LEFT;
        break;
    case Qt::RightButton:
        b = BTN_RIGHT;
        break;
    case Qt::MiddleButton:
        b = BTN_MIDDLE;
        break;
    default:
        // TODO: more buttons, check implementation in QtWayland
        // unsupported button
        return;
    }
    org_kde_kwin_fake_input_button(manager, b, state);
#endif
}

void FakeInput::requestPointerButtonPress(Qt::MouseButton button)
{
    d->sendPointerButtonState(button, WL_POINTER_BUTTON_STATE_PRESSED);
}

void FakeInput::requestPointerButtonPress(quint32 linuxButton)
{
    Q_ASSERT(d->manager.isValid());
    org_kde_kwin_fake_input_button(d->manager, linuxButton, WL_POINTER_BUTTON_STATE_PRESSED);
}

void FakeInput::requestPointerButtonRelease(Qt::MouseButton button)
{
    d->sendPointerButtonState(button, WL_POINTER_BUTTON_STATE_RELEASED);
}

void FakeInput::requestPointerButtonRelease(quint32 linuxButton)
{
    Q_ASSERT(d->manager.isValid());
    org_kde_kwin_fake_input_button(d->manager, linuxButton, WL_POINTER_BUTTON_STATE_RELEASED);
}

void FakeInput::requestPointerButtonClick(Qt::MouseButton button)
{
    requestPointerButtonPress(button);
    requestPointerButtonRelease(button);
}

void FakeInput::requestPointerButtonClick(quint32 linuxButton)
{
    requestPointerButtonPress(linuxButton);
    requestPointerButtonRelease(linuxButton);
}

void FakeInput::requestPointerAxis(Qt::Orientation axis, qreal delta)
{
    Q_ASSERT(d->manager.isValid());
    uint32_t a;
    switch (axis) {
    case Qt::Horizontal:
        a = WL_POINTER_AXIS_HORIZONTAL_SCROLL;
        break;
    case Qt::Vertical:
        a = WL_POINTER_AXIS_VERTICAL_SCROLL;
        break;
    default:
        Q_UNREACHABLE();
        break;
    }
    org_kde_kwin_fake_input_axis(d->manager, a, wl_fixed_from_double(delta));
}

void FakeInput::requestTouchDown(quint32 id, QPointF const& pos)
{
    Q_ASSERT(d->manager.isValid());
    org_kde_kwin_fake_input_touch_down(
        d->manager, id, wl_fixed_from_double(pos.x()), wl_fixed_from_double(pos.y()));
}

void FakeInput::requestTouchMotion(quint32 id, QPointF const& pos)
{
    Q_ASSERT(d->manager.isValid());
    org_kde_kwin_fake_input_touch_motion(
        d->manager, id, wl_fixed_from_double(pos.x()), wl_fixed_from_double(pos.y()));
}

void FakeInput::requestTouchUp(quint32 id)
{
    Q_ASSERT(d->manager.isValid());
    org_kde_kwin_fake_input_touch_up(d->manager, id);
}

void FakeInput::requestTouchCancel()
{
    Q_ASSERT(d->manager.isValid());
    org_kde_kwin_fake_input_touch_cancel(d->manager);
}

void FakeInput::requestTouchFrame()
{
    Q_ASSERT(d->manager.isValid());
    org_kde_kwin_fake_input_touch_frame(d->manager);
}

void FakeInput::requestKeyboardKeyPress(quint32 linuxKey)
{
    Q_ASSERT(d->manager.isValid());
    if (wl_proxy_get_version(d->manager) < ORG_KDE_KWIN_FAKE_INPUT_KEYBOARD_KEY_SINCE_VERSION) {
        return;
    }

    org_kde_kwin_fake_input_keyboard_key(d->manager, linuxKey, WL_KEYBOARD_KEY_STATE_PRESSED);
}

void FakeInput::requestKeyboardKeyRelease(quint32 linuxKey)
{
    Q_ASSERT(d->manager.isValid());
    if (wl_proxy_get_version(d->manager) < ORG_KDE_KWIN_FAKE_INPUT_KEYBOARD_KEY_SINCE_VERSION) {
        return;
    }

    org_kde_kwin_fake_input_keyboard_key(d->manager, linuxKey, WL_KEYBOARD_KEY_STATE_RELEASED);
}

FakeInput::operator org_kde_kwin_fake_input*() const
{
    return d->manager;
}

FakeInput::operator org_kde_kwin_fake_input*()
{
    return d->manager;
}

}
}
