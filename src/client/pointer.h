/********************************************************************
Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>

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
#ifndef WAYLAND_POINTER_H
#define WAYLAND_POINTER_H

#include <QObject>
#include <QPoint>
// STD
#include <Wrapland/Client/wraplandclient_export.h>
#include <memory>

struct wl_pointer;

namespace Wrapland
{
namespace Client
{

class Surface;

/**
 * @short Wrapper for the wl_pointer interface.
 *
 * This class is a convenient wrapper for the wl_pointer interface.
 *
 * To create an instance use Seat::createPointer.
 *
 * @see Seat
 **/
class WRAPLANDCLIENT_EXPORT Pointer : public QObject
{
    Q_OBJECT
public:
    enum class ButtonState {
        Released,
        Pressed,
    };
    enum class Axis {
        Vertical,
        Horizontal,
    };
    enum class AxisSource {
        Wheel,
        Finger,
        Continuous,
        WheelTilt,
    };
    explicit Pointer(QObject* parent = nullptr);
    virtual ~Pointer();

    /**
     * @returns @c true if managing a wl_pointer.
     **/
    bool isValid() const;
    /**
     * Setup this Pointer to manage the @p pointer.
     * When using Seat::createPointer there is no need to call this
     * method.
     **/
    void setup(wl_pointer* pointer);
    /**
     * Releases the wl_pointer interface.
     * After the interface has been released the Pointer instance is no
     * longer valid and can be setup with another wl_pointer interface.
     *
     * This method is automatically invoked when the Seat which created this
     * Pointer gets released.
     **/
    void release();

    /**
     * Sets the cursor image for this Pointer.
     *
     * This has only an effect if a Surface of the same client is focused.
     *
     * @param surface The Surface pointing to the image data, if @c null the cursor will be hidden
     * @param hotspot The hotspot of the cursor image
     * @see hideCursor
     * @since 5.3
     **/
    void setCursor(Surface* surface, QPoint const& hotspot = QPoint());
    /**
     * Hides the cursor. Same as calling setCursor with @c null for surface.
     * @see setCursor
     * @since 5.3
     **/
    void hideCursor();

    /**
     * @returns The Surface the Pointer is on, may be @c null.
     **/
    Surface* enteredSurface() const;
    /**
     * @overload
     **/
    Surface* enteredSurface();

    operator wl_pointer*();
    operator wl_pointer*() const;

Q_SIGNALS:
    /**
     * Notification that this seat's pointer is focused on a certain surface.
     *
     * When an seat's focus enters a surface, the pointer image is undefined
     * and a client should respond to this event by setting an appropriate pointer
     * image with the set_cursor request.
     *
     * @param serial The serial for this enter
     * @param relativeToSurface Coordinates relative to the upper-left corner of the Surface.
     **/
    void entered(quint32 serial, QPointF const& relativeToSurface);
    /**
     * Notification that this seat's pointer is no longer focused on a certain surface.
     *
     * The leave notification is sent before the enter notification for the new focus.
     *
     * @param serial The serial of this leave event
     **/
    void left(quint32 serial);
    /**
     * Notification of pointer location change.
     *
     * @param relativeToSurface  Coordinates relative to the upper-left corner of the entered
     * Surface.
     * @param time timestamp with millisecond granularity
     **/
    void motion(QPointF const& relativeToSurface, quint32 time);
    /**
     * Mouse button click and release notifications.
     *
     * The location of the click is given by the last motion or enter event.
     *
     * @param serial The serial of this button state change
     * @param time timestamp with millisecond granularity, with an undefined base.
     * @param button The button which got changed
     * @param state @c Released or @c Pressed
     **/
    void buttonStateChanged(quint32 serial,
                            quint32 time,
                            quint32 button,
                            Wrapland::Client::Pointer::ButtonState state);
    /**
     * Scroll and other axis notifications.
     *
     * @param time timestamp with millisecond granularity
     * @param axis @c Vertical or @c Horizontal
     * @param delta
     **/
    void axisChanged(quint32 time, Wrapland::Client::Pointer::Axis axis, qreal delta);
    /**
     * Indicates the source of scroll and other axes.
     *
     * @since 0.0.559
     **/
    void axisSourceChanged(Wrapland::Client::Pointer::AxisSource source);
    /**
     * Discrete step information for scroll and other axes.
     *
     * @since 0.0.559
     **/
    void axisDiscreteChanged(Wrapland::Client::Pointer::Axis axis, qint32 discreteDelta);
    /**
     * Stop notification for scroll and other axes.
     *
     * @since 0.0.559
     **/
    void axisStopped(quint32 time, Wrapland::Client::Pointer::Axis axis);

    /**
     * Indicates the end of a set of events that logically belong together.
     * A client is expected to accumulate the data in all events within the
     * frame before proceeding.
     * @since 0.0.545
     **/
    void frame();

private:
    class Private;
    std::unique_ptr<Private> d;
};

}
}

Q_DECLARE_METATYPE(Wrapland::Client::Pointer::ButtonState)
Q_DECLARE_METATYPE(Wrapland::Client::Pointer::Axis)
Q_DECLARE_METATYPE(Wrapland::Client::Pointer::AxisSource)

#endif
