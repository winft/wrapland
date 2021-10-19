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
#include "testserver.h"

#include "../../../server/compositor.h"
#include "../../../server/display.h"
#include "../../../server/pointer_pool.h"
#include "../../../server/seat.h"
#include "../../../server/subcompositor.h"
#include "../../../server/surface.h"
#include "../../../server/touch_pool.h"
#include "../../../server/wl_output.h"

#include "../../../server/fake_input.h"
#include "../../../server/kde_idle.h"
#include "../../../server/xdg_shell.h"
#include "../../../server/xdg_shell_toplevel.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QProcess>
#include <QTimer>
// system
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

using namespace Wrapland::Server;

TestServer::TestServer(QObject* parent)
    : QObject(parent)
    , m_repaintTimer(new QTimer(this))
    , m_timeSinceStart(new QElapsedTimer)
    , m_cursorPos(QPointF(0, 0))
{
}

TestServer::~TestServer() = default;

void TestServer::init()
{
    Q_ASSERT(!m_display);
    m_display.reset(new Display);
    m_display->start(Display::StartMode::ConnectClientsOnly);
    m_display->createShm();
    m_display->createCompositor();
    globals.xdg_shell = m_display->createXdgShell();

    connect(globals.xdg_shell.get(),
            &XdgShell::toplevelCreated,
            this,
            [this](XdgShellToplevel* surface) {
                m_shellSurfaces << surface;
                // TODO: pass keyboard/pointer/touch focus on mapped
                connect(surface, &XdgShellToplevel::resourceDestroyed, this, [this, surface] {
                    m_shellSurfaces.removeOne(surface);
                });
            });

    globals.seats.emplace_back(m_display->createSeat());
    globals.seats.front()->setHasKeyboard(true);
    globals.seats.front()->setHasPointer(true);
    globals.seats.front()->setHasTouch(true);

    m_display->createDataDeviceManager();
    m_display->createIdle();
    m_display->createSubCompositor();

    globals.outputs.push_back(std::make_unique<Wrapland::Server::Output>(m_display.get()));
    const QSize size(1280, 1024);
    globals.outputs.back()->set_geometry(QRectF(QPoint(0, 0), size));
    globals.outputs.back()->set_physical_size(size / 3.8);
    globals.outputs.back()->add_mode(Output::Mode{size});

    globals.fake_input = m_display->createFakeInput();
    connect(
        globals.fake_input.get(), &FakeInput::deviceCreated, this, [this](FakeInputDevice* device) {
            device->setAuthentication(true);
            connect(device,
                    &FakeInputDevice::pointerMotionRequested,
                    this,
                    [this](const QSizeF& delta) {
                        globals.seats.front()->setTimestamp(m_timeSinceStart->elapsed());
                        m_cursorPos = m_cursorPos + QPointF(delta.width(), delta.height());
                        globals.seats.front()->pointers().set_position(m_cursorPos);
                    });
            connect(device,
                    &FakeInputDevice::pointerButtonPressRequested,
                    this,
                    [this](quint32 button) {
                        globals.seats.front()->setTimestamp(m_timeSinceStart->elapsed());
                        globals.seats.front()->pointers().button_pressed(button);
                    });
            connect(device,
                    &FakeInputDevice::pointerButtonReleaseRequested,
                    this,
                    [this](quint32 button) {
                        globals.seats.front()->setTimestamp(m_timeSinceStart->elapsed());
                        globals.seats.front()->pointers().button_released(button);
                    });
            connect(device,
                    &FakeInputDevice::pointerAxisRequested,
                    this,
                    [this](Qt::Orientation orientation, qreal delta) {
                        globals.seats.front()->setTimestamp(m_timeSinceStart->elapsed());
                        globals.seats.front()->pointers().send_axis(orientation, delta);
                    });
            connect(device,
                    &FakeInputDevice::touchDownRequested,
                    this,
                    [this](quint32 id, const QPointF& pos) {
                        globals.seats.front()->setTimestamp(m_timeSinceStart->elapsed());
                        m_touchIdMapper.insert(id,
                                               globals.seats.front()->touches().touch_down(pos));
                    });
            connect(device,
                    &FakeInputDevice::touchMotionRequested,
                    this,
                    [this](quint32 id, const QPointF& pos) {
                        globals.seats.front()->setTimestamp(m_timeSinceStart->elapsed());
                        const auto it = m_touchIdMapper.constFind(id);
                        if (it != m_touchIdMapper.constEnd()) {
                            globals.seats.front()->touches().touch_move(it.value(), pos);
                        }
                    });
            connect(device, &FakeInputDevice::touchUpRequested, this, [this](quint32 id) {
                globals.seats.front()->setTimestamp(m_timeSinceStart->elapsed());
                const auto it = m_touchIdMapper.find(id);
                if (it != m_touchIdMapper.end()) {
                    globals.seats.front()->touches().touch_up(it.value());
                    m_touchIdMapper.erase(it);
                }
            });
            connect(device, &FakeInputDevice::touchCancelRequested, this, [this] {
                globals.seats.front()->setTimestamp(m_timeSinceStart->elapsed());
                globals.seats.front()->touches().cancel_sequence();
            });
            connect(device, &FakeInputDevice::touchFrameRequested, this, [this] {
                globals.seats.front()->setTimestamp(m_timeSinceStart->elapsed());
                globals.seats.front()->touches().touch_frame();
            });
        });

    m_repaintTimer->setInterval(1000 / 60);
    connect(m_repaintTimer, &QTimer::timeout, this, &TestServer::repaint);
    m_repaintTimer->start();
    m_timeSinceStart->start();
}

void TestServer::startTestApp(const QString& app, const QStringList& arguments)
{
    int sx[2];
    if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sx) < 0) {
        QCoreApplication::instance()->exit(1);
        return;
    }
    m_display->createClient(sx[0]);
    int socket = dup(sx[1]);
    if (socket == -1) {
        QCoreApplication::instance()->exit(1);
        return;
    }
    QProcess* p = new QProcess(this);
    p->setProcessChannelMode(QProcess::ForwardedChannels);
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("wayland"));
    environment.insert(QStringLiteral("WAYLAND_SOCKET"),
                       QString::fromUtf8(QByteArray::number(socket)));
    p->setProcessEnvironment(environment);
    auto finishedSignal
        = static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished);
    connect(p, finishedSignal, QCoreApplication::instance(), &QCoreApplication::exit);
    connect(p, &QProcess::errorOccurred, this, [] { QCoreApplication::instance()->exit(1); });
    p->start(app, arguments);
}

void TestServer::repaint()
{
    for (auto it = m_shellSurfaces.constBegin(), end = m_shellSurfaces.constEnd(); it != end;
         ++it) {
        (*it)->surface()->surface()->frameRendered(m_timeSinceStart->elapsed());
    }
}
