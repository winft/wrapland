/********************************************************************
Copyright © 2014  Martin Gräßlin <mgraesslin@kde.org>
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
#include "connection_thread.h"
#include "logging.h"

#include <QAbstractEventDispatcher>
#include <QGuiApplication>
#include <QDebug>
#include <QDir>
#include <QFileSystemWatcher>
#include <QMutex>
#include <QMutexLocker>
#include <QSocketNotifier>
#include <qpa/qplatformnativeinterface.h>

#include <errno.h>
#include <wayland-client-protocol.h>

namespace Wrapland
{

namespace Client
{

class Q_DECL_HIDDEN ConnectionThread::Private
{
public:
    Private(ConnectionThread *q);
    ~Private();

    void doEstablishConnection();
    void setupSocketNotifier();

    bool established = false;
    int error = 0;
    int protocolError = 0;

    wl_display *display = nullptr;
    int fd = -1;

    QString socketName;
    QDir runtimeDir;
    QScopedPointer<QSocketNotifier> socketNotifier;
    QScopedPointer<QFileSystemWatcher> socketWatcher;

    bool foreign = false;
    QMetaObject::Connection eventDispatcherConnection;

    static QVector<ConnectionThread*> connections;
    static QMutex mutex;
private:
    ConnectionThread *q;
};

QVector<ConnectionThread*> ConnectionThread::Private::connections = QVector<ConnectionThread*>{};
QMutex ConnectionThread::Private::mutex{QMutex::Recursive};


ConnectionThread::Private::Private(ConnectionThread *q)
    : socketName(QString::fromUtf8(qgetenv("WAYLAND_DISPLAY")))
    , runtimeDir(QString::fromUtf8(qgetenv("XDG_RUNTIME_DIR")))
    , q(q)
{
    if (socketName.isEmpty()) {
        socketName = QStringLiteral("wayland-0");
    }
    {
        QMutexLocker lock(&mutex);
        connections << q;
    }
}

ConnectionThread::Private::~Private()
{
    {
        QMutexLocker lock(&mutex);
        connections.removeOne(q);
    }
    if (display && !foreign) {
        if (established) {
            wl_display_flush(display);
        }
        wl_display_disconnect(display);
    }
}

void ConnectionThread::Private::doEstablishConnection()
{
    // To not leak memory we disconnect previous display objects.
    //
    // TODO: But in reality this API should better be constructed in a way that it is not possible
    // to reuse a ConnectionThread that was once connected to a display.
    if (display) {
        wl_display_disconnect(display);
    }
    if (fd != -1) {
        display = wl_display_connect_to_fd(fd);
    } else {
        display = wl_display_connect(socketName.toUtf8().constData());
    }
    if (!display) {
        qCWarning(WRAPLAND_CLIENT) << "Failed connecting to Wayland display";
        emit q->failed();
        return;
    }
    if (fd != -1) {
        qCDebug(WRAPLAND_CLIENT)
                << "Established connection to Wayland server over file descriptor:" << fd;
    } else {
        qCDebug(WRAPLAND_CLIENT) << "Established connection to Wayland server at:" << socketName;
    }

    // setup socket notifier
    setupSocketNotifier();

    established = true;
    emit q->establishedChanged(true);
}

void ConnectionThread::Private::setupSocketNotifier()
{
    const int fd = wl_display_get_fd(display);
    socketNotifier.reset(new QSocketNotifier(fd, QSocketNotifier::Read));
    QObject::connect(socketNotifier.data(), &QSocketNotifier::activated, q,
        [this](int count) {
            Q_UNUSED(count)
            if (!established) {
                return;
            }

            int ret = wl_display_dispatch(display);
            error = wl_display_get_error(display);

            if (ret < 0) {
                error = wl_display_get_error(display);
                Q_ASSERT(error);
                protocolError = wl_display_get_protocol_error(display, nullptr, nullptr);

                established = false;
                emit q->establishedChanged(false);
                socketNotifier.reset();
                return;
            }

            emit q->eventsRead();
        }
    );
}

ConnectionThread::ConnectionThread(QObject *parent)
    : QObject(parent)
    , d(new Private(this))
{
    d->eventDispatcherConnection = connect(QCoreApplication::eventDispatcher(), &QAbstractEventDispatcher::aboutToBlock, this,
        [this] {
            if (d->display) {
                wl_display_flush(d->display);
            }
        },
        Qt::DirectConnection);
}

ConnectionThread::ConnectionThread(wl_display *display, QObject *parent)
    : QObject(parent)
    , d(new Private(this))
{
    d->display = display;
    d->foreign = true;
}

ConnectionThread::~ConnectionThread()
{
    disconnect(d->eventDispatcherConnection);
}

ConnectionThread *ConnectionThread::fromApplication(QObject *parent)
{
    QPlatformNativeInterface *native = qApp->platformNativeInterface();
    if (!native) {
        return nullptr;
    }
    wl_display *display = reinterpret_cast<wl_display*>(native->nativeResourceForIntegration(QByteArrayLiteral("wl_display")));
    if (!display) {
        return nullptr;
    }
    auto *ct = new ConnectionThread(display, parent);
    connect(native, &QObject::destroyed, ct, [ct] { Q_EMIT ct->establishedChanged(false); } );
    return ct;
}

void ConnectionThread::establishConnection()
{
    d->error = 0;
    d->protocolError = 0;
    QMetaObject::invokeMethod(this, "doEstablishConnection", Qt::QueuedConnection);
}

void ConnectionThread::doEstablishConnection()
{
    d->doEstablishConnection();
}

void ConnectionThread::setSocketName(const QString &socketName)
{
    if (d->display) {
        // already initialized
        return;
    }
    d->socketName = socketName;
}

void ConnectionThread::setSocketFd(int fd)
{
    if (d->display) {
        // already initialized
        return;
    }
    d->fd = fd;
}

wl_display *ConnectionThread::display()
{
    return d->display;
}

QString ConnectionThread::socketName() const
{
    return d->socketName;
}

void ConnectionThread::flush()
{
    if (!d->established) {
        return;
    }
    wl_display_flush(d->display);
}

void ConnectionThread::roundtrip()
{
    if (!d->established) {
        return;
    }
    if (d->foreign) {
        // try to perform roundtrip through the QPA plugin if it's supported
        if (QPlatformNativeInterface *native = qApp->platformNativeInterface()) {
            // in case the platform provides a dedicated roundtrip function use that install of wl_display_roundtrip
            QFunctionPointer roundtripFunction = native->platformFunction(QByteArrayLiteral("roundtrip"));
            if (roundtripFunction) {
                roundtripFunction();
                return;
            }
        }
    }
    wl_display_roundtrip(d->display);
}

bool ConnectionThread::established() const
{
    return d->established;
}

int ConnectionThread::error() const
{
    return d->error;
}

bool ConnectionThread::hasProtocolError() const
{
    return d->error = EPROTO;
}

int ConnectionThread::protocolError() const
{
    return d->protocolError;
}

QVector<ConnectionThread*> ConnectionThread::connections()
{
    return Private::connections;
}

}
}
