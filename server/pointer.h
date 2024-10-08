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

#include <QObject>

#include <Wrapland/Server/wraplandserver_export.h>
#include <memory>

namespace Wrapland::Server
{
class Client;
class Seat;

class Cursor;
class Seat;
class Surface;

enum class PointerAxisSource : std::uint8_t;

class WRAPLANDSERVER_EXPORT Pointer : public QObject
{
    Q_OBJECT
public:
    Seat* seat() const;
    Surface* focusedSurface() const;

    Cursor* cursor() const;
    Client* client() const;

Q_SIGNALS:
    void cursorChanged();
    void resourceDestroyed();

private:
    void setFocusedSurface(quint32 serial, Surface* surface);

    void motion(QPointF const& position);
    void buttonPressed(quint32 serial, quint32 button);
    void buttonReleased(quint32 serial, quint32 button);
    void
    axis(Qt::Orientation orientation, qreal delta, qint32 discreteDelta, PointerAxisSource source);
    void axis(Qt::Orientation orientation, quint32 delta);
    void
    relativeMotion(QSizeF const& delta, QSizeF const& deltaNonAccelerated, quint64 microseconds);
    void frame();

    friend class RelativePointerManagerV1;
    friend class PointerGesturesV1;
    friend class PointerConstraintsV1;

    friend class Seat;
    friend class pointer_pool;
    friend class touch_pool;
    Pointer(Client* client, uint32_t version, uint32_t id, Seat* seat);

    class Private;
    Private* d_ptr;
};

class WRAPLANDSERVER_EXPORT Cursor : public QObject
{
    Q_OBJECT
public:
    ~Cursor() override;

    QPoint hotspot() const;
    quint32 enteredSerial() const;

    Pointer* pointer() const;
    Surface* surface() const;

Q_SIGNALS:
    void hotspotChanged();
    void enteredSerialChanged();
    void surfaceChanged();
    void changed();

private:
    friend class Pointer;
    explicit Cursor(Pointer* pointer);

    class Private;
    std::unique_ptr<Private> d_ptr;
};

}

Q_DECLARE_METATYPE(Wrapland::Server::Pointer*)
