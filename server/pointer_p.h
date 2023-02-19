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

#include "pointer.h"

#include "wayland/resource.h"

#include <QPoint>

namespace Wrapland::Server
{
class PointerPinchGestureV1;
class PointerSwipeGestureV1;
class PointerHoldGestureV1;
class RelativePointerV1;

class Cursor::Private
{
public:
    Private(Cursor* q_ptr, Pointer* _pointer);

    Pointer* pointer;
    quint32 enteredSerial = 0;
    QPoint hotspot;
    Surface* surface{nullptr};

    void update(Surface* surface, quint32 serial, QPoint const& _hotspot);

private:
    struct {
        QMetaObject::Connection commit;
        QMetaObject::Connection destroy;
    } surface_notifiers;

    Cursor* q_ptr;
};

class Pointer::Private : public Wayland::Resource<Pointer>
{
public:
    Private(Client* client, uint32_t version, uint32_t id, Seat* _seat, Pointer* q_ptr);

    Seat* seat;

    Surface* focusedSurface = nullptr;
    QMetaObject::Connection surfaceDestroyConnection;
    QMetaObject::Connection clientDestroyConnection;
    std::unique_ptr<Cursor> cursor;

    std::vector<RelativePointerV1*> relativePointers;
    std::vector<PointerSwipeGestureV1*> swipeGestures;
    std::vector<PointerPinchGestureV1*> pinchGestures;
    std::vector<PointerHoldGestureV1*> holdGestures;

    void sendEnter(quint32 serial, Surface* surface, QPointF const& pos);
    void sendLeave(quint32 serial, Surface* surface);
    void sendMotion(QPointF const& position);
    void sendFrame();

    void registerRelativePointer(RelativePointerV1* relativePointer);
    void registerSwipeGesture(PointerSwipeGestureV1* gesture);
    void registerPinchGesture(PointerPinchGestureV1* gesture);
    void registerHoldGesture(PointerHoldGestureV1* gesture);

    void startSwipeGesture(quint32 serial, quint32 fingerCount);
    void updateSwipeGesture(QSizeF const& delta);
    void endSwipeGesture(quint32 serial);
    void cancelSwipeGesture(quint32 serial);

    void startPinchGesture(quint32 serial, quint32 fingerCount);
    void updatePinchGesture(QSizeF const& delta, qreal scale, qreal rotation);
    void endPinchGesture(quint32 serial);
    void cancelPinchGesture(quint32 serial);

    void startHoldGesture(quint32 serial, quint32 fingerCount);
    void endHoldGesture(quint32 serial);
    void cancelHoldGesture(quint32 serial);

    void setFocusedSurface(quint32 serial, Surface* surface);

private:
    static void setCursorCallback(wl_client* wlClient,
                                  wl_resource* wlResource,
                                  uint32_t serial,
                                  wl_resource* wlSurface,
                                  int32_t hotspot_x,
                                  int32_t hotspot_y);
    void setCursor(quint32 serial, Surface* surface, QPoint const& hotspot);

    static const struct wl_pointer_interface s_interface;
};

}
