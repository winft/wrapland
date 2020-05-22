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
#include <QRegion>

#include <Wrapland/Server/wraplandserver_export.h>
#include <memory>

namespace Wrapland::Server
{
class Client;
class Display;
class Pointer;

class WRAPLANDSERVER_EXPORT PointerConstraintsV1 : public QObject
{
    Q_OBJECT
public:
    ~PointerConstraintsV1() override;

private:
    friend class Display;
    explicit PointerConstraintsV1(Display* display, QObject* parent = nullptr);

    class Private;
    std::unique_ptr<Private> d_ptr;
};

class WRAPLANDSERVER_EXPORT LockedPointerV1 : public QObject
{
    Q_OBJECT
public:
    enum class LifeTime {
        OneShot,
        Persistent,
    };

    LifeTime lifeTime() const;
    QRegion region() const;
    QPointF cursorPositionHint() const;
    bool isLocked() const;
    void setLocked(bool locked);

Q_SIGNALS:
    void resourceDestroyed();
    void regionChanged();
    void cursorPositionHintChanged();
    void lockedChanged();

private:
    friend class PointerConstraintsV1;
    friend class Surface;
    LockedPointerV1(Client* client,
                    uint32_t version,
                    uint32_t id,
                    PointerConstraintsV1* constraints);

    class Private;
    Private* d_ptr;
};

class WRAPLANDSERVER_EXPORT ConfinedPointerV1 : public QObject
{
    Q_OBJECT
public:
    enum class LifeTime {
        OneShot,
        Persistent,
    };

    LifeTime lifeTime() const;
    QRegion region() const;
    bool isConfined() const;
    void setConfined(bool confined);

Q_SIGNALS:
    void resourceDestroyed();
    void regionChanged();
    void confinedChanged();

private:
    friend class PointerConstraintsV1;
    friend class Surface;
    ConfinedPointerV1(Client* client,
                      uint32_t version,
                      uint32_t id,
                      PointerConstraintsV1* constraints);

    class Private;
    Private* d_ptr;
};

}
