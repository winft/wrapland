/********************************************************************
Copyright 2016  Martin Gräßlin <mgraesslin@kde.org>

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
#ifndef TESTSERVER_H
#define TESTSERVER_H

#include <QHash>
#include <QObject>
#include <QPointF>
#include <QVector>

//STD
#include <memory>

class QElapsedTimer;
class QTimer;

namespace Wrapland
{
namespace Server
{
class D_isplay;
class Seat;
class ShellInterface;
class ShellSurfaceInterface;
}
}

class TestServer : public QObject
{
    Q_OBJECT
public:
    explicit TestServer(QObject *parent);
    virtual ~TestServer();

    void init();
    void startTestApp(const QString &app, const QStringList &arguments);

private:
    void repaint();

    Wrapland::Server::D_isplay *m_display = nullptr;
    Wrapland::Server::ShellInterface *m_shell = nullptr;
    Wrapland::Server::Seat *m_seat = nullptr;
    QVector<Wrapland::Server::ShellSurfaceInterface*> m_shellSurfaces;
    QTimer *m_repaintTimer;
    std::unique_ptr<QElapsedTimer> m_timeSinceStart;
    QPointF m_cursorPos;
    QHash<qint32, qint32> m_touchIdMapper;
};

#endif
