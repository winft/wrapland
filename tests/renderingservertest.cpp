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
#include "../server/buffer.h"
#include "../server/compositor.h"
#include "../server/data_device_manager.h"
#include "../server/display.h"
#include "../server/keyboard_pool.h"
#include "../server/pointer_pool.h"
#include "../server/seat.h"
#include "../server/surface.h"
#include "../server/wl_output.h"
#include "../server/xdg_shell.h"
#include "../server/xdg_shell_toplevel.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QDateTime>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QWidget>
#include <QtConcurrent>

#include <iostream>
#include <unistd.h>

static int startXServer()
{
    const QByteArray process = QByteArrayLiteral("Xwayland");
    int pipeFds[2];
    if (pipe(pipeFds) != 0) {
        std::cerr << "FATAL ERROR failed to create pipe to start X Server " << process.constData()
                  << std::endl;
        exit(1);
    }

    pid_t pid = fork();
    if (pid == 0) {
        // child process - should be turned into Xwayland
        // writes to pipe, closes read side
        close(pipeFds[0]);
        char fdbuf[16];
        sprintf(fdbuf, "%d", pipeFds[1]);
        execlp(process.constData(),
               process.constData(),
               "-displayfd",
               fdbuf,
               "-rootless",
               (char*)nullptr);
        close(pipeFds[1]);
        exit(20);
    }
    // parent process - this is the wayland server
    // reads from pipe, closes write side
    close(pipeFds[1]);
    return pipeFds[0];
}

static void readDisplayFromPipe(int pipe)
{
    QFile readPipe;
    if (!readPipe.open(pipe, QIODevice::ReadOnly)) {
        std::cerr << "FATAL ERROR failed to open pipe to start X Server XWayland" << std::endl;
        exit(1);
    }
    QByteArray displayNumber = readPipe.readLine();

    displayNumber.prepend(QByteArray(":"));
    displayNumber.remove(displayNumber.size() - 1, 1);
    std::cout << "X-Server started on display " << displayNumber.constData() << std::endl;

    setenv("DISPLAY", displayNumber.constData(), true);

    // close our pipe
    close(pipe);
}

class CompositorWindow : public QWidget
{
    Q_OBJECT
public:
    explicit CompositorWindow(QWidget* parent = nullptr);
    virtual ~CompositorWindow();

    void surfaceCreated(Wrapland::Server::XdgShellToplevel* surface);

    void setSeat(QPointer<Wrapland::Server::Seat> const& seat);

protected:
    void paintEvent(QPaintEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    void updateFocus();
    QList<Wrapland::Server::XdgShellToplevel*> m_stackingOrder;
    QPointer<Wrapland::Server::Seat> m_seat;
};

CompositorWindow::CompositorWindow(QWidget* parent)
    : QWidget(parent)
{
    setMouseTracking(true);
}

CompositorWindow::~CompositorWindow() = default;

void CompositorWindow::surfaceCreated(Wrapland::Server::XdgShellToplevel* surface)
{
    using namespace Wrapland::Server;
    m_stackingOrder << surface;
    connect(surface, &XdgShellToplevel::fullscreenChanged, this, [surface, this](bool fullscreen) {
        if (fullscreen) {
            surface->configure(XdgShellSurface::State::Fullscreen, size());
        }
    });
    connect(surface, &XdgShellToplevel::maximizedChanged, this, [surface, this](bool maximized) {
        if (maximized) {
            surface->configure(XdgShellSurface::State::Maximized, size());
        }
    });
    connect(surface->surface()->surface(),
            &Wrapland::Server::Surface::committed,
            this,
            static_cast<void (CompositorWindow::*)()>(&CompositorWindow::update));
    connect(surface, &XdgShellToplevel::destroyed, this, [surface, this] {
        m_stackingOrder.removeAll(surface);
        updateFocus();
        update();
    });
    updateFocus();
}

void CompositorWindow::updateFocus()
{
    using namespace Wrapland::Server;
    if (!m_seat || m_stackingOrder.isEmpty()) {
        return;
    }
    auto it = std::find_if(
        m_stackingOrder.constBegin(), m_stackingOrder.constEnd(), [](XdgShellToplevel* s) {
            return s->surface()->surface()->state().buffer != nullptr;
        });
    if (it == m_stackingOrder.constEnd()) {
        return;
    }
    m_seat->pointers().set_focused_surface((*it)->surface()->surface());
    m_seat->setFocusedKeyboardSurface((*it)->surface()->surface());
}

void CompositorWindow::setSeat(QPointer<Wrapland::Server::Seat> const& seat)
{
    m_seat = seat;
}

void CompositorWindow::paintEvent(QPaintEvent* event)
{
    QWidget::paintEvent(event);
    QPainter p(this);
    for (auto s : m_stackingOrder) {
        if (auto b = s->surface()->surface()->state().buffer) {
            p.drawImage(QPoint(0, 0), b->shmImage()->createQImage());
            s->surface()->surface()->frameRendered(QDateTime::currentMSecsSinceEpoch());
        }
    }
}

void CompositorWindow::keyPressEvent(QKeyEvent* event)
{
    QWidget::keyPressEvent(event);
    if (!m_seat) {
        return;
    }
    if (!m_seat->keyboards().get_focus().surface) {
        updateFocus();
    }
    m_seat->setTimestamp(event->timestamp());
    m_seat->keyboards().key(event->nativeScanCode() - 8, Wrapland::Server::key_state::pressed);
}

void CompositorWindow::keyReleaseEvent(QKeyEvent* event)
{
    QWidget::keyReleaseEvent(event);
    if (!m_seat) {
        return;
    }
    m_seat->setTimestamp(event->timestamp());
    m_seat->keyboards().key(event->nativeScanCode() - 8, Wrapland::Server::key_state::released);
}

void CompositorWindow::mouseMoveEvent(QMouseEvent* event)
{
    QWidget::mouseMoveEvent(event);
    if (!m_seat->pointers().get_focus().surface) {
        updateFocus();
    }
    m_seat->setTimestamp(event->timestamp());
    m_seat->pointers().set_position(event->localPos().toPoint());
}

void CompositorWindow::mousePressEvent(QMouseEvent* event)
{
    QWidget::mousePressEvent(event);
    if (!m_seat->pointers().get_focus().surface) {
        if (!m_stackingOrder.isEmpty()) {
            m_seat->pointers().set_focused_surface(m_stackingOrder.last()->surface()->surface());
        }
    }
    m_seat->setTimestamp(event->timestamp());
    m_seat->pointers().button_pressed(event->button());
}

void CompositorWindow::mouseReleaseEvent(QMouseEvent* event)
{
    QWidget::mouseReleaseEvent(event);
    m_seat->setTimestamp(event->timestamp());
    m_seat->pointers().button_released(event->button());
}

void CompositorWindow::wheelEvent(QWheelEvent* event)
{
    QWidget::wheelEvent(event);
    m_seat->setTimestamp(event->timestamp());
    QPoint const& angle = event->angleDelta() / (8 * 15);
    if (angle.x() != 0) {
        m_seat->pointers().send_axis(Qt::Horizontal, angle.x());
    }
    if (angle.y() != 0) {
        m_seat->pointers().send_axis(Qt::Vertical, angle.y());
    }
}

int main(int argc, char** argv)
{
    using namespace Wrapland::Server;
    QApplication app(argc, argv);

    QCommandLineParser parser;
    parser.addHelpOption();
    QCommandLineOption xwaylandOption(QStringList{QStringLiteral("x"), QStringLiteral("xwayland")},
                                      QStringLiteral("Start a rootless Xwayland server"));
    parser.addOption(xwaylandOption);
    parser.process(app);

    Display display;
    display.start();

    auto data_device_manager = display.createDataDeviceManager();
    auto compositor = display.createCompositor();
    auto shell = display.createXdgShell();
    display.createShm();

    Wrapland::Server::output_metadata meta{.physical_size = {269, 202}};
    auto output = std::make_unique<Wrapland::Server::output>(meta, &display);
    const QSize windowSize(1024, 768);
    output->add_mode(output_mode{windowSize});

    auto seat = display.createSeat();
    seat->setHasKeyboard(true);
    seat->setHasPointer(true);
    seat->setName("testSeat0");

    CompositorWindow compositorWindow;
    compositorWindow.setSeat(seat.get());
    compositorWindow.setMinimumSize(windowSize);
    compositorWindow.setMaximumSize(windowSize);
    compositorWindow.setGeometry(QRect(QPoint(0, 0), windowSize));
    compositorWindow.show();
    QObject::connect(shell.get(),
                     &XdgShell::toplevelCreated,
                     &compositorWindow,
                     &CompositorWindow::surfaceCreated);

    // start XWayland
    if (parser.isSet(xwaylandOption)) {
        // starts XWayland by forking and opening a pipe
        int const pipe = startXServer();
        if (pipe == -1) {
            exit(1);
        }

        QtConcurrent::run([pipe] { readDisplayFromPipe(pipe); });
    }

    return app.exec();
}

#include "renderingservertest.moc"
