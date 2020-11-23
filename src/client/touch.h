/********************************************************************
Copyright 2015  Martin Gräßlin <mgraesslin@kde.org>

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
#ifndef WAYLAND_TOUCH_H
#define WAYLAND_TOUCH_H

#include <QObject>
#include <QPoint>
// STD
#include <Wrapland/Client/wraplandclient_export.h>
#include <memory>

struct wl_touch;

namespace Wrapland
{
namespace Client
{

class Surface;
class Touch;

/**
 * TODO
 */
class WRAPLANDCLIENT_EXPORT TouchPoint
{
public:
    virtual ~TouchPoint();

    /**
     * Unique in the scope of all TouchPoints currently being down.
     * As soon as the TouchPoint is now longer down another TouchPoint
     * might get assigned the id.
     **/
    qint32 id() const;
    /**
     * The serial when the down event happened.
     **/
    quint32 downSerial() const;
    /**
     * The serial when the up event happened.
     **/
    quint32 upSerial() const;
    /**
     * Most recent timestamp
     **/
    quint32 time() const;
    /**
     * All timestamps, references the positions.
     * That is each position has a timestamp.
     **/
    QVector<quint32> timestamps() const;
    /**
     * Most recent position
     **/
    QPointF position() const;
    /**
     * All positions this TouchPoint had, updated with each move.
     **/
    QVector<QPointF> positions() const;
    /**
     * The Surface this TouchPoint happened on.
     **/
    QPointer<Surface> surface() const;
    /**
     * @c true if currently down, @c false otherwise.
     **/
    bool isDown() const;

private:
    friend class Touch;
    explicit TouchPoint();
    class Private;
    std::unique_ptr<Private> d;
};

/**
 * @short Wrapper for the wl_touch interface.
 *
 * This class is a convenient wrapper for the wl_touch interface.
 *
 * To create an instance use Seat::createTouch.
 *
 * @see Seat
 **/
class WRAPLANDCLIENT_EXPORT Touch : public QObject
{
    Q_OBJECT
public:
    explicit Touch(QObject* parent = nullptr);
    virtual ~Touch();

    /**
     * @returns @c true if managing a wl_pointer.
     **/
    bool isValid() const;
    /**
     * Setup this Touch to manage the @p touch.
     * When using Seat::createTouch there is no need to call this
     * method.
     **/
    void setup(wl_touch* touch);
    /**
     * Releases the wl_touch interface.
     * After the interface has been released the Touch instance is no
     * longer valid and can be setup with another wl_touch interface.
     *
     * This method is automatically invoked when the Seat which created this
     * Touch gets released.
     **/
    void release();

    /**
     * The TouchPoints of the latest touch event sequence.
     * Only valid till the next touch event sequence is started
     **/
    QVector<TouchPoint*> sequence() const;

    operator wl_touch*();
    operator wl_touch*() const;

Q_SIGNALS:
    /**
     * A new touch sequence is started. The previous sequence is discarded.
     * @param startPoint The first point which started the sequence
     **/
    void sequenceStarted(Wrapland::Client::TouchPoint* startPoint);
    /**
     * Sent if the compositor decides the touch stream is a global
     * gesture.
     **/
    void sequenceCanceled();
    /**
     * Emitted once all touch points are no longer down.
     **/
    void sequenceEnded();
    /**
     * Indicates the end of a contact point list.
     **/
    void frameEnded();
    /**
     * TouchPoint @p point got added to the sequence.
     **/
    void pointAdded(Wrapland::Client::TouchPoint* point);
    /**
     * TouchPoint @p point is no longer down.
     * A new TouchPoint might reuse the Id of the @p point.
     **/
    void pointRemoved(Wrapland::Client::TouchPoint* point);
    /**
     * TouchPoint @p point moved.
     **/
    void pointMoved(Wrapland::Client::TouchPoint* point);

private:
    class Private;
    std::unique_ptr<Private> d;
};

}
}

Q_DECLARE_METATYPE(Wrapland::Client::TouchPoint*)

#endif
