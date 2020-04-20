/****************************************************************************
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
****************************************************************************/
#pragma once

#include "pointer_gestures_v1.h"

#include "wayland/global.h"

#include <wayland-pointer-gestures-unstable-v1-server-protocol.h>

namespace Wrapland
{
namespace Server
{

class Pointer;

class PointerGesturesV1::Private : public Wayland::Global<PointerGesturesV1>
{
public:
    Private(PointerGesturesV1* q, D_isplay* display);

private:
    static void swipeGestureCallback(wl_client* wlClient,
                                     wl_resource* wlResource,
                                     uint32_t id,
                                     wl_resource* wlPointer);
    static void pinchGestureCallback(wl_client* wlClient,
                                     wl_resource* wlResource,
                                     uint32_t id,
                                     wl_resource* wlPointer);

    PointerGesturesV1* q_ptr;

    static const struct zwp_pointer_gestures_v1_interface s_interface;
};

class PointerSwipeGestureV1 : public QObject
{
    Q_OBJECT
public:
    PointerSwipeGestureV1(Client* client, uint32_t version, uint32_t id, Pointer* pointer);
    ~PointerSwipeGestureV1() override;

    void start(quint32 serial, quint32 fingerCount);
    void update(const QSizeF& delta);
    void end(quint32 serial, bool cancel = false);
    void cancel(quint32 serial);

    Pointer* pointer;

Q_SIGNALS:
    void resourceDestroyed();

private:
    class Private;
    Private* d_ptr;
};

class PointerSwipeGestureV1::Private : public Wayland::Resource<PointerSwipeGestureV1>
{
public:
    Private(Client* client,
            uint32_t version,
            uint32_t id,
            Pointer* _pointer,
            PointerSwipeGestureV1* q);
    ~Private() override;

    Pointer* pointer;

private:
    static void swipeGestureCallback(wl_client* wlClient,
                                     wl_resource* wlResource,
                                     uint32_t id,
                                     wl_resource* wlPointer);
    static void pinchGestureCallback(wl_client* wlClient,
                                     wl_resource* wlResource,
                                     uint32_t id,
                                     wl_resource* wlPointer);

    static const struct zwp_pointer_gesture_swipe_v1_interface s_interface;
};

class PointerPinchGestureV1 : public QObject
{
    Q_OBJECT
public:
    PointerPinchGestureV1(Client* client, uint32_t version, uint32_t id, Pointer* pointer);
    ~PointerPinchGestureV1() override;

    void start(quint32 serial, quint32 fingerCount);
    void update(const QSizeF& delta, qreal scale, qreal rotation);
    void end(quint32 serial, bool cancel = false);
    void cancel(quint32 serial);

Q_SIGNALS:
    void resourceDestroyed();

private:
    class Private;
    Private* d_ptr;
};

class PointerPinchGestureV1::Private : public Wayland::Resource<PointerPinchGestureV1>
{
public:
    Private(Client* client,
            uint32_t version,
            uint32_t id,
            Pointer* pointer,
            PointerPinchGestureV1* q);
    ~Private() override;

    Pointer* pointer;

private:
    static const struct zwp_pointer_gesture_pinch_v1_interface s_interface;
};

}
}
