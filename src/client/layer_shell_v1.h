/*
    SPDX-FileCopyrightText: 2021 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#pragma once

#include <Wrapland/Client/wraplandclient_export.h>

#include <QObject>
#include <QSize>

#include <memory>
#include <string>

struct zwlr_layer_shell_v1;
struct zwlr_layer_surface_v1;

namespace Wrapland
{
namespace Client
{

class EventQueue;
class LayerSurfaceV1;
class Output;
class Surface;
class XdgShellPopup;

/**
 * TODO
 */
class WRAPLANDCLIENT_EXPORT LayerShellV1 : public QObject
{
    Q_OBJECT
public:
    enum class layer {
        background,
        bottom,
        top,
        overlay,
    };
    enum class keyboard_interactivity {
        none,
        exclusive,
        on_demand,
    };

    /**
     * Creates a new LayerShellV1.
     * Note: after constructing the LayerShellV1 it is not yet valid and one needs
     * to call setup. In order to get a ready to use LayerShellV1 prefer using
     * Registry::createLayerShellV1.
     */
    explicit LayerShellV1(QObject* parent = nullptr);
    virtual ~LayerShellV1();

    /**
     * @returns @c true if managing a zwlr_layer_shell_v1.
     */
    bool isValid() const;
    /**
     * Setup this LayerShellV1 to manage the @p layer_shell.
     * When using Registry::createLayerShellV1 there is no need to call this
     * method.
     */
    void setup(zwlr_layer_shell_v1* layer_shell);
    /**
     * Releases the zwlr_layer_shell_v1 interface.
     * After the interface has been released the LayerShellV1 instance is no
     * longer valid and can be setup with another zwlr_layer_shell_v1 interface.
     */
    void release();

    /**
     * Sets the @p queue to use for creating a LayerSurfaceV1.
     */
    void setEventQueue(EventQueue* queue);
    /**
     * @returns The event queue to use for creating a LayerSurfaceV1.
     */
    EventQueue* eventQueue();

    /**
     * Creates and setup a new LayerSurfaceV1 with @p parent.
     * @param parent The parent to pass to the LayerSurfaceV1.
     * @returns The new created LayerSurfaceV1
     */
    LayerSurfaceV1* get_layer_surface(Surface* surface,
                                      Output* output,
                                      layer lay,
                                      std::string const& domain,
                                      QObject* parent = nullptr);

    operator zwlr_layer_shell_v1*();
    operator zwlr_layer_shell_v1*() const;

Q_SIGNALS:
    /**
     * The corresponding global for this interface on the Registry got removed.
     *
     * This signal gets only emitted if the LayerShellV1 got created by
     * Registry::createLayerShellV1
     */
    void removed();

private:
    class Private;
    std::unique_ptr<Private> d;
};

/**
 * @short Wrapper for the zwlr_layer_surface_v1 interface.
 *
 * This class is a convenient wrapper for the zwlr_layer_surface_v1 interface.
 * To create a layer surface call LayerShellV1::create_layer_surface.
 *
 * The main purpose of this class is to setup the next frame which
 * should be rendered. Therefore it provides methods to add damage
 * and to attach a new Buffer and to finalize the frame by calling
 * commit.
 *
 * @see LayerShellV1
 */
class WRAPLANDCLIENT_EXPORT LayerSurfaceV1 : public QObject
{
    Q_OBJECT
public:
    virtual ~LayerSurfaceV1();

    /**
     * Setup this LayerSurfaceV1 to manage the @p layer_surface.
     * When using LayerShellV1::create_layer_surface there is no need to call this
     * method.
     */
    void setup(zwlr_layer_surface_v1* layer_surface);
    /**
     * Releases the zwlr_layer_surface_v1 interface.
     * After the interface has been released the LayerSurfaceV1 instance is no
     * longer valid and can be setup with another zwlr_layer_surface_v1 interface.
     */
    void release();

    /**
     * @returns @c true if managing a zwlr_layer_surface_v1.
     */
    bool isValid() const;

    void set_size(QSize const& size);
    void set_anchor(Qt::Edges anchor);
    void set_exclusive_zone(int zone);
    void set_margin(QMargins const& margins);
    void set_keyboard_interactivity(LayerShellV1::keyboard_interactivity interactivity);
    void get_popup(XdgShellPopup* popup);
    void ack_configure(int serial);
    void set_layer(LayerShellV1::layer arg);

    operator zwlr_layer_surface_v1*();
    operator zwlr_layer_surface_v1*() const;

Q_SIGNALS:
    void configure_requested(QSize const& size, quint32 serial);
    void closed();

private:
    friend class LayerShellV1;
    explicit LayerSurfaceV1(QObject* parent = nullptr);
    class Private;
    std::unique_ptr<Private> d;
};

}
}
