/*
    SPDX-FileCopyrightText: 2021 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#include "layer_shell_v1.h"

#include "event_queue.h"
#include "output.h"
#include "surface.h"
#include "wayland_pointer_p.h"
#include "xdg_shell_popup.h"

#include <wayland-wlr-layer-shell-client-protocol.h>

namespace Wrapland::Client
{

class Q_DECL_HIDDEN LayerShellV1::Private
{
public:
    Private() = default;

    WaylandPointer<zwlr_layer_shell_v1, zwlr_layer_shell_v1_destroy> manager;
    EventQueue* queue = nullptr;
};

LayerShellV1::LayerShellV1(QObject* parent)
    : QObject(parent)
    , d(new Private)
{
}

LayerShellV1::~LayerShellV1()
{
    release();
}

void LayerShellV1::release()
{
    d->manager.release();
}

bool LayerShellV1::isValid() const
{
    return d->manager.isValid();
}

void LayerShellV1::setup(zwlr_layer_shell_v1* manager)
{
    assert(manager);
    assert(!d->manager);
    d->manager.setup(manager);
}

void LayerShellV1::setEventQueue(EventQueue* queue)
{
    d->queue = queue;
}

EventQueue* LayerShellV1::eventQueue()
{
    return d->queue;
}

uint32_t to_wl_layer(LayerShellV1::layer lay)
{
    switch (lay) {
    case LayerShellV1::layer::bottom:
        return ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM;
    case LayerShellV1::layer::top:
        return ZWLR_LAYER_SHELL_V1_LAYER_TOP;
    case LayerShellV1::layer::overlay:
        return ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY;
    case LayerShellV1::layer::background:
    default:
        return ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND;
    }
}

LayerSurfaceV1* LayerShellV1::get_layer_surface(Surface* surface,
                                                Output* output,
                                                LayerShellV1::layer lay,
                                                std::string const& domain,
                                                QObject* parent)
{
    assert(isValid());
    auto layer_surface = new LayerSurfaceV1(parent);
    auto proxy = zwlr_layer_shell_v1_get_layer_surface(d->manager,
                                                       *surface,
                                                       output ? output->output() : nullptr,
                                                       to_wl_layer(lay),
                                                       domain.c_str());
    if (d->queue) {
        d->queue->addProxy(proxy);
    }
    layer_surface->setup(proxy);
    return layer_surface;
}

LayerShellV1::operator zwlr_layer_shell_v1*()
{
    return d->manager;
}

LayerShellV1::operator zwlr_layer_shell_v1*() const
{
    return d->manager;
}

class LayerSurfaceV1::Private
{
public:
    explicit Private(LayerSurfaceV1* qptr);
    void setup(zwlr_layer_surface_v1* layer_surface);

    WaylandPointer<zwlr_layer_surface_v1, zwlr_layer_surface_v1_destroy> layer_surface;

private:
    static zwlr_layer_surface_v1_listener const s_listener;
    static void configure_callback(void* data,
                                   zwlr_layer_surface_v1* surf,
                                   uint32_t serial,
                                   uint32_t width,
                                   uint32_t height);
    static void closed_callback(void* data, zwlr_layer_surface_v1* surf);
    LayerSurfaceV1* qptr;
};

const zwlr_layer_surface_v1_listener LayerSurfaceV1::Private::s_listener = {
    LayerSurfaceV1::Private::configure_callback,
    LayerSurfaceV1::Private::closed_callback,
};

LayerSurfaceV1::Private::Private(LayerSurfaceV1* qptr)
    : qptr{qptr}
{
}

void LayerSurfaceV1::Private::configure_callback(void* data,
                                                 [[maybe_unused]] zwlr_layer_surface_v1* surf,
                                                 uint32_t serial,
                                                 uint32_t width,
                                                 uint32_t height)
{
    auto priv = reinterpret_cast<Private*>(data);
    Q_EMIT priv->qptr->configure_requested(QSize(width, height), serial);
}

void LayerSurfaceV1::Private::closed_callback(void* data,
                                              [[maybe_unused]] zwlr_layer_surface_v1* surf)
{
    auto priv = reinterpret_cast<Private*>(data);
    Q_EMIT priv->qptr->closed();
}

void LayerSurfaceV1::Private::setup(zwlr_layer_surface_v1* layer_surface)
{
    assert(layer_surface);
    assert(!this->layer_surface);
    this->layer_surface.setup(layer_surface);
    zwlr_layer_surface_v1_add_listener(this->layer_surface, &s_listener, this);
}

LayerSurfaceV1::LayerSurfaceV1(QObject* parent)
    : QObject(parent)
    , d(new Private(this))
{
}

LayerSurfaceV1::~LayerSurfaceV1()
{
    release();
}

void LayerSurfaceV1::release()
{
    d->layer_surface.release();
}

void LayerSurfaceV1::setup(zwlr_layer_surface_v1* layer_surface)
{
    d->setup(layer_surface);
}

bool LayerSurfaceV1::isValid() const
{
    return d->layer_surface.isValid();
}

void LayerSurfaceV1::set_size(QSize const& size)
{
    assert(isValid());
    zwlr_layer_surface_v1_set_size(d->layer_surface, size.width(), size.height());
}

uint32_t to_wl_anchor(Qt::Edges anchor)
{
    uint32_t ret{0};
    if (anchor & Qt::TopEdge) {
        ret |= ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP;
    }
    if (anchor & Qt::BottomEdge) {
        ret |= ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM;
    }
    if (anchor & Qt::LeftEdge) {
        ret |= ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT;
    }
    if (anchor & Qt::RightEdge) {
        ret |= ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT;
    }
    return ret;
}

void LayerSurfaceV1::set_anchor(Qt::Edges anchor)
{
    assert(isValid());
    zwlr_layer_surface_v1_set_anchor(d->layer_surface, to_wl_anchor(anchor));
}

void LayerSurfaceV1::set_exclusive_zone(int zone)
{
    assert(isValid());
    zwlr_layer_surface_v1_set_exclusive_zone(d->layer_surface, zone);
}

void LayerSurfaceV1::set_margin(QMargins const& margins)
{
    assert(isValid());
    zwlr_layer_surface_v1_set_margin(
        d->layer_surface, margins.top(), margins.right(), margins.bottom(), margins.left());
}

uint32_t to_wl_keyboard_interactivity(LayerShellV1::keyboard_interactivity interactivity)
{
    switch (interactivity) {
    case LayerShellV1::keyboard_interactivity::exclusive:
        return ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_EXCLUSIVE;
    case LayerShellV1::keyboard_interactivity::on_demand:
        return ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_ON_DEMAND;
    case LayerShellV1::keyboard_interactivity::none:
    default:
        return ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_NONE;
    }
}

void LayerSurfaceV1::set_keyboard_interactivity(LayerShellV1::keyboard_interactivity interactivity)
{
    assert(isValid());
    zwlr_layer_surface_v1_set_keyboard_interactivity(d->layer_surface,
                                                     to_wl_keyboard_interactivity(interactivity));
}

void LayerSurfaceV1::get_popup(XdgShellPopup* popup)
{
    assert(isValid());
    zwlr_layer_surface_v1_get_popup(d->layer_surface, *popup);
}

void LayerSurfaceV1::ack_configure(int serial)
{
    assert(isValid());
    zwlr_layer_surface_v1_ack_configure(d->layer_surface, serial);
}

void LayerSurfaceV1::set_layer(LayerShellV1::layer arg)
{
    assert(isValid());
    zwlr_layer_surface_v1_set_layer(d->layer_surface, to_wl_layer(arg));
}

LayerSurfaceV1::operator zwlr_layer_surface_v1*()
{
    return d->layer_surface;
}

LayerSurfaceV1::operator zwlr_layer_surface_v1*() const
{
    return d->layer_surface;
}

}
