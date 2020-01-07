/****************************************************************************
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
****************************************************************************/
#pragma once

#include <QObject>

#include <Wrapland/Client/wraplandclient_export.h>

#include <memory>
#include <time.h>

struct wp_presentation;
struct wp_presentation_feedback;

namespace Wrapland
{
namespace Client
{

class EventQueue;
class PresentationFeedback;
class Output;
class Surface;

/**
 * @short Wrapper for the wp_presentation interface.
 *
 * This class provides a convenient wrapper for the wp_presentation interface.
 *
 * To use this class one needs to interact with the Registry. There are two
 * possible ways to create the PresentationManager interface:
 * @code
 * PresentationManager *c = registry->createPresentationManager(name, version);
 * @endcode
 *
 * This creates the PresentationManager and sets it up directly. As an alternative this
 * can also be done in a more low level way:
 * @code
 * PresentationManager *c = new PresentationManager;
 * c->setup(registry->bindPresentationManager(name, version));
 * @endcode
 *
 * The PresentationManager can be used as a drop-in replacement for any wp_presentation
 * pointer as it provides matching cast operators.
 *
 * @see Registry
 **/
class WRAPLANDCLIENT_EXPORT PresentationManager : public QObject
{
    Q_OBJECT
public:
    /**
     * Creates a new PresentationManager.
     * Note: after constructing the PresentationManager it is not yet valid and one needs
     * to call setup. In order to get a ready to use PresentationManager prefer using
     * Registry::createPresentationManager.
     **/
    explicit PresentationManager(QObject *parent = nullptr);
    ~PresentationManager() override;

    /**
     * Setup this PresentationManager to manage the @p presentationmanager.
     * When using Registry::createPresentationManager there is no need to call this
     * method.
     **/
    void setup(wp_presentation *presentation);
    /**
     * @returns @c true if managing a wp_presentation.
     **/
    bool isValid() const;
    /**
     * Releases the wp_presentation interface.
     * After the interface has been released the PresentationManager instance is no
     * longer valid and can be setup with another wp_presentation interface.
     **/
    void release();

    /**
     * Sets the @p queue to use for creating objects with this PresentationManager.
     **/
    void setEventQueue(EventQueue *queue);
    /**
     * @returns The event queue to use for creating objects with this PresentationManager.
     **/
    EventQueue *eventQueue();

    clockid_t clockId() const;
    PresentationFeedback *createFeedback(Surface *surface, QObject *parent = nullptr);

    operator wp_presentation*();
    operator wp_presentation*() const;

Q_SIGNALS:
    /**
     * The corresponding global for this interface on the Registry got removed.
     *
     * This signal gets only emitted if the PresentationManager got created by
     * Registry::createPresentationManager
     **/
    void removed();
    void clockIdChanged();

private:
    class Private;
    std::unique_ptr<Private> d;
};

class WRAPLANDCLIENT_EXPORT PresentationFeedback : public QObject
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

    ~PresentationFeedback() override;

    /**
     * Setup this PresentationFeedback to manage the @p presentationfeedback.
     * When using PresentationManager::createPresentationFeedback there is no need to call this
     * method.
     **/
    void setup(struct wp_presentation_feedback *feedback);
    /**
     * @returns @c true if managing a wp_presentation_feedback.
     **/
    bool isValid() const;

    Output* syncOutput() const;

    uint32_t tvSecHi() const;
    uint32_t tvSecLo() const;
    uint32_t tvNsec() const;
    uint32_t refresh() const;
    uint32_t seqHi() const;
    uint32_t seqLo() const;
    Kinds flags() const;

Q_SIGNALS:
    void presented();
    void discarded();

private:
    friend class PresentationManager;
    explicit PresentationFeedback(QObject *parent = nullptr);

    class Private;
    std::unique_ptr<Private> d;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(Wrapland::Client::PresentationFeedback::Kinds)

}
}
