/*
    SPDX-FileCopyrightText: 2021 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#pragma once

#include <Wrapland/Server/wraplandserver_export.h>

#include <QMargins>
#include <QObject>
#include <QSize>
#include <memory>

namespace Wrapland::Server
{

class Client;
class Display;
class LayerSurfaceV1;
class Output;
class Surface;
class XdgShellPopup;

class WRAPLANDSERVER_EXPORT LayerShellV1 : public QObject
{
    Q_OBJECT
public:
    explicit LayerShellV1(Display* display);
    ~LayerShellV1() override;

Q_SIGNALS:
    void surface_created(LayerSurfaceV1* surface);

private:
    class Private;
    std::unique_ptr<Private> d_ptr;
};

class WRAPLANDSERVER_EXPORT LayerSurfaceV1 : public QObject
{
    Q_OBJECT
public:
    enum class Layer {
        Background,
        Bottom,
        Top,
        Overlay,
    };
    enum class KeyboardInteractivity {
        None,
        Exclusive,
        OnDemand,
    };

    Surface* surface() const;
    Output* output() const;

    /**
     * Compositor should set a fixed output on first commit if client did not specify one.
     */
    void set_output(Output* output);
    std::string domain() const;

    QSize size() const;
    Qt::Edges anchor() const;
    QMargins margins() const;

    Layer layer() const;
    KeyboardInteractivity keyboard_interactivity() const;

    int exclusive_zone() const;
    Qt::Edge exclusive_edge() const;

    uint32_t configure(QSize const& size);
    void close();

    bool change_pending() const;

Q_SIGNALS:
    void configure_acknowledged(quint32 serial);
    void got_popup(Wrapland::Server::XdgShellPopup* popup);
    void resourceDestroyed();

private:
    LayerSurfaceV1(Client* client,
                   uint32_t version,
                   uint32_t id,
                   Surface* surface,
                   Output* output,
                   Layer layer,
                   std::string domain);
    friend class LayerShellV1;
    friend class Surface;

    class Private;
    Private* d_ptr;
};

}
