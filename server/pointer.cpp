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
#include "pointer.h"
#include "pointer_p.h"

#include "client.h"
#include "data_device.h"
#include "display.h"
#include "seat.h"

#include "wayland/client.h"
#include "wayland/display.h"
#include "wayland/global.h"

#include "pointer_constraints_v1.h"
#include "pointer_gestures_v1_p.h"
#include "relative_pointer_v1_p.h"

#include "subcompositor.h"
#include "surface.h"
#include "surface_p.h"

#include <wayland-server.h>

namespace Wrapland::Server
{

const struct wl_pointer_interface Pointer::Private::s_interface = {
    setCursorCallback,
    destroyCallback,
};

void Pointer::Private::setCursorCallback([[maybe_unused]] wl_client* wlClient,
                                         wl_resource* wlResource,
                                         uint32_t serial,
                                         wl_resource* wlSurface,
                                         int32_t hotspot_x,
                                         int32_t hotspot_y)
{
    auto priv = handle(wlResource)->d_ptr;
    auto surface = wlSurface ? Wayland::Resource<Surface>::handle(wlSurface) : nullptr;

    priv->setCursor(serial, surface, QPoint(hotspot_x, hotspot_y));
}

Pointer::Private::Private(Client* client, uint32_t version, uint32_t id, Seat* _seat, Pointer* q)
    : Wayland::Resource<Pointer>(client, version, id, &wl_pointer_interface, &s_interface, q)
    , seat{_seat}
{
    // TODO(unknown author): handle touch
    connect(seat, &Seat::pointerPosChanged, q, [this] {
        if (!focusedSurface) {
            return;
        }
        if (seat->isDragPointer()) {
            const auto* originSurface = seat->dragSource()->origin();
            const bool proxyRemoteFocused
                = originSurface->dataProxy() && originSurface == focusedSurface;
            if (!proxyRemoteFocused) {
                // handled by DataDevice
                return;
            }
        }
        if (!focusedSurface->lockedPointer().isNull()
            && focusedSurface->lockedPointer()->isLocked()) {
            return;
        }
        const QPointF pos = seat->focusedPointerSurfaceTransformation().map(seat->pointerPos());
        sendMotion(pos);
        sendFrame();
    });
}

void Pointer::Private::setCursor(quint32 serial, Surface* surface, const QPoint& hotspot)
{
    if (!cursor) {
        cursor.reset(new Cursor(handle()));
        cursor->d_ptr->update(QPointer<Surface>(surface), serial, hotspot);
        QObject::connect(cursor.get(), &Cursor::changed, handle(), &Pointer::cursorChanged);
        Q_EMIT handle()->cursorChanged();
    } else {
        cursor->d_ptr->update(QPointer<Surface>(surface), serial, hotspot);
    }
}

void Pointer::Private::sendEnter(quint32 serial, Surface* surface, QPointF const& pos)
{
    send<wl_pointer_send_enter>(serial,
                                surface->d_ptr->resource(),
                                wl_fixed_from_double(pos.x()),
                                wl_fixed_from_double(pos.y()));
}

void Pointer::Private::sendLeave(quint32 serial, Surface* surface)
{
    if (!surface) {
        return;
    }
    send<wl_pointer_send_leave>(serial, surface->d_ptr->resource());
}

void Pointer::Private::sendMotion(const QPointF& position)
{
    send<wl_pointer_send_motion>(
        seat->timestamp(), wl_fixed_from_double(position.x()), wl_fixed_from_double(position.y()));
}

void Pointer::Private::sendFrame()
{
    send<wl_pointer_send_frame, WL_POINTER_FRAME_SINCE_VERSION>();
}

void Pointer::Private::registerRelativePointer(RelativePointerV1* relativePointer)
{
    relativePointers.push_back(relativePointer);

    QObject::connect(
        relativePointer, &RelativePointerV1::resourceDestroyed, handle(), [this, relativePointer] {
            relativePointers.erase(
                std::remove(relativePointers.begin(), relativePointers.end(), relativePointer),
                relativePointers.end());
        });
}

void Pointer::Private::registerSwipeGesture(PointerSwipeGestureV1* gesture)
{
    swipeGestures.push_back(gesture);
    QObject::connect(gesture, &PointerSwipeGestureV1::resourceDestroyed, handle(), [this, gesture] {
        swipeGestures.erase(std::remove(swipeGestures.begin(), swipeGestures.end(), gesture),
                            swipeGestures.end());
    });
}

void Pointer::Private::registerPinchGesture(PointerPinchGestureV1* gesture)
{
    pinchGestures.push_back(gesture);
    QObject::connect(gesture, &PointerPinchGestureV1::resourceDestroyed, handle(), [this, gesture] {
        pinchGestures.erase(std::remove(pinchGestures.begin(), pinchGestures.end(), gesture),
                            pinchGestures.end());
    });
}

void Pointer::Private::startSwipeGesture(quint32 serial, quint32 fingerCount)
{
    if (swipeGestures.empty()) {
        return;
    }
    for (auto gesture : swipeGestures) {
        gesture->start(serial, fingerCount);
    }
}

void Pointer::Private::updateSwipeGesture(const QSizeF& delta)
{
    if (swipeGestures.empty()) {
        return;
    }
    for (auto gesture : swipeGestures) {
        gesture->update(delta);
    }
}

void Pointer::Private::endSwipeGesture(quint32 serial)
{
    if (swipeGestures.empty()) {
        return;
    }
    for (auto gesture : swipeGestures) {
        gesture->end(serial);
    }
}

void Pointer::Private::cancelSwipeGesture(quint32 serial)
{
    if (swipeGestures.empty()) {
        return;
    }
    for (auto gesture : swipeGestures) {
        gesture->cancel(serial);
    }
}

void Pointer::Private::startPinchGesture(quint32 serial, quint32 fingerCount)
{
    if (pinchGestures.empty()) {
        return;
    }
    for (auto gesture : pinchGestures) {
        gesture->start(serial, fingerCount);
    }
}

void Pointer::Private::updatePinchGesture(const QSizeF& delta, qreal scale, qreal rotation)
{
    if (pinchGestures.empty()) {
        return;
    }
    for (auto gesture : pinchGestures) {
        gesture->update(delta, scale, rotation);
    }
}

void Pointer::Private::endPinchGesture(quint32 serial)
{
    if (pinchGestures.empty()) {
        return;
    }
    for (auto gesture : pinchGestures) {
        gesture->end(serial);
    }
}

void Pointer::Private::cancelPinchGesture(quint32 serial)
{
    if (pinchGestures.empty()) {
        return;
    }
    for (auto gesture : pinchGestures) {
        gesture->cancel(serial);
    }
}

void Pointer::Private::setFocusedSurface(quint32 serial, Surface* surface)
{
    sendLeave(serial, focusedSurface);
    disconnect(surfaceDestroyConnection);
    disconnect(clientDestroyConnection);

    if (!surface) {
        focusedSurface = nullptr;
        return;
    }

    focusedSurface = surface;
    surfaceDestroyConnection
        = connect(focusedSurface, &Surface::resourceDestroyed, handle(), [this] {
              disconnect(clientDestroyConnection);
              sendLeave(client()->display()->handle()->nextSerial(), focusedSurface);
              sendFrame();
              focusedSurface = nullptr;
          });
    clientDestroyConnection = connect(client()->handle(), &Client::disconnected, handle(), [this] {
        disconnect(surfaceDestroyConnection);
        focusedSurface = nullptr;
    });

    const QPointF pos = seat->focusedPointerSurfaceTransformation().map(seat->pointerPos());
    sendEnter(serial, focusedSurface, pos);
    client()->flush();
}

Pointer::Pointer(Client* client, uint32_t version, uint32_t id, Seat* seat)
    : QObject(nullptr)
    , d_ptr(new Private(client, version, id, seat, this))
{
}

void Pointer::setFocusedSurface(quint32 serial, Surface* surface)
{
    d_ptr->setFocusedSurface(serial, surface);
}

void Pointer::buttonPressed(quint32 serial, quint32 button)
{
    Q_ASSERT(d_ptr->focusedSurface);

    d_ptr->send<wl_pointer_send_button>(
        serial, d_ptr->seat->timestamp(), button, WL_POINTER_BUTTON_STATE_PRESSED);
    d_ptr->sendFrame();
}

void Pointer::buttonReleased(quint32 serial, quint32 button)
{
    Q_ASSERT(d_ptr->focusedSurface);

    d_ptr->send<wl_pointer_send_button>(
        serial, d_ptr->seat->timestamp(), button, WL_POINTER_BUTTON_STATE_RELEASED);
    d_ptr->sendFrame();
}

void Pointer::axis(Qt::Orientation orientation,
                   qreal delta,
                   qint32 discreteDelta,
                   PointerAxisSource source)
{
    Q_ASSERT(d_ptr->focusedSurface);

    const auto wlOrientation = (orientation == Qt::Vertical) ? WL_POINTER_AXIS_VERTICAL_SCROLL
                                                             : WL_POINTER_AXIS_HORIZONTAL_SCROLL;

    auto getWlSource = [source] {
        wl_pointer_axis_source wlSource = WL_POINTER_AXIS_SOURCE_WHEEL;
        switch (source) {
        case PointerAxisSource::Wheel:
            wlSource = WL_POINTER_AXIS_SOURCE_WHEEL;
            break;
        case PointerAxisSource::Finger:
            wlSource = WL_POINTER_AXIS_SOURCE_FINGER;
            break;
        case PointerAxisSource::Continuous:
            wlSource = WL_POINTER_AXIS_SOURCE_CONTINUOUS;
            break;
        case PointerAxisSource::WheelTilt:
            wlSource = WL_POINTER_AXIS_SOURCE_WHEEL_TILT;
            break;
        default:
            Q_UNREACHABLE();
            break;
        }
        return wlSource;
    };

    if (source != PointerAxisSource::Unknown) {
        d_ptr->send<wl_pointer_send_axis_source, WL_POINTER_AXIS_SOURCE_SINCE_VERSION>(
            getWlSource());
    }

    if (delta != 0.0) {
        if (discreteDelta) {
            d_ptr->send<wl_pointer_send_axis_discrete, WL_POINTER_AXIS_DISCRETE_SINCE_VERSION>(
                wlOrientation, discreteDelta);
        }
        d_ptr->send<wl_pointer_send_axis>(
            d_ptr->seat->timestamp(), wlOrientation, wl_fixed_from_double(delta));
    } else {
        d_ptr->send<wl_pointer_send_axis_stop, WL_POINTER_AXIS_STOP_SINCE_VERSION>(
            d_ptr->seat->timestamp(), wlOrientation);
    }

    d_ptr->sendFrame();
}

// TODO(romangg): Remove this legacy function.
void Pointer::axis(Qt::Orientation orientation, quint32 delta)
{
    Q_ASSERT(d_ptr->focusedSurface);
    auto const wlorient = (orientation == Qt::Vertical) ? WL_POINTER_AXIS_VERTICAL_SCROLL
                                                        : WL_POINTER_AXIS_HORIZONTAL_SCROLL;
    d_ptr->send<wl_pointer_send_axis>(
        d_ptr->seat->timestamp(), wlorient, wl_fixed_from_int(static_cast<int>(delta)));
    d_ptr->sendFrame();
}

Client* Pointer::client() const
{
    return d_ptr->client()->handle();
}

Seat* Pointer::seat() const
{
    return d_ptr->seat;
}

Cursor* Pointer::cursor() const
{
    return d_ptr->cursor.get();
}

void Pointer::relativeMotion(const QSizeF& delta,
                             const QSizeF& deltaNonAccelerated,
                             quint64 microseconds)
{
    if (d_ptr->relativePointers.empty()) {
        return;
    }
    for (auto relativePointer : d_ptr->relativePointers) {
        relativePointer->relativeMotion(microseconds, delta, deltaNonAccelerated);
    }
    d_ptr->sendFrame();
}

Pointer* Pointer::get(void* data)
{
    return Private::handle(reinterpret_cast<wl_resource*>(data));
}

Cursor::Private::Private(Cursor* q, Pointer* _pointer)
    : pointer(_pointer)
    , q_ptr(q)
{
}

void Cursor::Private::update(const QPointer<Surface>& s, quint32 serial, const QPoint& _hotspot)
{
    bool emitChanged = false;
    if (enteredSerial != serial) {
        enteredSerial = serial;
        emitChanged = true;
        Q_EMIT q_ptr->enteredSerialChanged();
    }
    if (hotspot != _hotspot) {
        hotspot = _hotspot;
        emitChanged = true;
        Q_EMIT q_ptr->hotspotChanged();
    }
    if (surface != s) {
        if (!surface.isNull()) {
            QObject::disconnect(surface.data(), &Surface::damaged, q_ptr, &Cursor::changed);
        }
        surface = s;
        if (!surface.isNull()) {
            QObject::connect(surface.data(), &Surface::damaged, q_ptr, &Cursor::changed);
        }
        emitChanged = true;
        Q_EMIT q_ptr->surfaceChanged();
    }
    if (emitChanged) {
        Q_EMIT q_ptr->changed();
    }
}

Cursor::Cursor(Pointer* pointer)
    : QObject(nullptr)
    , d_ptr(new Private(this, pointer))
{
}

Cursor::~Cursor() = default;

quint32 Cursor::enteredSerial() const
{
    return d_ptr->enteredSerial;
}

QPoint Cursor::hotspot() const
{
    return d_ptr->hotspot;
}

Pointer* Cursor::pointer() const
{
    return d_ptr->pointer;
}

QPointer<Surface> Cursor::surface() const
{
    return d_ptr->surface;
}

}
