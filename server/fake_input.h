/********************************************************************
Copyright 2020  Faveraux Adrien <ad1rie3@hotmail.fr>

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
#include <QPointF>
#include <QSizeF>
#include <memory>

#include <Wrapland/Server/wraplandserver_export.h>

namespace Wrapland::Server
{

class Display;
class FakeInputDevice;

class WRAPLANDSERVER_EXPORT FakeInput : public QObject
{
    Q_OBJECT
public:
    explicit FakeInput(Display* display);
    ~FakeInput() override;

Q_SIGNALS:
    void deviceCreated(Wrapland::Server::FakeInputDevice* device);
    void device_destroyed(Wrapland::Server::FakeInputDevice* device);

private:
    class Private;
    std::unique_ptr<Private> d_ptr;
};

class WRAPLANDSERVER_EXPORT FakeInputDevice : public QObject
{
    Q_OBJECT
public:
    ~FakeInputDevice() override;

    void setAuthentication(bool authenticated);
    bool isAuthenticated() const;

Q_SIGNALS:
    void authenticationRequested(const QString& application, const QString& reason);
    void pointerMotionRequested(const QSizeF& delta);
    void pointerMotionAbsoluteRequested(const QPointF& pos);
    void pointerButtonPressRequested(quint32 button);
    void pointerButtonReleaseRequested(quint32 button);
    void pointerAxisRequested(Qt::Orientation orientation, qreal delta);
    void touchDownRequested(quint32 id, const QPointF& pos);
    void touchMotionRequested(quint32 id, const QPointF& pos);
    void touchUpRequested(quint32 id);
    void touchCancelRequested();
    void touchFrameRequested();
    void keyboardKeyPressRequested(quint32 key);
    void keyboardKeyReleaseRequested(quint32 key);

private:
    friend class FakeInput;
    class Private;

    explicit FakeInputDevice(std::unique_ptr<Private> p);
    std::unique_ptr<Private> d_ptr;
};

}

Q_DECLARE_METATYPE(Wrapland::Server::FakeInputDevice*)
