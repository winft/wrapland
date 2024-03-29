/****************************************************************************
Copyright 2015  Marco Martin <notmart@gmail.com>

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
#ifndef WRAPLAND_CLIENT_SLIDE_H
#define WRAPLAND_CLIENT_SLIDE_H

#include <QObject>
// STD
#include <memory>

#include <Wrapland/Client/wraplandclient_export.h>

struct org_kde_kwin_slide_manager;
struct org_kde_kwin_slide;

namespace Wrapland
{
namespace Client
{

class EventQueue;
class Slide;
class Surface;

/**
 * @short Wrapper for the org_kde_kwin_slide_manager interface.
 *
 * This class provides a convenient wrapper for the org_kde_kwin_slide_manager interface.
 *
 * Ask the compositor to move the surface from a location
 * to another with a slide animation.
 *
 * The from argument provides a clue about where the slide
 * animation begins, offset is the distance from screen
 * edge to begin the animation.
 *
 * To use this class one needs to interact with the Registry. There are two
 * possible ways to create the SlideManager interface:
 * @code
 * SlideManager *c = registry->createSlideManager(name, version);
 * @endcode
 *
 * This creates the SlideManager and sets it up directly. As an alternative this
 * can also be done in a more low level way:
 * @code
 * SlideManager *c = new SlideManager;
 * c->setup(registry->bindSlideManager(name, version));
 * @endcode
 *
 * The SlideManager can be used as a drop-in replacement for any org_kde_kwin_slide_manager
 * pointer as it provides matching cast operators.
 *
 * @see Registry
 **/
class WRAPLANDCLIENT_EXPORT SlideManager : public QObject
{
    Q_OBJECT
public:
    /**
     * Creates a new SlideManager.
     * Note: after constructing the SlideManager it is not yet valid and one needs
     * to call setup. In order to get a ready to use SlideManager prefer using
     * Registry::createSlideManager.
     **/
    explicit SlideManager(QObject* parent = nullptr);
    virtual ~SlideManager();

    /**
     * Setup this SlideManager to manage the @p slidemanager.
     * When using Registry::createSlideManager there is no need to call this
     * method.
     **/
    void setup(org_kde_kwin_slide_manager* slidemanager);
    /**
     * @returns @c true if managing a org_kde_kwin_slide_manager.
     **/
    bool isValid() const;
    /**
     * Releases the org_kde_kwin_slide_manager interface.
     * After the interface has been released the SlideManager instance is no
     * longer valid and can be setup with another org_kde_kwin_slide_manager interface.
     **/
    void release();

    /**
     * Sets the @p queue to use for creating objects with this SlideManager.
     **/
    void setEventQueue(EventQueue* queue);
    /**
     * @returns The event queue to use for creating objects with this SlideManager.
     **/
    EventQueue* eventQueue();

    Slide* createSlide(Surface* surface, QObject* parent = nullptr);

    void removeSlide(Surface* surface);

    operator org_kde_kwin_slide_manager*();
    operator org_kde_kwin_slide_manager*() const;

Q_SIGNALS:
    /**
     * The corresponding global for this interface on the Registry got removed.
     *
     * This signal gets only emitted if the SlideManager got created by
     * Registry::createSlideManager
     **/
    void removed();

private:
    class Private;
    std::unique_ptr<Private> d;
};

/**
 * TODO
 */
class WRAPLANDCLIENT_EXPORT Slide : public QObject
{
    Q_OBJECT
public:
    enum Location {
        Left = 0, /**< Slide from the left edge of the screen */
        Top,      /**< Slide from the top edge of the screen */
        Right,    /**< Slide from the bottom edge of the screen */
        Bottom,   /**< Slide from the bottom edge of the screen */
    };

    virtual ~Slide();

    /**
     * Setup this Slide to manage the @p slide.
     * When using SlideManager::createSlide there is no need to call this
     * method.
     **/
    void setup(org_kde_kwin_slide* slide);

    /**
     * @returns @c true if managing a org_kde_kwin_slide.
     **/
    bool isValid() const;

    /**
     * Releases the org_kde_kwin_slide interface.
     * After the interface has been released the Slide instance is no
     * longer valid and can be setup with another org_kde_kwin_slide interface.
     **/
    void release();

    void commit();

    /**
     * Set the location of the screen to slide the window from
     */
    void setLocation(Slide::Location location);

    /**
     * Set the offset from the screen edge
     * to make the window slide from
     */
    void setOffset(qint32 offset);

    operator org_kde_kwin_slide*();
    operator org_kde_kwin_slide*() const;

private:
    friend class SlideManager;
    explicit Slide(QObject* parent = nullptr);
    class Private;
    std::unique_ptr<Private> d;
};

}
}

#endif
