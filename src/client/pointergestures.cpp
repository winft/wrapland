/****************************************************************************
Copyright 2016  Martin Gräßlin <mgraesslin@kde.org>

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
#include "pointergestures.h"
#include "event_queue.h"
#include "pointer.h"
#include "surface.h"
#include "wayland_pointer_p.h"

#include <wayland-pointer-gestures-unstable-v1-client-protocol.h>

#include <QSizeF>

namespace Wrapland
{
namespace Client
{

class Q_DECL_HIDDEN PointerGestures::Private
{
public:
    Private() = default;

    WaylandPointer<zwp_pointer_gestures_v1, zwp_pointer_gestures_v1_destroy> pointergestures;
    EventQueue* queue = nullptr;
};

PointerGestures::PointerGestures(QObject* parent)
    : QObject(parent)
    , d(new Private)
{
}

PointerGestures::~PointerGestures()
{
    release();
}

void PointerGestures::setup(zwp_pointer_gestures_v1* pointergestures)
{
    Q_ASSERT(pointergestures);
    Q_ASSERT(!d->pointergestures);
    d->pointergestures.setup(pointergestures);
}

void PointerGestures::release()
{
    d->pointergestures.release();
}

PointerGestures::operator zwp_pointer_gestures_v1*()
{
    return d->pointergestures;
}

PointerGestures::operator zwp_pointer_gestures_v1*() const
{
    return d->pointergestures;
}

bool PointerGestures::isValid() const
{
    return d->pointergestures.isValid();
}

void PointerGestures::setEventQueue(EventQueue* queue)
{
    d->queue = queue;
}

EventQueue* PointerGestures::eventQueue()
{
    return d->queue;
}

PointerSwipeGesture* PointerGestures::createSwipeGesture(Pointer* pointer, QObject* parent)
{
    Q_ASSERT(isValid());
    PointerSwipeGesture* p = new PointerSwipeGesture(parent);
    auto w = zwp_pointer_gestures_v1_get_swipe_gesture(d->pointergestures, *pointer);
    if (d->queue) {
        d->queue->addProxy(w);
    }
    p->setup(w);
    return p;
}

PointerPinchGesture* PointerGestures::createPinchGesture(Pointer* pointer, QObject* parent)
{
    Q_ASSERT(isValid());
    PointerPinchGesture* p = new PointerPinchGesture(parent);
    auto w = zwp_pointer_gestures_v1_get_pinch_gesture(d->pointergestures, *pointer);
    if (d->queue) {
        d->queue->addProxy(w);
    }
    p->setup(w);
    return p;
}

pointer_hold_gesture* PointerGestures::create_hold_gesture(Pointer* pointer, QObject* parent)
{
    Q_ASSERT(isValid());
    auto gesture = new pointer_hold_gesture(parent);
    auto w = zwp_pointer_gestures_v1_get_hold_gesture(d->pointergestures, *pointer);
    if (d->queue) {
        d->queue->addProxy(w);
    }
    gesture->setup(w);
    return gesture;
}

class Q_DECL_HIDDEN PointerSwipeGesture::Private
{
public:
    Private(PointerSwipeGesture* q);

    void setup(zwp_pointer_gesture_swipe_v1* pg);

    WaylandPointer<zwp_pointer_gesture_swipe_v1, zwp_pointer_gesture_swipe_v1_destroy>
        pointerswipegesture;
    quint32 fingerCount = 0;
    QPointer<Surface> surface;

private:
    static void beginCallback(void* data,
                              zwp_pointer_gesture_swipe_v1* zwp_pointer_gesture_swipe_v1,
                              uint32_t serial,
                              uint32_t time,
                              wl_surface* surface,
                              uint32_t fingers);
    static void updateCallback(void* data,
                               zwp_pointer_gesture_swipe_v1* zwp_pointer_gesture_swipe_v1,
                               uint32_t time,
                               wl_fixed_t dx,
                               wl_fixed_t dy);
    static void endCallback(void* data,
                            zwp_pointer_gesture_swipe_v1* zwp_pointer_gesture_swipe_v1,
                            uint32_t serial,
                            uint32_t time,
                            int32_t cancelled);

    PointerSwipeGesture* q;
    static zwp_pointer_gesture_swipe_v1_listener const s_listener;
};

zwp_pointer_gesture_swipe_v1_listener const PointerSwipeGesture::Private::s_listener = {
    beginCallback,
    updateCallback,
    endCallback,
};

void PointerSwipeGesture::Private::beginCallback(
    void* data,
    zwp_pointer_gesture_swipe_v1* zwp_pointer_gesture_swipe_v1,
    uint32_t serial,
    uint32_t time,
    wl_surface* surface,
    uint32_t fingers)
{
    auto p = reinterpret_cast<PointerSwipeGesture::Private*>(data);
    Q_ASSERT(p->pointerswipegesture == zwp_pointer_gesture_swipe_v1);
    p->fingerCount = fingers;
    p->surface = QPointer<Surface>(Surface::get(surface));
    Q_EMIT p->q->started(serial, time);
}

void PointerSwipeGesture::Private::updateCallback(
    void* data,
    zwp_pointer_gesture_swipe_v1* zwp_pointer_gesture_swipe_v1,
    uint32_t time,
    wl_fixed_t dx,
    wl_fixed_t dy)
{
    auto p = reinterpret_cast<PointerSwipeGesture::Private*>(data);
    Q_ASSERT(p->pointerswipegesture == zwp_pointer_gesture_swipe_v1);
    Q_EMIT p->q->updated(QSizeF(wl_fixed_to_double(dx), wl_fixed_to_double(dy)), time);
}

void PointerSwipeGesture::Private::endCallback(
    void* data,
    zwp_pointer_gesture_swipe_v1* zwp_pointer_gesture_swipe_v1,
    uint32_t serial,
    uint32_t time,
    int32_t cancelled)
{
    auto p = reinterpret_cast<PointerSwipeGesture::Private*>(data);
    Q_ASSERT(p->pointerswipegesture == zwp_pointer_gesture_swipe_v1);
    if (cancelled) {
        Q_EMIT p->q->cancelled(serial, time);
    } else {
        Q_EMIT p->q->ended(serial, time);
    }
    p->fingerCount = 0;
    p->surface.clear();
}

PointerSwipeGesture::Private::Private(PointerSwipeGesture* q)
    : q(q)
{
}

PointerSwipeGesture::PointerSwipeGesture(QObject* parent)
    : QObject(parent)
    , d(new Private(this))
{
}

PointerSwipeGesture::~PointerSwipeGesture()
{
    release();
}

quint32 PointerSwipeGesture::fingerCount() const
{
    return d->fingerCount;
}

QPointer<Surface> PointerSwipeGesture::surface() const
{
    return d->surface;
}

void PointerSwipeGesture::Private::setup(zwp_pointer_gesture_swipe_v1* pg)
{
    Q_ASSERT(pg);
    Q_ASSERT(!pointerswipegesture);
    pointerswipegesture.setup(pg);
    zwp_pointer_gesture_swipe_v1_add_listener(pointerswipegesture, &s_listener, this);
}

void PointerSwipeGesture::setup(zwp_pointer_gesture_swipe_v1* pointerswipegesture)
{
    d->setup(pointerswipegesture);
}

void PointerSwipeGesture::release()
{
    d->pointerswipegesture.release();
}

PointerSwipeGesture::operator zwp_pointer_gesture_swipe_v1*()
{
    return d->pointerswipegesture;
}

PointerSwipeGesture::operator zwp_pointer_gesture_swipe_v1*() const
{
    return d->pointerswipegesture;
}

bool PointerSwipeGesture::isValid() const
{
    return d->pointerswipegesture.isValid();
}

class Q_DECL_HIDDEN PointerPinchGesture::Private
{
public:
    Private(PointerPinchGesture* q);

    void setup(zwp_pointer_gesture_pinch_v1* pg);

    WaylandPointer<zwp_pointer_gesture_pinch_v1, zwp_pointer_gesture_pinch_v1_destroy>
        pointerpinchgesture;
    quint32 fingerCount = 0;
    QPointer<Surface> surface;

private:
    static void beginCallback(void* data,
                              zwp_pointer_gesture_pinch_v1* zwp_pointer_gesture_pinch_v1,
                              uint32_t serial,
                              uint32_t time,
                              wl_surface* surface,
                              uint32_t fingers);
    static void updateCallback(void* data,
                               zwp_pointer_gesture_pinch_v1* zwp_pointer_gesture_pinch_v1,
                               uint32_t time,
                               wl_fixed_t dx,
                               wl_fixed_t dy,
                               wl_fixed_t scale,
                               wl_fixed_t rotation);
    static void endCallback(void* data,
                            zwp_pointer_gesture_pinch_v1* zwp_pointer_gesture_pinch_v1,
                            uint32_t serial,
                            uint32_t time,
                            int32_t cancelled);

    PointerPinchGesture* q;
    static zwp_pointer_gesture_pinch_v1_listener const s_listener;
};

zwp_pointer_gesture_pinch_v1_listener const PointerPinchGesture::Private::s_listener = {
    beginCallback,
    updateCallback,
    endCallback,
};

void PointerPinchGesture::Private::beginCallback(void* data,
                                                 zwp_pointer_gesture_pinch_v1* pg,
                                                 uint32_t serial,
                                                 uint32_t time,
                                                 wl_surface* surface,
                                                 uint32_t fingers)
{
    auto p = reinterpret_cast<PointerPinchGesture::Private*>(data);
    Q_ASSERT(p->pointerpinchgesture == pg);
    p->fingerCount = fingers;
    p->surface = QPointer<Surface>(Surface::get(surface));
    Q_EMIT p->q->started(serial, time);
}

void PointerPinchGesture::Private::updateCallback(void* data,
                                                  zwp_pointer_gesture_pinch_v1* pg,
                                                  uint32_t time,
                                                  wl_fixed_t dx,
                                                  wl_fixed_t dy,
                                                  wl_fixed_t scale,
                                                  wl_fixed_t rotation)
{
    auto p = reinterpret_cast<PointerPinchGesture::Private*>(data);
    Q_ASSERT(p->pointerpinchgesture == pg);
    Q_EMIT p->q->updated(QSizeF(wl_fixed_to_double(dx), wl_fixed_to_double(dy)),
                         wl_fixed_to_double(scale),
                         wl_fixed_to_double(rotation),
                         time);
}

void PointerPinchGesture::Private::endCallback(void* data,
                                               zwp_pointer_gesture_pinch_v1* pg,
                                               uint32_t serial,
                                               uint32_t time,
                                               int32_t cancelled)
{
    auto p = reinterpret_cast<PointerPinchGesture::Private*>(data);
    Q_ASSERT(p->pointerpinchgesture == pg);
    if (cancelled) {
        Q_EMIT p->q->cancelled(serial, time);
    } else {
        Q_EMIT p->q->ended(serial, time);
    }
    p->fingerCount = 0;
    p->surface.clear();
}

PointerPinchGesture::Private::Private(PointerPinchGesture* q)
    : q(q)
{
}

PointerPinchGesture::PointerPinchGesture(QObject* parent)
    : QObject(parent)
    , d(new Private(this))
{
}

PointerPinchGesture::~PointerPinchGesture()
{
    release();
}

void PointerPinchGesture::Private::setup(zwp_pointer_gesture_pinch_v1* pg)
{
    Q_ASSERT(pg);
    Q_ASSERT(!pointerpinchgesture);
    pointerpinchgesture.setup(pg);
    zwp_pointer_gesture_pinch_v1_add_listener(pointerpinchgesture, &s_listener, this);
}

void PointerPinchGesture::setup(zwp_pointer_gesture_pinch_v1* pointerpinchgesture)
{
    d->setup(pointerpinchgesture);
}

void PointerPinchGesture::release()
{
    d->pointerpinchgesture.release();
}

PointerPinchGesture::operator zwp_pointer_gesture_pinch_v1*()
{
    return d->pointerpinchgesture;
}

PointerPinchGesture::operator zwp_pointer_gesture_pinch_v1*() const
{
    return d->pointerpinchgesture;
}

bool PointerPinchGesture::isValid() const
{
    return d->pointerpinchgesture.isValid();
}

quint32 PointerPinchGesture::fingerCount() const
{
    return d->fingerCount;
}

QPointer<Surface> PointerPinchGesture::surface() const
{
    return d->surface;
}

class Q_DECL_HIDDEN pointer_hold_gesture::Private
{
public:
    Private(pointer_hold_gesture* q);

    void setup(zwp_pointer_gesture_hold_v1* pg);

    WaylandPointer<zwp_pointer_gesture_hold_v1, zwp_pointer_gesture_hold_v1_destroy> native;
    quint32 fingerCount = 0;
    QPointer<Surface> surface;

private:
    static void begin_callback(void* data,
                               zwp_pointer_gesture_hold_v1* zwp_pointer_gesture_hold_v1,
                               uint32_t serial,
                               uint32_t time,
                               wl_surface* surface,
                               uint32_t fingers);
    static void end_callback(void* data,
                             zwp_pointer_gesture_hold_v1* zwp_pointer_gesture_hold_v1,
                             uint32_t serial,
                             uint32_t time,
                             int32_t cancelled);

    pointer_hold_gesture* q;
    static zwp_pointer_gesture_hold_v1_listener const s_listener;
};

zwp_pointer_gesture_hold_v1_listener const pointer_hold_gesture::Private::s_listener = {
    begin_callback,
    end_callback,
};

void pointer_hold_gesture::Private::begin_callback(void* data,
                                                   zwp_pointer_gesture_hold_v1* pg,
                                                   uint32_t serial,
                                                   uint32_t time,
                                                   wl_surface* surface,
                                                   uint32_t fingers)
{
    auto p = reinterpret_cast<pointer_hold_gesture::Private*>(data);
    Q_ASSERT(p->native == pg);
    p->fingerCount = fingers;
    p->surface = QPointer<Surface>(Surface::get(surface));
    Q_EMIT p->q->started(serial, time);
}

void pointer_hold_gesture::Private::end_callback(void* data,
                                                 zwp_pointer_gesture_hold_v1* pg,
                                                 uint32_t serial,
                                                 uint32_t time,
                                                 int32_t cancelled)
{
    auto p = reinterpret_cast<pointer_hold_gesture::Private*>(data);
    Q_ASSERT(p->native == pg);
    if (cancelled) {
        Q_EMIT p->q->cancelled(serial, time);
    } else {
        Q_EMIT p->q->ended(serial, time);
    }
    p->fingerCount = 0;
    p->surface.clear();
}

pointer_hold_gesture::Private::Private(pointer_hold_gesture* q)
    : q(q)
{
}

pointer_hold_gesture::pointer_hold_gesture(QObject* parent)
    : QObject(parent)
    , d(new Private(this))
{
}

pointer_hold_gesture::~pointer_hold_gesture()
{
    release();
}

void pointer_hold_gesture::Private::setup(zwp_pointer_gesture_hold_v1* pg)
{
    Q_ASSERT(pg);
    Q_ASSERT(!native);
    native.setup(pg);
    zwp_pointer_gesture_hold_v1_add_listener(native, &s_listener, this);
}

void pointer_hold_gesture::setup(zwp_pointer_gesture_hold_v1* gesture)
{
    d->setup(gesture);
}

void pointer_hold_gesture::release()
{
    d->native.release();
}

pointer_hold_gesture::operator zwp_pointer_gesture_hold_v1*()
{
    return d->native;
}

pointer_hold_gesture::operator zwp_pointer_gesture_hold_v1*() const
{
    return d->native;
}

bool pointer_hold_gesture::isValid() const
{
    return d->native.isValid();
}

quint32 pointer_hold_gesture::fingerCount() const
{
    return d->fingerCount;
}

QPointer<Surface> pointer_hold_gesture::surface() const
{
    return d->surface;
}

}

}
