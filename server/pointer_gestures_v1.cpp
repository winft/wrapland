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
#include "pointer_gestures_v1.h"
#include "pointer_gestures_v1_p.h"

#include "display.h"
#include "pointer.h"
#include "pointer_p.h"
#include "seat.h"

#include "wayland/client.h"
#include "wayland/display.h"
#include "wayland/global.h"
#include "wayland/resource.h"

#include "surface_interface.h"

namespace Wrapland
{
namespace Server
{

PointerGesturesV1::Private::Private(PointerGesturesV1* q, D_isplay* display)
    : Wayland::Global<PointerGesturesV1>(q,
                                         display,
                                         &zwp_pointer_gestures_v1_interface,
                                         &s_interface)
    , q_ptr(q)
{
}

const struct zwp_pointer_gestures_v1_interface PointerGesturesV1::Private::s_interface = {
    swipeGestureCallback,
    pinchGestureCallback,
    resourceDestroyCallback,
};

void PointerGesturesV1::Private::swipeGestureCallback(wl_client* wlClient,
                                                      wl_resource* wlResource,
                                                      uint32_t id,
                                                      wl_resource* wlPointer)
{
    auto handle = fromResource(wlResource);
    auto client = handle->d_ptr->display()->handle()->getClient(wlClient);
    auto pointer = Wayland::Resource<Pointer>::fromResource(wlPointer);

    auto swiper
        = new PointerSwipeGestureV1(client, handle->d_ptr->version(), id, pointer->handle());
    if (!swiper) {
        return;
    }

    static_cast<Pointer::Private*>(pointer)->registerSwipeGesture(swiper);
}

void PointerGesturesV1::Private::pinchGestureCallback(wl_client* wlClient,
                                                      wl_resource* wlResource,
                                                      uint32_t id,
                                                      wl_resource* wlPointer)
{
    auto handle = fromResource(wlResource);
    auto client = handle->d_ptr->display()->handle()->getClient(wlClient);
    auto pointer = Wayland::Resource<Pointer>::fromResource(wlPointer);

    auto pincher
        = new PointerPinchGestureV1(client, handle->d_ptr->version(), id, pointer->handle());
    if (!pincher) {
        return;
    }

    static_cast<Pointer::Private*>(pointer)->registerPinchGesture(pincher);
}

PointerGesturesV1::PointerGesturesV1(D_isplay* display, QObject* parent)
    : QObject(parent)
    , d_ptr(new Private(this, display))
{
    d_ptr->create();
}

PointerGesturesV1::~PointerGesturesV1() = default;

PointerSwipeGestureV1::Private::Private(Client* client,
                                        uint32_t version,
                                        uint32_t id,
                                        Pointer* _pointer,
                                        PointerSwipeGestureV1* q)
    : Wayland::Resource<PointerSwipeGestureV1>(client,
                                               version,
                                               id,
                                               &zwp_pointer_gesture_swipe_v1_interface,
                                               &s_interface,
                                               q)
    , pointer(_pointer)
{
}

PointerSwipeGestureV1::Private::~Private() = default;

const struct zwp_pointer_gesture_swipe_v1_interface PointerSwipeGestureV1::Private::s_interface = {
    destroyCallback,
};

void PointerSwipeGestureV1::start(quint32 serial, quint32 fingerCount)
{
    auto seat = d_ptr->pointer->seat();

    d_ptr->send([serial, fingerCount, seat](wl_resource* wlResource) {
        zwp_pointer_gesture_swipe_v1_send_begin(wlResource,
                                                serial,
                                                seat->timestamp(),
                                                seat->focusedPointerSurface()->resource(),
                                                fingerCount);
    });
}

void PointerSwipeGestureV1::update(const QSizeF& delta)
{
    auto seat = d_ptr->pointer->seat();

    d_ptr->send([delta, seat](wl_resource* wlResource) {
        zwp_pointer_gesture_swipe_v1_send_update(wlResource,
                                                 seat->timestamp(),
                                                 wl_fixed_from_double(delta.width()),
                                                 wl_fixed_from_double(delta.height()));
    });
}

void PointerSwipeGestureV1::end(quint32 serial, bool cancel)
{
    auto seat = d_ptr->pointer->seat();

    d_ptr->send([serial, cancel, seat](wl_resource* wlResource) {
        zwp_pointer_gesture_swipe_v1_send_end(
            wlResource, serial, seat->timestamp(), uint32_t(cancel));
    });
}

void PointerSwipeGestureV1::cancel(quint32 serial)
{
    end(serial, true);
}

PointerSwipeGestureV1::PointerSwipeGestureV1(Client* client,
                                             uint32_t version,
                                             uint32_t id,
                                             Pointer* pointer)
    : d_ptr(new Private(client, version, id, pointer, this))
{
}

PointerSwipeGestureV1::~PointerSwipeGestureV1() = default;

PointerPinchGestureV1::Private::Private(Client* client,
                                        uint32_t version,
                                        uint32_t id,
                                        Pointer* _pointer,
                                        PointerPinchGestureV1* q)
    : Wayland::Resource<PointerPinchGestureV1>(client,
                                               version,
                                               id,
                                               &zwp_pointer_gesture_pinch_v1_interface,
                                               &s_interface,
                                               q)
    , pointer(_pointer)
{
}

PointerPinchGestureV1::Private::~Private() = default;

const struct zwp_pointer_gesture_pinch_v1_interface PointerPinchGestureV1::Private::s_interface = {
    destroyCallback,
};

PointerPinchGestureV1::PointerPinchGestureV1(Client* client,
                                             uint32_t version,
                                             uint32_t id,
                                             Pointer* pointer)
    : d_ptr(new Private(client, version, id, pointer, this))
{
}

PointerPinchGestureV1::~PointerPinchGestureV1() = default;

void PointerPinchGestureV1::start(quint32 serial, quint32 fingerCount)
{
    auto seat = d_ptr->pointer->seat();

    d_ptr->send([serial, fingerCount, seat](wl_resource* wlResource) {
        zwp_pointer_gesture_pinch_v1_send_begin(wlResource,
                                                serial,
                                                seat->timestamp(),
                                                seat->focusedPointerSurface()->resource(),
                                                fingerCount);
    });
}

void PointerPinchGestureV1::update(const QSizeF& delta, qreal scale, qreal rotation)
{
    auto seat = d_ptr->pointer->seat();

    d_ptr->send([delta, scale, rotation, seat](wl_resource* wlResource) {
        zwp_pointer_gesture_pinch_v1_send_update(wlResource,
                                                 seat->timestamp(),
                                                 wl_fixed_from_double(delta.width()),
                                                 wl_fixed_from_double(delta.height()),
                                                 wl_fixed_from_double(scale),
                                                 wl_fixed_from_double(rotation));
    });
}

void PointerPinchGestureV1::end(quint32 serial, bool cancel)
{
    auto seat = d_ptr->pointer->seat();

    d_ptr->send([serial, cancel, seat](wl_resource* wlResource) {
        zwp_pointer_gesture_pinch_v1_send_end(
            wlResource, serial, seat->timestamp(), uint32_t(cancel));
    });
}

void PointerPinchGestureV1::cancel(quint32 serial)
{
    end(serial, true);
}

}
}
