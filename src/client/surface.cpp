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
#include "surface.h"
#include "buffer.h"
#include "output.h"
#include "region.h"
#include "wayland_pointer_p.h"

#include <QGuiApplication>
#include <QRegion>
#include <QVector>
#include <QWindow>
#include <qpa/qplatformnativeinterface.h>
// Wayland
#include <wayland-client-protocol.h>

namespace Wrapland
{
namespace Client
{

class Q_DECL_HIDDEN Surface::Private
{
public:
    Private(Surface* q);

    void setup(wl_surface* s);
    void setupFrameCallback();

    WaylandPointer<wl_surface, wl_surface_destroy> surface;

    QSize size;
    bool foreign = false;
    qint32 scale = 1;

    wl_callback* pendingFrameCallback = nullptr;
    QVector<Output*> outputs;

    static QList<Surface*> s_surfaces;

private:
    void handleFrameCallback();
    static void frameCallback(void* data, wl_callback* callback, uint32_t time);
    static void enterCallback(void* data, wl_surface* wl_surface, wl_output* output);
    static void leaveCallback(void* data, wl_surface* wl_surface, wl_output* output);

    Surface* q;
    static wl_callback_listener const s_listener;
    static wl_surface_listener const s_surfaceListener;
};

QList<Surface*> Surface::Private::s_surfaces = QList<Surface*>();

Surface::Private::Private(Surface* q)
    : q(q)
{
}

Surface::Surface(QObject* parent)
    : QObject(parent)
    , d(new Private(this))
{
    Private::s_surfaces << this;
}

Surface::~Surface()
{
    Private::s_surfaces.removeAll(this);
    release();
}

Surface* Surface::fromWindow(QWindow* window)
{
    if (!window) {
        return nullptr;
    }
    QPlatformNativeInterface* native = qApp->platformNativeInterface();
    if (!native) {
        return nullptr;
    }
    window->create();
    wl_surface* s = reinterpret_cast<wl_surface*>(
        native->nativeResourceForWindow(QByteArrayLiteral("surface"), window));
    if (!s) {
        return nullptr;
    }
    if (auto surface = get(s)) {
        return surface;
    }
    Surface* surface = new Surface(window);
    surface->d->surface.setup(s, true);
    return surface;
}

Surface* Surface::fromQtWinId(WId wid)
{
    QWindow* window = nullptr;

    for (auto win : qApp->allWindows()) {
        if (win->winId() == wid) {
            window = win;
            break;
        }
    }

    if (!window) {
        return nullptr;
    }
    return fromWindow(window);
}

void Surface::release()
{
    if (d->pendingFrameCallback) {
        wl_callback_destroy(d->pendingFrameCallback);
        d->pendingFrameCallback = nullptr;
    }
    d->surface.release();
}

void Surface::setup(wl_surface* surface)
{
    d->setup(surface);
}

void Surface::Private::setup(wl_surface* s)
{
    Q_ASSERT(s);
    Q_ASSERT(!surface);
    surface.setup(s);
    wl_surface_add_listener(s, &s_surfaceListener, this);
}

void Surface::Private::frameCallback(void* data, wl_callback* callback, uint32_t time)
{
    Q_UNUSED(time)
    auto s = reinterpret_cast<Surface::Private*>(data);
    if (callback) {
        wl_callback_destroy(callback);
    }
    s->handleFrameCallback();
}

void Surface::Private::handleFrameCallback()
{
    pendingFrameCallback = nullptr;
    Q_EMIT q->frameRendered();
}

#ifndef DOXYGEN_SHOULD_SKIP_THIS
const struct wl_callback_listener Surface::Private::s_listener = {
    frameCallback,
};

const struct wl_surface_listener Surface::Private::s_surfaceListener = {
    enterCallback,
    leaveCallback,
};
#endif

void Surface::Private::enterCallback(void* data, wl_surface* surface, wl_output* output)
{
    Q_UNUSED(surface);
    auto s = reinterpret_cast<Surface::Private*>(data);
    Output* o = Output::get(output);
    if (!o) {
        return;
    }
    s->outputs << o;
    QObject::connect(o, &Output::removed, s->q, [s, o]() {
        if (!s->outputs.contains(o)) {
            return;
        }
        s->outputs.removeOne(o);
        s->q->outputLeft(o);
    });
    Q_EMIT s->q->outputEntered(o);
}

void Surface::Private::leaveCallback(void* data, wl_surface* surface, wl_output* output)
{
    Q_UNUSED(surface);
    auto s = reinterpret_cast<Surface::Private*>(data);
    Output* o = Output::get(output);
    if (!o) {
        return;
    }
    s->outputs.removeOne(o);
    Q_EMIT s->q->outputLeft(o);
}

void Surface::Private::setupFrameCallback()
{
    Q_ASSERT(!pendingFrameCallback);
    pendingFrameCallback = wl_surface_frame(surface);
    wl_callback_add_listener(pendingFrameCallback, &s_listener, this);
}

void Surface::setupFrameCallback()
{
    Q_ASSERT(isValid());
    d->setupFrameCallback();
}

void Surface::commit(Surface::CommitFlag flag)
{
    Q_ASSERT(isValid());
    if (flag == CommitFlag::FrameCallback) {
        setupFrameCallback();
    }
    wl_surface_commit(d->surface);
}

void Surface::damage(QRegion const& region)
{
    for (QRect const& rect : region) {
        damage(rect);
    }
}

void Surface::damage(QRect const& rect)
{
    Q_ASSERT(isValid());
    wl_surface_damage(d->surface, rect.x(), rect.y(), rect.width(), rect.height());
}

void Surface::damageBuffer(QRegion const& region)
{
    for (QRect const& r : region) {
        damageBuffer(r);
    }
}

void Surface::damageBuffer(QRect const& rect)
{
    Q_ASSERT(isValid());
    wl_surface_damage_buffer(d->surface, rect.x(), rect.y(), rect.width(), rect.height());
}

void Surface::attachBuffer(wl_buffer* buffer, QPoint const& offset)
{
    Q_ASSERT(isValid());
    wl_surface_attach(d->surface, buffer, offset.x(), offset.y());
}

void Surface::attachBuffer(Buffer* buffer, QPoint const& offset)
{
    attachBuffer(buffer ? buffer->buffer() : nullptr, offset);
}

void Surface::attachBuffer(Buffer::Ptr buffer, QPoint const& offset)
{
    attachBuffer(buffer.lock().get(), offset);
}

void Surface::setInputRegion(Region const* region)
{
    Q_ASSERT(isValid());
    if (region) {
        wl_surface_set_input_region(d->surface, *region);
    } else {
        wl_surface_set_input_region(d->surface, nullptr);
    }
}

void Surface::setOpaqueRegion(Region const* region)
{
    Q_ASSERT(isValid());
    if (region) {
        wl_surface_set_opaque_region(d->surface, *region);
    } else {
        wl_surface_set_opaque_region(d->surface, nullptr);
    }
}

void Surface::setSize(QSize const& size)
{
    if (d->size == size) {
        return;
    }
    d->size = size;
    Q_EMIT sizeChanged(d->size);
}

Surface* Surface::get(wl_surface* native)
{
    auto it = std::find_if(Private::s_surfaces.constBegin(),
                           Private::s_surfaces.constEnd(),
                           [native](Surface* s) { return s->d->surface == native; });
    if (it != Private::s_surfaces.constEnd()) {
        return *(it);
    }
    return nullptr;
}

QList<Surface*> const& Surface::all()
{
    return Private::s_surfaces;
}

bool Surface::isValid() const
{
    return d->surface.isValid();
}

QSize Surface::size() const
{
    return d->size;
}

Surface::operator wl_surface*()
{
    return d->surface;
}

Surface::operator wl_surface*() const
{
    return d->surface;
}

quint32 Surface::id() const
{
    wl_surface* s = *this;
    return wl_proxy_get_id(reinterpret_cast<wl_proxy*>(s));
}

qint32 Surface::scale() const
{
    return d->scale;
}

void Surface::setScale(qint32 scale)
{
    d->scale = scale;
    wl_surface_set_buffer_scale(d->surface, scale);
}

QVector<Output*> Surface::outputs() const
{
    return d->outputs;
}

}
}
