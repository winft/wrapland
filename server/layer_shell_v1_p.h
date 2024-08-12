/*
    SPDX-FileCopyrightText: 2021 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#pragma once

#include "layer_shell_v1.h"

#include "wayland/global.h"
#include "wayland/resource.h"

#include <QObject>
#include <deque>

#include <wayland-wlr-layer-shell-server-protocol.h>

namespace Wrapland::Server
{

class Display;

constexpr uint32_t LayerShellV1Version = 4;
using LayerShellV1Global = Wayland::Global<LayerShellV1, LayerShellV1Version>;

using Interactivity = LayerSurfaceV1::KeyboardInteractivity;
using Layer = LayerSurfaceV1::Layer;

class LayerShellV1::Private : public LayerShellV1Global
{
public:
    Private(Display* display, LayerShellV1* qptr);

private:
    static void getCallback(LayerShellV1Global::bind_t* bind,
                            uint32_t id,
                            wl_resource* wlSurface,
                            wl_resource* wlOutput,
                            uint32_t wlLayer,
                            char const* nspace);
    static void destroyCallback(LayerShellV1Global::bind_t* bind);

    static const struct zwlr_layer_shell_v1_interface s_interface;
};

class LayerSurfaceV1::Private : public Wayland::Resource<LayerSurfaceV1>
{
public:
    Private(Client* client,
            uint32_t version,
            uint32_t id,
            Surface* surface,
            Server::output* output,
            Layer layer,
            std::string domain,
            LayerSurfaceV1* qptr);
    ~Private() override;

    bool commit();
    void set_output(Server::output* output);

    struct State {
        // Protocol stipulates that size has zero width/height by default.
        QSize size{0, 0};
        Qt::Edges anchor;
        int exclusive_zone{0};
        QMargins margins;
        Interactivity keyboard_interactivity{Interactivity::None};
        Layer layer;

        bool set{false};
    } pending, current;

    Surface* surface{nullptr};
    Server::output* output{nullptr};
    std::string domain;

    std::deque<uint32_t> configure_serials;
    bool closed{false};

private:
    static void
    setSizeCallback(wl_client* wlClient, wl_resource* wlResource, uint32_t width, uint32_t height);
    static void setAnchorCallback(wl_client* wlClient, wl_resource* wlResource, uint32_t anchor);
    static void setExclusiveZoneCallback(wl_client* wlClient, wl_resource* wlResource, int zone);
    static void setMarginCallback(wl_client* wlClient,
                                  wl_resource* wlResource,
                                  int top,
                                  int right,
                                  int bottom,
                                  int left);
    static void setKeyboardInteractivityCallback(wl_client* wlClient,
                                                 wl_resource* wlResource,
                                                 uint32_t interactivity);
    static void
    getPopupCallback(wl_client* wlClient, wl_resource* wlResource, wl_resource* wlPopup);
    static void ackConfigureCallback(wl_client* wlClient, wl_resource* wlResource, uint32_t serial);
    static void setLayerCallback(wl_client* wlClient, wl_resource* wlResource, uint32_t layer);

    static const struct zwlr_layer_surface_v1_interface s_interface;
};

}
