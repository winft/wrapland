/********************************************************************
Copyright 2015  Martin Gräßlin <mgraesslin@kde.org>
Copyright 2015  Marco Martin <mart@kde.org>

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
#ifndef WRAPLAND_BLUR_H
#define WRAPLAND_BLUR_H

#include "buffer.h"

#include <QObject>
#include <QPoint>
#include <QSize>

// STD
#include <memory>

#include <Wrapland/Client/wraplandclient_export.h>

struct wl_buffer;
struct wl_region;
struct org_kde_kwin_blur;
struct org_kde_kwin_blur_manager;

class QMarginsF;
class QWindow;

namespace Wrapland
{
namespace Client
{

class EventQueue;
class Blur;
class Surface;
class Region;

/**
 * TODO
 */
class WRAPLANDCLIENT_EXPORT BlurManager : public QObject
{
    Q_OBJECT
public:
    /**
     * Creates a new BlurManager.
     * Note: after constructing the BlurManager it is not yet valid and one needs
     * to call setup. In order to get a ready to use BlurManager prefer using
     * Registry::createBlurManager.
     **/
    explicit BlurManager(QObject* parent = nullptr);
    virtual ~BlurManager();

    /**
     * @returns @c true if managing a org_kde_kwin_blur_manager.
     **/
    bool isValid() const;
    /**
     * Setup this BlurManager to manage the @p compositor.
     * When using Registry::createBlurManager there is no need to call this
     * method.
     **/
    void setup(org_kde_kwin_blur_manager* compositor);
    /**
     * Releases the org_kde_kwin_blur_manager interface.
     * After the interface has been released the BlurManager instance is no
     * longer valid and can be setup with another org_kde_kwin_blur_manager interface.
     **/
    void release();

    /**
     * Sets the @p queue to use for creating a Blur.
     **/
    void setEventQueue(EventQueue* queue);
    /**
     * @returns The event queue to use for creating a Blur.
     **/
    EventQueue* eventQueue();

    /**
     * Creates and setup a new Blur with @p parent.
     * @param parent The parent to pass to the Blur.
     * @returns The new created Blur
     **/
    Blur* createBlur(Surface* surface, QObject* parent = nullptr);
    void removeBlur(Surface* surface);

    operator org_kde_kwin_blur_manager*();
    operator org_kde_kwin_blur_manager*() const;

Q_SIGNALS:
    /**
     * The corresponding global for this interface on the Registry got removed.
     *
     * This signal gets only emitted if the BlurManager got created by
     * Registry::createBlurManager
     *
     * @since 5.5
     **/
    void removed();

private:
    class Private;
    std::unique_ptr<Private> d;
};

/**
 * @short Wrapper for the org_kde_kwin_blur interface.
 *
 * This class is a convenient wrapper for the org_kde_kwin_blur interface.
 * To create a Blur call BlurManager::createBlur.
 *
 * The main purpose of this class is to setup the next frame which
 * should be rendered. Therefore it provides methods to add damage
 * and to attach a new Buffer and to finalize the frame by calling
 * commit.
 *
 * @see BlurManager
 **/
class WRAPLANDCLIENT_EXPORT Blur : public QObject
{
    Q_OBJECT
public:
    virtual ~Blur();

    /**
     * Setup this Blur to manage the @p blur.
     * When using BlurManager::createBlur there is no need to call this
     * method.
     **/
    void setup(org_kde_kwin_blur* blur);
    /**
     * Releases the org_kde_kwin_blur interface.
     * After the interface has been released the Blur instance is no
     * longer valid and can be setup with another org_kde_kwin_blur interface.
     **/
    void release();

    /**
     * @returns @c true if managing a org_kde_kwin_blur.
     **/
    bool isValid() const;

    void commit();

    /**
     * Sets the area of the window that will have a blurred
     * background.
     * The region will have to be created with
     * Compositor::createRegion(QRegion)
     */
    void setRegion(Region* region);

    operator org_kde_kwin_blur*();
    operator org_kde_kwin_blur*() const;

private:
    friend class BlurManager;
    explicit Blur(QObject* parent = nullptr);
    class Private;
    std::unique_ptr<Private> d;
};

}
}

#endif
