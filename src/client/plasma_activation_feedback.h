/*
    SPDX-FileCopyrightText: 2022 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#pragma once

#include <QObject>

#include <Wrapland/Client/wraplandclient_export.h>

#include <memory>

struct org_kde_plasma_activation_feedback;
struct org_kde_plasma_activation;

namespace Wrapland::Client
{

class EventQueue;
class plasma_activation;

/**
 * @short Wrapper for the plasma_activation_feedback interface.
 *
 * This class provides a convenient wrapper for the plasma_activation_feedback interface.
 *
 * To use this class one needs to interact with the Registry. There are two
 * possible ways to create the plasma_activation_feedback interface:
 * @code
 * plasma_activation_feedback *c = registry->createPlasmaActivationFeedback(name, version);
 * @endcode
 *
 * This creates the plasma_activation_feedback and sets it up directly. As an alternative this
 * can also be done in a more low level way:
 * @code
 * plasma_activation_feedback *c = new plasma_activation_feedback;
 * c->setup(registry->bindPlasmaActivationFeedback(name, version));
 * @endcode
 *
 * The plasma_activation_feedback can be used as a drop-in replacement for any
 *plasma_activation_feedback pointer as it provides matching cast operators.
 *
 * @see Registry
 **/
class WRAPLANDCLIENT_EXPORT plasma_activation_feedback : public QObject
{
    Q_OBJECT
public:
    /**
     * Creates a new plasma_activation_feedback.
     * Note: after constructing the plasma_activation_feedback it is not yet valid and one needs
     * to call setup. In order to get a ready to use plasma_activation_feedback prefer using
     * Registry::createPlasmaActivationFeedback.
     **/
    explicit plasma_activation_feedback(QObject* parent = nullptr);
    ~plasma_activation_feedback() override;

    /**
     * Setup this plasma_activation_feedback to manage the @p plasma_activation_feedback.
     * When using Registry::createPlasmaActivationFeedback there is no need to call this
     * method.
     **/
    void setup(org_kde_plasma_activation_feedback* activation_feedback);
    /**
     * @returns @c true if managing a plasma_activation_feedback.
     **/
    bool isValid() const;
    /**
     * Releases the plasma_activation_feedback interface.
     * After the interface has been released the plasma_activation_feedback instance is no
     * longer valid and can be setup with another plasma_activation_feedback interface.
     **/
    void release();

    /**
     * Sets the @p queue to use for creating objects with this plasma_activation_feedback.
     **/
    void setEventQueue(EventQueue* queue);
    /**
     * @returns The event queue to use for creating objects with this plasma_activation_feedback.
     **/
    EventQueue* eventQueue();

    operator org_kde_plasma_activation_feedback*();
    operator org_kde_plasma_activation_feedback*() const;

Q_SIGNALS:
    /**
     * The corresponding global for this interface on the Registry got removed.
     *
     * This signal gets only emitted if the plasma_activation_feedback got created by
     * Registry::createPlasmaActivationFeedback
     **/
    void removed();
    void activation(plasma_activation*);

private:
    class Private;
    std::unique_ptr<Private> d_ptr;
};

class WRAPLANDCLIENT_EXPORT plasma_activation : public QObject
{
    Q_OBJECT
public:
    ~plasma_activation() override;

    /**
     * Setup this plasma_activation to manage the @p plasma_activation.
     **/
    void setup(struct org_kde_plasma_activation* activation);
    /**
     * @returns @c true if managing a org_kde_plasma_activation.
     **/
    bool isValid() const;

    std::string const& app_id() const;
    bool is_finished() const;

Q_SIGNALS:
    void app_id_changed();
    void finished();

private:
    friend class plasma_activation_feedback;
    explicit plasma_activation(QObject* parent = nullptr);

    class Private;
    std::unique_ptr<Private> d_ptr;
};

}
