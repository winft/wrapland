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
#include "touch.h"
#include "surface.h"
#include "wayland_pointer_p.h"
// Qt
#include <QPointF>
#include <QPointer>
#include <QVector>
// wayland
#include <wayland-client-protocol.h>

namespace Wrapland
{
namespace Client
{

class Q_DECL_HIDDEN Touch::Private
{
public:
    Private(Touch* q);
    void setup(wl_touch* t);
    WaylandPointer<wl_touch, wl_touch_release> touch;
    bool active = false;
    QVector<TouchPoint*> sequence;
    TouchPoint* getActivePoint(qint32 id) const;

private:
    static void downCallback(void* data,
                             wl_touch* touch,
                             uint32_t serial,
                             uint32_t time,
                             wl_surface* surface,
                             int32_t id,
                             wl_fixed_t x,
                             wl_fixed_t y);
    static void upCallback(void* data, wl_touch* touch, uint32_t serial, uint32_t time, int32_t id);
    static void motionCallback(void* data,
                               wl_touch* touch,
                               uint32_t time,
                               int32_t id,
                               wl_fixed_t x,
                               wl_fixed_t y);
    static void frameCallback(void* data, wl_touch* touch);
    static void cancelCallback(void* data, wl_touch* touch);
    void down(quint32 serial,
              quint32 time,
              qint32 id,
              QPointF const& position,
              QPointer<Surface> const& surface);
    void up(quint32 serial, quint32 time, qint32 id);
    void motion(quint32 time, qint32 id, QPointF const& position);

    Touch* q;
    static wl_touch_listener const s_listener;
};

class TouchPoint::Private
{
public:
    qint32 id = 0;
    quint32 downSerial = 0;
    quint32 upSerial = 0;
    QPointer<Surface> surface;
    QVector<QPointF> positions;
    QVector<quint32> timestamps;
    bool down = true;
};

TouchPoint::TouchPoint()
    : d(new Private)
{
}

TouchPoint::~TouchPoint() = default;

QPointF TouchPoint::position() const
{
    if (d->positions.isEmpty()) {
        return QPointF();
    }
    return d->positions.last();
}

QVector<QPointF> TouchPoint::positions() const
{
    return d->positions;
}

quint32 TouchPoint::downSerial() const
{
    return d->downSerial;
}

quint32 TouchPoint::upSerial() const
{
    return d->upSerial;
}

QPointer<Surface> TouchPoint::surface() const
{
    return d->surface;
}

quint32 TouchPoint::time() const
{
    if (d->timestamps.isEmpty()) {
        return 0;
    }
    return d->timestamps.last();
}

QVector<quint32> TouchPoint::timestamps() const
{
    return d->timestamps;
}

bool TouchPoint::isDown() const
{
    return d->down;
}

qint32 TouchPoint::id() const
{
    return d->id;
}

Touch::Private::Private(Touch* q)
    : q(q)
{
}

void Touch::Private::setup(wl_touch* t)
{
    Q_ASSERT(t);
    Q_ASSERT(!touch);
    touch.setup(t);
    wl_touch_add_listener(touch, &s_listener, this);
}

wl_touch_listener const Touch::Private::s_listener = {
    downCallback,
    upCallback,
    motionCallback,
    frameCallback,
    cancelCallback,
};

void Touch::Private::downCallback(void* data,
                                  wl_touch* touch,
                                  uint32_t serial,
                                  uint32_t time,
                                  wl_surface* surface,
                                  int32_t id,
                                  wl_fixed_t x,
                                  wl_fixed_t y)
{
    auto t = reinterpret_cast<Touch::Private*>(data);
    Q_ASSERT(t->touch == touch);
    t->down(serial,
            time,
            id,
            QPointF(wl_fixed_to_double(x), wl_fixed_to_double(y)),
            QPointer<Surface>(Surface::get(surface)));
}

void Touch::Private::down(quint32 serial,
                          quint32 time,
                          qint32 id,
                          QPointF const& position,
                          QPointer<Surface> const& surface)
{
    TouchPoint* p = new TouchPoint;
    p->d->downSerial = serial;
    p->d->surface = surface;
    p->d->id = id;
    p->d->positions << position;
    p->d->timestamps << time;
    if (active) {
        sequence << p;
        Q_EMIT q->pointAdded(p);
    } else {
        qDeleteAll(sequence);
        sequence.clear();
        sequence << p;
        active = true;
        Q_EMIT q->sequenceStarted(p);
    }
}

TouchPoint* Touch::Private::getActivePoint(qint32 id) const
{
    auto it = std::find_if(sequence.constBegin(), sequence.constEnd(), [id](TouchPoint* p) {
        return p->id() == id && p->isDown();
    });
    if (it == sequence.constEnd()) {
        return nullptr;
    }
    return *it;
}

void Touch::Private::upCallback(void* data,
                                wl_touch* touch,
                                uint32_t serial,
                                uint32_t time,
                                int32_t id)
{
    auto t = reinterpret_cast<Touch::Private*>(data);
    Q_ASSERT(t->touch == touch);
    t->up(serial, time, id);
}

void Touch::Private::up(quint32 serial, quint32 time, qint32 id)
{
    TouchPoint* p = getActivePoint(id);
    if (!p) {
        return;
    }
    p->d->timestamps << time;
    p->d->upSerial = serial;
    p->d->down = false;
    Q_EMIT q->pointRemoved(p);
    // check whether the sequence ended
    for (auto it = sequence.constBegin(); it != sequence.constEnd(); ++it) {
        if ((*it)->isDown()) {
            return;
        }
    }
    // no touch point is down
    active = false;
    Q_EMIT q->sequenceEnded();
}

void Touch::Private::motionCallback(void* data,
                                    wl_touch* touch,
                                    uint32_t time,
                                    int32_t id,
                                    wl_fixed_t x,
                                    wl_fixed_t y)
{
    auto t = reinterpret_cast<Touch::Private*>(data);
    Q_ASSERT(t->touch == touch);
    t->motion(time, id, QPointF(wl_fixed_to_double(x), wl_fixed_to_double(y)));
}

void Touch::Private::motion(quint32 time, qint32 id, QPointF const& position)
{
    TouchPoint* p = getActivePoint(id);
    if (!p) {
        return;
    }
    p->d->positions << position;
    p->d->timestamps << time;
    Q_EMIT q->pointMoved(p);
}

void Touch::Private::frameCallback(void* data, wl_touch* touch)
{
    auto t = reinterpret_cast<Touch::Private*>(data);
    Q_ASSERT(t->touch == touch);
    Q_EMIT t->q->frameEnded();
}

void Touch::Private::cancelCallback(void* data, wl_touch* touch)
{
    auto t = reinterpret_cast<Touch::Private*>(data);
    Q_ASSERT(t->touch == touch);
    t->active = false;
    Q_EMIT t->q->sequenceCanceled();
}

Touch::Touch(QObject* parent)
    : QObject(parent)
    , d(new Private(this))
{
}

Touch::~Touch()
{
    qDeleteAll(d->sequence);
    d->sequence.clear();
    release();
}

void Touch::release()
{
    d->touch.release();
}

void Touch::setup(wl_touch* touch)
{
    d->setup(touch);
}

bool Touch::isValid() const
{
    return d->touch.isValid();
}

Touch::operator wl_touch*() const
{
    return d->touch;
}

Touch::operator wl_touch*()
{
    return d->touch;
}

QVector<TouchPoint*> Touch::sequence() const
{
    return d->sequence;
}

}
}
