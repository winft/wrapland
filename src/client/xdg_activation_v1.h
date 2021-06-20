/*
    SPDX-FileCopyrightText: 2021 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#pragma once

#include <QObject>

#include <Wrapland/Client/wraplandclient_export.h>

#include <memory>

struct xdg_activation_v1;
struct xdg_activation_token_v1;

namespace Wrapland
{
namespace Client
{

class EventQueue;
class Seat;
class Surface;
class XdgActivationTokenV1;

/**
 * @short Wrapper for the xdg_activation_v1 interface.
 *
 * This class provides a convenient wrapper for the xdg_activation_v1 interface.
 *
 * To use this class one needs to interact with the Registry. There are two
 * possible ways to create the XdgActivationV1 interface:
 * @code
 * XdgActivationV1 *c = registry->createXdgActivationV1(name, version);
 * @endcode
 *
 * This creates the XdgActivationV1 and sets it up directly. As an alternative this
 * can also be done in a more low level way:
 * @code
 * XdgActivationV1 *c = new XdgActivationV1;
 * c->setup(registry->bindXdgActivationV1(name, version));
 * @endcode
 *
 * The XdgActivationV1 can be used as a drop-in replacement for any xdg_activation_v1
 * pointer as it provides matching cast operators.
 *
 * @see Registry
 **/
class WRAPLANDCLIENT_EXPORT XdgActivationV1 : public QObject
{
    Q_OBJECT
public:
    /**
     * Creates a new XdgActivationV1.
     * Note: after constructing the XdgActivationV1 it is not yet valid and one needs
     * to call setup. In order to get a ready to use XdgActivationV1 prefer using
     * Registry::createXdgActivationV1.
     **/
    explicit XdgActivationV1(QObject* parent = nullptr);
    ~XdgActivationV1() override;

    /**
     * Setup this XdgActivationV1 to manage the @p XdgActivationV1.
     * When using Registry::createXdgActivationV1 there is no need to call this
     * method.
     **/
    void setup(xdg_activation_v1* activation);
    /**
     * @returns @c true if managing a xdg_activation_v1.
     **/
    bool isValid() const;
    /**
     * Releases the xdg_activation_v1 interface.
     * After the interface has been released the XdgActivationV1 instance is no
     * longer valid and can be setup with another xdg_activation_v1 interface.
     **/
    void release();

    /**
     * Sets the @p queue to use for creating objects with this XdgActivationV1.
     **/
    void setEventQueue(EventQueue* queue);
    /**
     * @returns The event queue to use for creating objects with this XdgActivationV1.
     **/
    EventQueue* eventQueue();

    XdgActivationTokenV1* create_token(QObject* parent = nullptr);
    void activate(std::string const& token, Surface* surface);

    operator xdg_activation_v1*();
    operator xdg_activation_v1*() const;

Q_SIGNALS:
    /**
     * The corresponding global for this interface on the Registry got removed.
     *
     * This signal gets only emitted if the XdgActivationV1 got created by
     * Registry::createXdgActivationV1
     **/
    void removed();

private:
    class Private;
    std::unique_ptr<Private> d_ptr;
};

class WRAPLANDCLIENT_EXPORT XdgActivationTokenV1 : public QObject
{
    Q_OBJECT
public:
    enum class Kind {
        Vsync,
        HwClock,
        HwCompletion,
        ZeroCopy,
    };
    Q_DECLARE_FLAGS(Kinds, Kind)

    ~XdgActivationTokenV1() override;

    /**
     * Setup this XdgActivationTokenV1 to manage the @p XdgActivationTokenV1.
     * When using XdgActivationV1::createXdgActivationTokenV1 there is no need to call this
     * method.
     **/
    void setup(struct xdg_activation_token_v1* token);
    /**
     * @returns @c true if managing a xdg_activation_token_v1.
     **/
    bool isValid() const;

    void set_serial(uint32_t serial, Seat* seat);
    void set_app_id(std::string const& app_id);
    void set_surface(Surface* surface);

    void commit();

Q_SIGNALS:
    void done(std::string const& token);

private:
    friend class XdgActivationV1;
    explicit XdgActivationTokenV1(QObject* parent = nullptr);

    class Private;
    std::unique_ptr<Private> d_ptr;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(Wrapland::Client::XdgActivationTokenV1::Kinds)

}
}
