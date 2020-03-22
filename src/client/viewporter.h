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
#pragma once

#include "buffer.h"

#include <QObject>
#include <QPoint>
#include <QSize>

#include <Wrapland/Client/wraplandclient_export.h>

struct wl_buffer;
struct wp_viewport;
struct wp_viewporter;

class QMarginsF;
class QWindow;

namespace Wrapland
{
namespace Client
{

class EventQueue;
class Viewport;
class Surface;

/**
 * @short Wrapper for the wp_viewporter interface.
 *
 * This class provides a convenient wrapper for the wp_viewporter interface.
 *
 * To use this class one needs to interact with the Registry. There are two
 * possible ways to create the Viewporter interface:
 * @code
 * Viewporter *vp = registry->createViewporter(name, version);
 * @endcode
 *
 * This creates the Viewporter and sets it up directly. As an alternative this
 * can also be done in a more low level way:
 * @code
 * Viewporter *vp = new Viewporter;
 * s->setup(registry->bindViewporter(name, version));
 * @endcode
 *
 * The Viewporter can be used as a drop-in replacement for any wp_viewporter
 * pointer as it provides matching cast operators.
 *
 * @see Registry
 * @since 5.66
 **/
class WRAPLANDCLIENT_EXPORT Viewporter : public QObject
{
    Q_OBJECT
public:
    /**
     * Creates a new Viewporter.
     * Note: after constructing the Viewporter it is not yet valid and one needs
     * to call setup. In order to get a ready to use Viewporter prefer using
     * Registry::createViewporter.
     **/
    explicit Viewporter(QObject *parent = nullptr);
    virtual ~Viewporter();

    /**
     * @returns @c true if managing a wp_viewporter.
     **/
    bool isValid() const;
    /**
     * Setup this Viewporter to manage the @p viewporter.
     * When using Registry::createViewporter there is no need to call this
     * method.
     **/
    void setup(wp_viewporter *viewporter);
    /**
     * Releases the wp_viewporter interface.
     * After the interface has been released the Viewporter instance is no
     * longer valid and can be setup with another wp_viewporter interface.
     **/
    void release();

    /**
     * Sets the @p queue to use for creating a Viewport.
     **/
    void setEventQueue(EventQueue *queue);
    /**
     * @returns The event queue to use for creating a Viewport.
     **/
    EventQueue *eventQueue();

    /**
     * Creates and setup a new Viewport with @p parent.
     * @param surface The surface to use this Viewport with.
     * @param parent The parent to pass to the Viewport.
     * @returns The new created Viewport
     **/
    Viewport *createViewport(Surface *surface, QObject *parent = nullptr);

    operator wp_viewporter*();
    operator wp_viewporter*() const;

Q_SIGNALS:
    /**
     * The corresponding global for this interface on the Registry got removed.
     *
     * This signal gets only emitted if the Compositor got created by
     * Registry::createViewporter
     *
     * @since 5.66
     **/
    void removed();

private:
    class Private;
    QScopedPointer<Private> d;
};

/**
 * @short Wrapper for the wp_viewport interface.
 *
 * This class is a convenient wrapper for the wp_viewport interface.
 * To create a Viewport call Viewporter::createViewport.
 *
 **/
class WRAPLANDCLIENT_EXPORT Viewport : public QObject
{
    Q_OBJECT
public:
    ~Viewport() override;

    /**
     * Setup this Viewport to manage the @p viewport.
     * When using Viewporter::createViewport there is no need to call this
     * method.
     **/
    void setup(wp_viewport *viewport);
    /**
     * Releases the wp_viewport interface.
     * After the interface has been released the Viewport instance is no
     * longer valid and can be setup with another wp_viewport interface.
     **/
    void release();

    /**
     * @returns @c true if managing a wp_viewport.
     **/
    bool isValid() const;

    void setSourceRectangle(const QRectF &source);
    void setDestinationSize(const QSize &dest);

    operator wp_viewport*();
    operator wp_viewport*() const;

private:
    friend class Viewporter;
    explicit Viewport(QObject *parent = nullptr);
    class Private;
    QScopedPointer<Private> d;
};

}
}

