/*
    SPDX-FileCopyrightText: 2021 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#include "layer_shell_v1_p.h"

#include "display.h"
#include "surface_p.h"
#include "wl_output_p.h"
#include "xdg_shell_popup.h"

namespace Wrapland::Server
{

const struct zwlr_layer_shell_v1_interface LayerShellV1::Private::s_interface = {
    cb<getCallback>,
    cb<destroyCallback>,
};

LayerShellV1::Private::Private(Display* display, LayerShellV1* qptr)
    : LayerShellV1Global(qptr, display, &zwlr_layer_shell_v1_interface, &s_interface)
{
    create();
}

LayerShellV1::Private::~Private() = default;

LayerSurfaceV1::Layer get_layer(uint32_t layer)
{
    switch (layer) {
    case ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM:
        return Layer::Bottom;
    case ZWLR_LAYER_SHELL_V1_LAYER_TOP:
        return Layer::Top;
    case ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY:
        return Layer::Overlay;
    case ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND:
    default:
        return Layer::Background;
    }
}

void LayerShellV1::Private::getCallback(LayerShellV1Bind* bind,
                                        uint32_t id,
                                        wl_resource* wlSurface,
                                        wl_resource* wlOutput,
                                        uint32_t wlLayer,
                                        char const* nspace)
{
    auto surface = Wayland::Resource<Surface>::handle(wlSurface);
    auto output = wlOutput ? WlOutputGlobal::handle(wlOutput)->output() : nullptr;
    auto layer = get_layer(wlLayer);

    if (surface->d_ptr->has_role()) {
        bind->post_error(ZWLR_LAYER_SHELL_V1_ERROR_ROLE, "Surface already has a role.");
        return;
    }
    if (surface->d_ptr->had_buffer_attached) {
        bind->post_error(ZWLR_LAYER_SHELL_V1_ERROR_ALREADY_CONSTRUCTED,
                         "Creation after a buffer was already attached.");
        return;
    }
    if (wlLayer != ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND && layer == Layer::Background) {
        bind->post_error(ZWLR_LAYER_SHELL_V1_ERROR_INVALID_LAYER, "Invalid layer set.");
        return;
    }
    auto layer_surface = new LayerSurfaceV1(
        bind->client()->handle(), bind->version(), id, surface, output, layer, std::string(nspace));
    if (!layer_surface->d_ptr->resource()) {
        bind->post_no_memory();
        delete layer_surface;
        return;
    }
    Q_EMIT bind->global()->handle()->surface_created(layer_surface);
}

void LayerShellV1::Private::destroyCallback([[maybe_unused]] LayerShellV1Bind* bind)
{
    // TODO(romangg): unregister the client?
}

LayerShellV1::LayerShellV1(Display* display, QObject* parent)
    : QObject(parent)
    , d_ptr(new Private(display, this))
{
}

LayerShellV1::~LayerShellV1() = default;

const struct zwlr_layer_surface_v1_interface LayerSurfaceV1::Private::s_interface = {
    setSizeCallback,
    setAnchorCallback,
    setExclusiveZoneCallback,
    setMarginCallback,
    setKeyboardInteractivityCallback,
    getPopupCallback,
    ackConfigureCallback,
    destroyCallback,
    setLayerCallback,
};

LayerSurfaceV1::Private::Private(Client* client,
                                 uint32_t version,
                                 uint32_t id,
                                 Surface* surface,
                                 Output* output,
                                 Layer layer,
                                 std::string domain,
                                 LayerSurfaceV1* qptr)
    : Wayland::Resource<LayerSurfaceV1>(client,
                                        version,
                                        id,
                                        &zwlr_layer_surface_v1_interface,
                                        &s_interface,
                                        qptr)
    , surface{surface}
    , domain{std::move(domain)}
{
    if (output) {
        set_output(output);
    }
    current.layer = layer;
    pending.layer = layer;

    // First commit must be processed.
    current.set = true;
    pending.set = true;

    surface->d_ptr->layer_surface = qptr;
    QObject::connect(surface, &Surface::resourceDestroyed, qptr, [this] {
        this->surface = nullptr;

        // TODO(romangg): do not use that but an extra signal or automatic with surface.
        Q_EMIT handle()->resourceDestroyed();
    });
}

LayerSurfaceV1::Private::~Private()
{
    if (surface) {
        surface->d_ptr->layer_surface = nullptr;
    }
}

void LayerSurfaceV1::Private::setSizeCallback([[maybe_unused]] wl_client* wlClient,
                                              wl_resource* wlResource,
                                              uint32_t width,
                                              uint32_t height)
{
    auto priv = handle(wlResource)->d_ptr;
    priv->pending.size = QSize(width, height);
    priv->pending.set = true;
}

void LayerSurfaceV1::Private::setAnchorCallback([[maybe_unused]] wl_client* wlClient,
                                                wl_resource* wlResource,
                                                uint32_t anchor)
{
    auto get_anchor = [](auto anchor) {
        Qt::Edges ret;
        if (anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP) {
            ret |= Qt::TopEdge;
        }
        if (anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM) {
            ret |= Qt::BottomEdge;
        }
        if (anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT) {
            ret |= Qt::LeftEdge;
        }
        if (anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT) {
            ret |= Qt::RightEdge;
        }
        return ret;
    };

    auto priv = handle(wlResource)->d_ptr;
    priv->pending.anchor = get_anchor(anchor);
    priv->pending.set = true;
}

void LayerSurfaceV1::Private::setExclusiveZoneCallback([[maybe_unused]] wl_client* wlClient,
                                                       wl_resource* wlResource,
                                                       int zone)
{
    auto priv = handle(wlResource)->d_ptr;
    priv->pending.exclusive_zone = zone;
    priv->pending.set = true;
}

void LayerSurfaceV1::Private::setMarginCallback([[maybe_unused]] wl_client* wlClient,
                                                wl_resource* wlResource,
                                                int top,
                                                int right,
                                                int bottom,
                                                int left)
{
    auto priv = handle(wlResource)->d_ptr;
    priv->pending.margins = QMargins(left, top, right, bottom);
    priv->pending.set = true;
}

void LayerSurfaceV1::Private::setKeyboardInteractivityCallback([[maybe_unused]] wl_client* wlClient,
                                                               wl_resource* wlResource,
                                                               uint32_t interactivity)
{
    auto priv = handle(wlResource)->d_ptr;
    if (interactivity == ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_EXCLUSIVE) {
        priv->pending.keyboard_interactivity = KeyboardInteractivity::Exclusive;
    } else if (interactivity == ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_ON_DEMAND) {
        priv->pending.keyboard_interactivity = KeyboardInteractivity::OnDemand;
    } else {
        priv->pending.keyboard_interactivity = KeyboardInteractivity::None;
    }
    priv->pending.set = true;
}

void LayerSurfaceV1::Private::getPopupCallback([[maybe_unused]] wl_client* wlClient,
                                               wl_resource* wlResource,
                                               wl_resource* wlPopup)
{
    auto priv = handle(wlResource)->d_ptr;
    auto popup = Wayland::Resource<XdgShellPopup>::handle(wlPopup);

    if (popup->transientFor()) {
        // TODO(romangg): * transientFor() does not cover the case where popup was assigned through
        //                  another request like this get_popup call. On the other side the protocol
        //                  only talks about it in regards to the creation time anyways.
        //                * Should we send an error? Protocol doesn't say so.
        return;
    }
    if (popup->surface()->surface()->d_ptr->had_buffer_attached) {
        // TODO(romangg): Should we send an error? Protocol doesn't say so.
        return;
    }

    // TODO(romangg): check that the popup has a null parent and initial state already set.
    Q_EMIT priv->handle()->got_popup(popup);
}

void LayerSurfaceV1::Private::ackConfigureCallback([[maybe_unused]] wl_client* wlClient,
                                                   wl_resource* wlResource,
                                                   uint32_t serial)
{
    auto priv = handle(wlResource)->d_ptr;

    auto& serials = priv->configure_serials;

    if (std::count(serials.cbegin(), serials.cend(), serial) == 0) {
        return;
    }

    while (!serials.empty()) {
        auto next = serials.front();
        serials.pop_front();

        if (next == serial) {
            break;
        }
    }
    Q_EMIT priv->handle()->configure_acknowledged(serial);
}

void LayerSurfaceV1::Private::setLayerCallback([[maybe_unused]] wl_client* wlClient,
                                               wl_resource* wlResource,
                                               uint32_t layer)
{
    auto priv = handle(wlResource)->d_ptr;
    priv->pending.layer = get_layer(layer);
    priv->pending.set = true;
}

void LayerSurfaceV1::Private::set_output(Output* output)
{
    assert(output);
    this->output = output;
    connect(output->wayland_output(), &WlOutput::removed, handle(), [this]() {
        this->output = nullptr;
        handle()->close();
        Q_EMIT handle()->resourceDestroyed();
    });
}

bool LayerSurfaceV1::Private::commit()
{
    if (closed) {
        return false;
    }
    if (!pending.set) {
        current.set = false;
        return true;
    }

    if (pending.size.width() == 0) {
        if (!(pending.anchor & Qt::LeftEdge) || !(pending.anchor & Qt::RightEdge)) {
            postError(ZWLR_LAYER_SURFACE_V1_ERROR_INVALID_SIZE,
                      "Width zero while not anchoring to both vertical edges.");
            return false;
        }
    }
    if (pending.size.height() == 0) {
        if (!(pending.anchor & Qt::TopEdge) || !(pending.anchor & Qt::BottomEdge)) {
            postError(ZWLR_LAYER_SURFACE_V1_ERROR_INVALID_SIZE,
                      "Height zero while not anchoring to both horizontal edges.");
            return false;
        }
    }

    current = pending;
    pending.set = false;
    return true;
}

LayerSurfaceV1::LayerSurfaceV1(Client* client,
                               uint32_t version,
                               uint32_t id,
                               Surface* surface,
                               Output* output,
                               Layer layer,
                               std::string domain)
    : d_ptr(new Private(client, version, id, surface, output, layer, std::move(domain), this))
{
}

Surface* LayerSurfaceV1::surface() const
{
    return d_ptr->surface;
}

Output* LayerSurfaceV1::output() const
{
    return d_ptr->output;
}

void LayerSurfaceV1::set_output(Output* output)
{
    assert(output);
    assert(!d_ptr->output);
    d_ptr->set_output(output);
}

std::string LayerSurfaceV1::domain() const
{
    return d_ptr->domain;
}

QSize LayerSurfaceV1::size() const
{
    return d_ptr->current.size;
}

Qt::Edges LayerSurfaceV1::anchor() const
{
    return d_ptr->current.anchor;
}

QMargins LayerSurfaceV1::margins() const
{
    auto const anchor = d_ptr->current.anchor;
    auto margins = d_ptr->current.margins;

    if (!(anchor & Qt::LeftEdge)) {
        margins.setLeft(0);
    }
    if (!(anchor & Qt::TopEdge)) {
        margins.setTop(0);
    }
    if (!(anchor & Qt::RightEdge)) {
        margins.setRight(0);
    }
    if (!(anchor & Qt::BottomEdge)) {
        margins.setBottom(0);
    }

    return margins;
}

LayerSurfaceV1::Layer LayerSurfaceV1::layer() const
{
    return d_ptr->current.layer;
}

LayerSurfaceV1::KeyboardInteractivity LayerSurfaceV1::keyboard_interactivity() const
{
    return d_ptr->current.keyboard_interactivity;
}

int LayerSurfaceV1::exclusive_zone() const
{
    auto zone = d_ptr->current.exclusive_zone;

    if (zone <= 0) {
        return zone;
    }
    if (exclusive_edge() == 0) {
        // By protocol when no exclusive edge is well defined, a positive zone is set to 0.
        return 0;
    }

    return zone;
}

Qt::Edge LayerSurfaceV1::exclusive_edge() const
{
    auto const& state = d_ptr->current;

    if (state.exclusive_zone <= 0) {
        return Qt::Edge();
    }

    auto anchor = state.anchor;

    if (anchor & Qt::TopEdge) {
        if (anchor & Qt::BottomEdge) {
            return Qt::Edge();
        }
        if (anchor == Qt::TopEdge) {
            return Qt::TopEdge;
        }
        if ((anchor & Qt::LeftEdge) && (anchor & Qt::RightEdge)) {
            return Qt::TopEdge;
        }
        return Qt::Edge();
    }
    if (anchor & Qt::BottomEdge) {
        if (anchor == Qt::BottomEdge) {
            return Qt::BottomEdge;
        }
        if ((anchor & Qt::LeftEdge) && (anchor & Qt::RightEdge)) {
            return Qt::BottomEdge;
        }
    }
    if (anchor == Qt::LeftEdge) {
        return Qt::LeftEdge;
    }
    if (anchor == Qt::RightEdge) {
        return Qt::RightEdge;
    }
    return Qt::Edge();
}

uint32_t LayerSurfaceV1::configure(QSize const& size)
{
    auto serial = d_ptr->client()->display()->handle()->nextSerial();
    d_ptr->configure_serials.push_back(serial);
    d_ptr->send<zwlr_layer_surface_v1_send_configure>(serial, size.width(), size.height());
    return serial;
}

void LayerSurfaceV1::close()
{
    d_ptr->closed = true;
    d_ptr->send<zwlr_layer_surface_v1_send_closed>();
}

bool LayerSurfaceV1::change_pending() const
{
    return d_ptr->current.set;
}

}
