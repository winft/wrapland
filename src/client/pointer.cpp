/********************************************************************
Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>

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
#include "pointer.h"
#include "surface.h"
#include "wayland_pointer_p.h"
// Qt
#include <QPointF>
#include <QPointer>
// wayland
#include <wayland-client-protocol.h>

namespace Wrapland
{
namespace Client
{

static Pointer::Axis wlAxisToPointerAxis(uint32_t axis)
{
    switch (axis) {
    case WL_POINTER_AXIS_VERTICAL_SCROLL:
        return Pointer::Axis::Vertical;
    case WL_POINTER_AXIS_HORIZONTAL_SCROLL:
        return Pointer::Axis::Horizontal;
    }

    Q_UNREACHABLE();
}

class Q_DECL_HIDDEN Pointer::Private
{
public:
    Private(Pointer* q);
    void setup(wl_pointer* p);

    WaylandPointer<wl_pointer, wl_pointer_release> pointer;
    QPointer<Surface> enteredSurface;
    quint32 enteredSerial = 0;

private:
    void enter(uint32_t serial, wl_surface* surface, QPointF const& relativeToSurface);
    void leave(uint32_t serial);
    static void enterCallback(void* data,
                              wl_pointer* pointer,
                              uint32_t serial,
                              wl_surface* surface,
                              wl_fixed_t sx,
                              wl_fixed_t sy);
    static void
    leaveCallback(void* data, wl_pointer* pointer, uint32_t serial, wl_surface* surface);
    static void
    motionCallback(void* data, wl_pointer* pointer, uint32_t time, wl_fixed_t sx, wl_fixed_t sy);
    static void buttonCallback(void* data,
                               wl_pointer* pointer,
                               uint32_t serial,
                               uint32_t time,
                               uint32_t button,
                               uint32_t state);
    static void
    axisCallback(void* data, wl_pointer* pointer, uint32_t time, uint32_t axis, wl_fixed_t value);
    static void frameCallback(void* data, wl_pointer* pointer);
    static void axisSourceCallback(void* data, wl_pointer* pointer, uint32_t axis_source);
    static void axisStopCallback(void* data, wl_pointer* pointer, uint32_t time, uint32_t axis);
    static void
    axisDiscreteCallback(void* data, wl_pointer* pointer, uint32_t axis, int32_t discrete);

    Pointer* q;
    static wl_pointer_listener const s_listener;
};

Pointer::Private::Private(Pointer* q)
    : q(q)
{
}

void Pointer::Private::setup(wl_pointer* p)
{
    Q_ASSERT(p);
    Q_ASSERT(!pointer);
    pointer.setup(p);
    wl_pointer_add_listener(pointer, &s_listener, this);
}

wl_pointer_listener const Pointer::Private::s_listener = {
    enterCallback,
    leaveCallback,
    motionCallback,
    buttonCallback,
    axisCallback,
    frameCallback,
    axisSourceCallback,
    axisStopCallback,
    axisDiscreteCallback,
};

Pointer::Pointer(QObject* parent)
    : QObject(parent)
    , d(new Private(this))
{
}

Pointer::~Pointer()
{
    release();
}

void Pointer::release()
{
    d->pointer.release();
}

void Pointer::setup(wl_pointer* pointer)
{
    d->setup(pointer);
}

void Pointer::Private::enterCallback(void* data,
                                     wl_pointer* pointer,
                                     uint32_t serial,
                                     wl_surface* surface,
                                     wl_fixed_t sx,
                                     wl_fixed_t sy)
{
    auto p = reinterpret_cast<Pointer::Private*>(data);
    Q_ASSERT(p->pointer == pointer);
    p->enter(serial, surface, QPointF(wl_fixed_to_double(sx), wl_fixed_to_double(sy)));
}

void Pointer::Private::enter(uint32_t serial, wl_surface* surface, QPointF const& relativeToSurface)
{
    enteredSurface = QPointer<Surface>(Surface::get(surface));
    enteredSerial = serial;
    Q_EMIT q->entered(serial, relativeToSurface);
}

void Pointer::Private::leaveCallback(void* data,
                                     wl_pointer* pointer,
                                     uint32_t serial,
                                     wl_surface* surface)
{
    auto p = reinterpret_cast<Pointer::Private*>(data);
    Q_ASSERT(p->pointer == pointer);
    Q_UNUSED(surface)
    p->leave(serial);
}

void Pointer::Private::leave(uint32_t serial)
{
    enteredSurface.clear();
    Q_EMIT q->left(serial);
}

void Pointer::Private::motionCallback(void* data,
                                      wl_pointer* pointer,
                                      uint32_t time,
                                      wl_fixed_t sx,
                                      wl_fixed_t sy)
{
    auto p = reinterpret_cast<Pointer::Private*>(data);
    Q_ASSERT(p->pointer == pointer);
    Q_EMIT p->q->motion(QPointF(wl_fixed_to_double(sx), wl_fixed_to_double(sy)), time);
}

void Pointer::Private::buttonCallback(void* data,
                                      wl_pointer* pointer,
                                      uint32_t serial,
                                      uint32_t time,
                                      uint32_t button,
                                      uint32_t state)
{
    auto p = reinterpret_cast<Pointer::Private*>(data);
    Q_ASSERT(p->pointer == pointer);
    auto toState = [state] {
        if (state == WL_POINTER_BUTTON_STATE_RELEASED) {
            return ButtonState::Released;
        } else {
            return ButtonState::Pressed;
        }
    };
    Q_EMIT p->q->buttonStateChanged(serial, time, button, toState());
}

void Pointer::Private::axisCallback(void* data,
                                    wl_pointer* pointer,
                                    uint32_t time,
                                    uint32_t axis,
                                    wl_fixed_t value)
{
    auto p = reinterpret_cast<Pointer::Private*>(data);
    Q_ASSERT(p->pointer == pointer);
    Q_EMIT p->q->axisChanged(time, wlAxisToPointerAxis(axis), wl_fixed_to_double(value));
}

void Pointer::Private::frameCallback(void* data, wl_pointer* pointer)
{
    auto p = reinterpret_cast<Pointer::Private*>(data);
    Q_ASSERT(p->pointer == pointer);
    Q_EMIT p->q->frame();
}

void Pointer::Private::axisSourceCallback(void* data, wl_pointer* pointer, uint32_t axis_source)
{
    auto p = reinterpret_cast<Pointer::Private*>(data);
    Q_ASSERT(p->pointer == pointer);
    AxisSource source;
    switch (axis_source) {
    case WL_POINTER_AXIS_SOURCE_WHEEL:
        source = AxisSource::Wheel;
        break;
    case WL_POINTER_AXIS_SOURCE_FINGER:
        source = AxisSource::Finger;
        break;
    case WL_POINTER_AXIS_SOURCE_CONTINUOUS:
        source = AxisSource::Continuous;
        break;
    case WL_POINTER_AXIS_SOURCE_WHEEL_TILT:
        source = AxisSource::WheelTilt;
        break;
    default:
        Q_UNREACHABLE();
        break;
    }
    Q_EMIT p->q->axisSourceChanged(source);
}

void Pointer::Private::axisStopCallback(void* data,
                                        wl_pointer* pointer,
                                        uint32_t time,
                                        uint32_t axis)
{
    auto p = reinterpret_cast<Pointer::Private*>(data);
    Q_ASSERT(p->pointer == pointer);
    Q_EMIT p->q->axisStopped(time, wlAxisToPointerAxis(axis));
}

void Pointer::Private::axisDiscreteCallback(void* data,
                                            wl_pointer* pointer,
                                            uint32_t axis,
                                            int32_t discrete)
{
    auto p = reinterpret_cast<Pointer::Private*>(data);
    Q_ASSERT(p->pointer == pointer);
    Q_EMIT p->q->axisDiscreteChanged(wlAxisToPointerAxis(axis), discrete);
}

void Pointer::setCursor(Surface* surface, QPoint const& hotspot)
{
    Q_ASSERT(isValid());
    wl_surface* s = nullptr;
    if (surface) {
        s = *surface;
    }
    wl_pointer_set_cursor(d->pointer, d->enteredSerial, s, hotspot.x(), hotspot.y());
}

void Pointer::hideCursor()
{
    setCursor(nullptr);
}

Surface* Pointer::enteredSurface()
{
    return d->enteredSurface.data();
}

Surface* Pointer::enteredSurface() const
{
    return d->enteredSurface.data();
}

bool Pointer::isValid() const
{
    return d->pointer.isValid();
}

Pointer::operator wl_pointer*() const
{
    return d->pointer;
}

Pointer::operator wl_pointer*()
{
    return d->pointer;
}

}
}
