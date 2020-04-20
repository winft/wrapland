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
#include <QPointer>

namespace Wrapland
{
namespace Server
{
class PointerPinchGestureV1;
class PointerSwipeGestureV1;
class RelativePointerV1;

class Cursor::Private
{
public:
    Private(Cursor* q, Pointer* _pointer);

    Pointer* pointer;
    quint32 enteredSerial = 0;
    QPoint hotspot;
    QPointer<Surface> surface;

    void update(const QPointer<Surface>& surface, quint32 serial, const QPoint& hotspot);

private:
    Cursor* q_ptr;
};

class Pointer::Private : public Wayland::Resource<Pointer>
{
public:
    Private(Client* client, uint32_t version, uint32_t id, Seat* _seat, Pointer* q);

    Seat* seat;

    Surface* focusedSurface = nullptr;
    QPointer<Surface> focusedChildSurface;
    QMetaObject::Connection destroyConnection;
    Cursor* cursor = nullptr;

    std::vector<RelativePointerV1*> relativePointers;
    std::vector<PointerSwipeGestureV1*> swipeGestures;
    std::vector<PointerPinchGestureV1*> pinchGestures;

    void sendEnter(quint32 serial, Surface* surface, const QPointF& parentSurfacePosition);
    void sendLeave(quint32 serial, Surface* surface);
    void sendMotion(const QPointF& position);
    void sendAxis(Qt::Orientation orientation, quint32 delta);
    void sendFrame();

    void registerRelativePointer(RelativePointerV1* relativePointer);
    void registerSwipeGesture(PointerSwipeGestureV1* gesture);
    void registerPinchGesture(PointerPinchGestureV1* gesture);

    void startSwipeGesture(quint32 serial, quint32 fingerCount);
    void updateSwipeGesture(const QSizeF& delta);
    void endSwipeGesture(quint32 serial);
    void cancelSwipeGesture(quint32 serial);

    void startPinchGesture(quint32 serial, quint32 fingerCount);
    void updatePinchGesture(const QSizeF& delta, qreal scale, qreal rotation);
    void endPinchGesture(quint32 serial);
    void cancelPinchGesture(quint32 serial);

    Pointer* q_ptr;

private:
    static void setCursorCallback(wl_client* wlClient,
                                  wl_resource* wlResource,
                                  uint32_t serial,
                                  wl_resource* wlSurface,
                                  int32_t hotspot_x,
                                  int32_t hotspot_y);
    void setCursor(quint32 serial, Surface* surface, const QPoint& hotspot);

    static const struct wl_pointer_interface s_interface;
};

}
}
