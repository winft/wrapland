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
#include "viewporter.h"

#include "event_queue.h"
#include "surface.h"
#include "wayland_pointer_p.h"

#include <wayland-util.h>
#include <wayland-viewporter-client-protocol.h>

namespace Wrapland
{

namespace Client
{

class Q_DECL_HIDDEN Viewporter::Private
{
public:
    Private() = default;

    WaylandPointer<wp_viewporter, wp_viewporter_destroy> manager;
    EventQueue* queue = nullptr;
};

Viewporter::Viewporter(QObject* parent)
    : QObject(parent)
    , d(new Private)
{
}

Viewporter::~Viewporter()
{
    release();
}

void Viewporter::release()
{
    d->manager.release();
}

bool Viewporter::isValid() const
{
    return d->manager.isValid();
}

void Viewporter::setup(wp_viewporter* manager)
{
    Q_ASSERT(manager);
    Q_ASSERT(!d->manager);
    d->manager.setup(manager);
}

void Viewporter::setEventQueue(EventQueue* queue)
{
    d->queue = queue;
}

EventQueue* Viewporter::eventQueue()
{
    return d->queue;
}

Viewport* Viewporter::createViewport(Surface* surface, QObject* parent)
{
    Q_ASSERT(isValid());
    Viewport* vp = new Viewport(parent);
    auto w = wp_viewporter_get_viewport(d->manager, *surface);
    if (d->queue) {
        d->queue->addProxy(w);
    }
    vp->setup(w);
    return vp;
}

Viewporter::operator wp_viewporter*()
{
    return d->manager;
}

Viewporter::operator wp_viewporter*() const
{
    return d->manager;
}

class Viewport::Private
{
public:
    WaylandPointer<wp_viewport, wp_viewport_destroy> viewport;
};

Viewport::Viewport(QObject* parent)
    : QObject(parent)
    , d(new Private)
{
}

Viewport::~Viewport()
{
    release();
}

void Viewport::release()
{
    d->viewport.release();
}

void Viewport::setup(wp_viewport* viewport)
{
    Q_ASSERT(viewport);
    Q_ASSERT(!d->viewport);
    d->viewport.setup(viewport);
}

bool Viewport::isValid() const
{
    return d->viewport.isValid();
}

void Viewport::setSourceRectangle(QRectF const& source)
{
    Q_ASSERT(isValid());
    wp_viewport_set_source(d->viewport,
                           wl_fixed_from_double(source.x()),
                           wl_fixed_from_double(source.y()),
                           wl_fixed_from_double(source.width()),
                           wl_fixed_from_double(source.height()));
}

void Viewport::setDestinationSize(QSize const& dest)
{
    Q_ASSERT(isValid());
    wp_viewport_set_destination(d->viewport, dest.width(), dest.height());
}

Viewport::operator wp_viewport*()
{
    return d->viewport;
}

Viewport::operator wp_viewport*() const
{
    return d->viewport;
}

}
}
