/****************************************************************************
Copyright 2018  Marco Martin <notmart@gmail.com>

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
#include "plasmavirtualdesktop.h"
#include "event_queue.h"
#include "wayland_pointer_p.h"

#include <QDebug>
#include <QMap>

#include <wayland-plasma-virtual-desktop-client-protocol.h>

namespace Wrapland
{
namespace Client
{

class Q_DECL_HIDDEN PlasmaVirtualDesktopManagement::Private
{
public:
    Private(PlasmaVirtualDesktopManagement* q);

    void setup(org_kde_plasma_virtual_desktop_management* arg);

    WaylandPointer<org_kde_plasma_virtual_desktop_management,
                   org_kde_plasma_virtual_desktop_management_destroy>
        plasmavirtualdesktopmanagement;
    EventQueue* queue = nullptr;

    quint32 rows = 1;
    QList<PlasmaVirtualDesktop*> desktops;

    inline QList<PlasmaVirtualDesktop*>::const_iterator constFindDesktop(QString const& id);
    inline QList<PlasmaVirtualDesktop*>::iterator findDesktop(QString const& id);

private:
    static void createdCallback(
        void* data,
        org_kde_plasma_virtual_desktop_management* org_kde_plasma_virtual_desktop_management,
        char const* id,
        uint32_t position);
    static void removedCallback(
        void* data,
        org_kde_plasma_virtual_desktop_management* org_kde_plasma_virtual_desktop_management,
        char const* id);
    static void rowsCallback(
        void* data,
        org_kde_plasma_virtual_desktop_management* org_kde_plasma_virtual_desktop_management,
        uint32_t rows);
    static void doneCallback(
        void* data,
        org_kde_plasma_virtual_desktop_management* org_kde_plasma_virtual_desktop_management);

    PlasmaVirtualDesktopManagement* q;

    static org_kde_plasma_virtual_desktop_management_listener const s_listener;
};

class Q_DECL_HIDDEN PlasmaVirtualDesktop::Private
{
public:
    Private(PlasmaVirtualDesktop* q);

    void setup(org_kde_plasma_virtual_desktop* arg);

    WaylandPointer<org_kde_plasma_virtual_desktop, org_kde_plasma_virtual_desktop_destroy>
        plasmavirtualdesktop;

    QString id;
    QString name;
    bool active = false;

private:
    PlasmaVirtualDesktop* q;

private:
    static void idCallback(void* data,
                           org_kde_plasma_virtual_desktop* org_kde_plasma_virtual_desktop,
                           char const* id);
    static void nameCallback(void* data,
                             org_kde_plasma_virtual_desktop* org_kde_plasma_virtual_desktop,
                             char const* name);

    static void activatedCallback(void* data,
                                  org_kde_plasma_virtual_desktop* org_kde_plasma_virtual_desktop);
    static void deactivatedCallback(void* data,
                                    org_kde_plasma_virtual_desktop* org_kde_plasma_virtual_desktop);
    static void doneCallback(void* data,
                             org_kde_plasma_virtual_desktop* org_kde_plasma_virtual_desktop);
    static void removedCallback(void* data,
                                org_kde_plasma_virtual_desktop* org_kde_plasma_virtual_desktop);

    static org_kde_plasma_virtual_desktop_listener const s_listener;
};

inline QList<PlasmaVirtualDesktop*>::const_iterator
PlasmaVirtualDesktopManagement::Private::constFindDesktop(QString const& id)
{
    return std::find_if(desktops.constBegin(),
                        desktops.constEnd(),
                        [id](PlasmaVirtualDesktop const* desk) { return desk->id() == id; });
}

inline QList<PlasmaVirtualDesktop*>::iterator
PlasmaVirtualDesktopManagement::Private::findDesktop(QString const& id)
{
    return std::find_if(desktops.begin(), desktops.end(), [id](PlasmaVirtualDesktop const* desk) {
        return desk->id() == id;
    });
}

org_kde_plasma_virtual_desktop_management_listener const
    PlasmaVirtualDesktopManagement::Private::s_listener
    = {
        createdCallback,
        removedCallback,
        doneCallback,
        rowsCallback,
};

void PlasmaVirtualDesktopManagement::Private::createdCallback(
    void* data,
    org_kde_plasma_virtual_desktop_management* org_kde_plasma_virtual_desktop_management,
    char const* id,
    uint32_t position)
{
    auto p = reinterpret_cast<PlasmaVirtualDesktopManagement::Private*>(data);
    Q_ASSERT(p->plasmavirtualdesktopmanagement == org_kde_plasma_virtual_desktop_management);
    QString const stringId = QString::fromUtf8(id);
    PlasmaVirtualDesktop* vd = p->q->getVirtualDesktop(stringId);
    Q_ASSERT(vd);

    p->desktops.insert(position, vd);
    // TODO: emit a lot of desktopMoved?

    Q_EMIT p->q->desktopCreated(stringId, position);
}

void PlasmaVirtualDesktopManagement::Private::removedCallback(
    void* data,
    org_kde_plasma_virtual_desktop_management* org_kde_plasma_virtual_desktop_management,
    char const* id)
{
    auto p = reinterpret_cast<PlasmaVirtualDesktopManagement::Private*>(data);
    Q_ASSERT(p->plasmavirtualdesktopmanagement == org_kde_plasma_virtual_desktop_management);
    QString const stringId = QString::fromUtf8(id);
    PlasmaVirtualDesktop* vd = p->q->getVirtualDesktop(stringId);
    // TODO: emit a lot of desktopMoved?
    Q_ASSERT(vd);
    auto i = p->findDesktop(stringId);
    p->desktops.erase(i);
    vd->release();
    vd->deleteLater();
    Q_EMIT p->q->desktopRemoved(stringId);
}

void PlasmaVirtualDesktopManagement::Private::rowsCallback(
    void* data,
    org_kde_plasma_virtual_desktop_management* org_kde_plasma_virtual_desktop_management,
    uint32_t rows)
{
    auto p = reinterpret_cast<PlasmaVirtualDesktopManagement::Private*>(data);
    Q_ASSERT(p->plasmavirtualdesktopmanagement == org_kde_plasma_virtual_desktop_management);
    if (rows == 0) {
        return;
    }
    p->rows = rows;
    Q_EMIT p->q->rowsChanged(rows);
}

void PlasmaVirtualDesktopManagement::Private::doneCallback(
    void* data,
    org_kde_plasma_virtual_desktop_management* org_kde_plasma_virtual_desktop_management)
{
    auto p = reinterpret_cast<PlasmaVirtualDesktopManagement::Private*>(data);
    Q_ASSERT(p->plasmavirtualdesktopmanagement == org_kde_plasma_virtual_desktop_management);
    Q_EMIT p->q->done();
}

PlasmaVirtualDesktopManagement::PlasmaVirtualDesktopManagement(QObject* parent)
    : QObject(parent)
    , d(new Private(this))
{
}

PlasmaVirtualDesktopManagement::Private::Private(PlasmaVirtualDesktopManagement* q)
    : q(q)
{
}

void PlasmaVirtualDesktopManagement::Private::setup(org_kde_plasma_virtual_desktop_management* arg)
{
    Q_ASSERT(arg);
    Q_ASSERT(!plasmavirtualdesktopmanagement);
    plasmavirtualdesktopmanagement.setup(arg);
    org_kde_plasma_virtual_desktop_management_add_listener(
        plasmavirtualdesktopmanagement, &s_listener, this);
}

PlasmaVirtualDesktopManagement::~PlasmaVirtualDesktopManagement()
{
    release();
}

void PlasmaVirtualDesktopManagement::setup(
    org_kde_plasma_virtual_desktop_management* plasmavirtualdesktopmanagement)
{
    d->setup(plasmavirtualdesktopmanagement);
}

void PlasmaVirtualDesktopManagement::release()
{
    d->plasmavirtualdesktopmanagement.release();
}

PlasmaVirtualDesktopManagement::operator org_kde_plasma_virtual_desktop_management*()
{
    return d->plasmavirtualdesktopmanagement;
}

PlasmaVirtualDesktopManagement::operator org_kde_plasma_virtual_desktop_management*() const
{
    return d->plasmavirtualdesktopmanagement;
}

bool PlasmaVirtualDesktopManagement::isValid() const
{
    return d->plasmavirtualdesktopmanagement.isValid();
}

void PlasmaVirtualDesktopManagement::setEventQueue(EventQueue* queue)
{
    d->queue = queue;
}

EventQueue* PlasmaVirtualDesktopManagement::eventQueue()
{
    return d->queue;
}

PlasmaVirtualDesktop* PlasmaVirtualDesktopManagement::getVirtualDesktop(QString const& id)
{
    Q_ASSERT(isValid());

    if (id.isEmpty()) {
        return nullptr;
    }

    auto i = d->constFindDesktop(id);
    if (i != d->desktops.constEnd()) {
        return *i;
    }

    auto w = org_kde_plasma_virtual_desktop_management_get_virtual_desktop(
        d->plasmavirtualdesktopmanagement, id.toUtf8());

    if (!w) {
        return nullptr;
    }

    if (d->queue) {
        d->queue->addProxy(w);
    }

    auto desktop = new PlasmaVirtualDesktop(this);
    desktop->setup(w);
    desktop->d->id = id;

    return desktop;
}

void PlasmaVirtualDesktopManagement::requestRemoveVirtualDesktop(QString const& id)
{
    Q_ASSERT(isValid());

    org_kde_plasma_virtual_desktop_management_request_remove_virtual_desktop(
        d->plasmavirtualdesktopmanagement, id.toUtf8());
}

void PlasmaVirtualDesktopManagement::requestCreateVirtualDesktop(QString const& name,
                                                                 quint32 position)
{
    Q_ASSERT(isValid());

    org_kde_plasma_virtual_desktop_management_request_create_virtual_desktop(
        d->plasmavirtualdesktopmanagement, name.toUtf8(), position);
}

QList<PlasmaVirtualDesktop*> PlasmaVirtualDesktopManagement::desktops() const
{
    return d->desktops;
}

quint32 PlasmaVirtualDesktopManagement::rows() const
{
    return d->rows;
}

org_kde_plasma_virtual_desktop_listener const PlasmaVirtualDesktop::Private::s_listener = {
    idCallback,
    nameCallback,
    activatedCallback,
    deactivatedCallback,
    doneCallback,
    removedCallback,
};

void PlasmaVirtualDesktop::Private::idCallback(
    void* data,
    org_kde_plasma_virtual_desktop* org_kde_plasma_virtual_desktop,
    char const* id)
{
    auto p = reinterpret_cast<PlasmaVirtualDesktop::Private*>(data);
    Q_ASSERT(p->plasmavirtualdesktop == org_kde_plasma_virtual_desktop);
    p->id = QString::fromUtf8(id);
}

void PlasmaVirtualDesktop::Private::nameCallback(
    void* data,
    org_kde_plasma_virtual_desktop* org_kde_plasma_virtual_desktop,
    char const* name)
{
    auto p = reinterpret_cast<PlasmaVirtualDesktop::Private*>(data);
    Q_ASSERT(p->plasmavirtualdesktop == org_kde_plasma_virtual_desktop);
    p->name = QString::fromUtf8(name);
}

void PlasmaVirtualDesktop::Private::activatedCallback(
    void* data,
    org_kde_plasma_virtual_desktop* org_kde_plasma_virtual_desktop)
{
    auto p = reinterpret_cast<PlasmaVirtualDesktop::Private*>(data);
    Q_ASSERT(p->plasmavirtualdesktop == org_kde_plasma_virtual_desktop);
    p->active = true;
    Q_EMIT p->q->activated();
}

void PlasmaVirtualDesktop::Private::deactivatedCallback(
    void* data,
    org_kde_plasma_virtual_desktop* org_kde_plasma_virtual_desktop)
{
    auto p = reinterpret_cast<PlasmaVirtualDesktop::Private*>(data);
    Q_ASSERT(p->plasmavirtualdesktop == org_kde_plasma_virtual_desktop);
    p->active = false;
    Q_EMIT p->q->deactivated();
}

void PlasmaVirtualDesktop::Private::doneCallback(
    void* data,
    org_kde_plasma_virtual_desktop* org_kde_plasma_virtual_desktop)
{
    auto p = reinterpret_cast<PlasmaVirtualDesktop::Private*>(data);
    Q_ASSERT(p->plasmavirtualdesktop == org_kde_plasma_virtual_desktop);
    Q_EMIT p->q->done();
}

void PlasmaVirtualDesktop::Private::removedCallback(
    void* data,
    org_kde_plasma_virtual_desktop* org_kde_plasma_virtual_desktop)
{
    auto p = reinterpret_cast<PlasmaVirtualDesktop::Private*>(data);
    Q_ASSERT(p->plasmavirtualdesktop == org_kde_plasma_virtual_desktop);
    Q_EMIT p->q->removed();
}

PlasmaVirtualDesktop::Private::Private(PlasmaVirtualDesktop* q)
    : q(q)
{
}

PlasmaVirtualDesktop::PlasmaVirtualDesktop(QObject* parent)
    : QObject(parent)
    , d(new Private(this))
{
}

void PlasmaVirtualDesktop::Private::setup(org_kde_plasma_virtual_desktop* arg)
{
    Q_ASSERT(arg);
    Q_ASSERT(!plasmavirtualdesktop);
    plasmavirtualdesktop.setup(arg);
    org_kde_plasma_virtual_desktop_add_listener(plasmavirtualdesktop, &s_listener, this);
}

PlasmaVirtualDesktop::~PlasmaVirtualDesktop()
{
    release();
}

void PlasmaVirtualDesktop::setup(org_kde_plasma_virtual_desktop* plasmavirtualdesktop)
{
    d->setup(plasmavirtualdesktop);
}

void PlasmaVirtualDesktop::release()
{
    d->plasmavirtualdesktop.release();
}

PlasmaVirtualDesktop::operator org_kde_plasma_virtual_desktop*()
{
    return d->plasmavirtualdesktop;
}

PlasmaVirtualDesktop::operator org_kde_plasma_virtual_desktop*() const
{
    return d->plasmavirtualdesktop;
}

bool PlasmaVirtualDesktop::isValid() const
{
    return d->plasmavirtualdesktop.isValid();
}

void PlasmaVirtualDesktop::requestActivate()
{
    Q_ASSERT(isValid());
    org_kde_plasma_virtual_desktop_request_activate(d->plasmavirtualdesktop);
}

QString PlasmaVirtualDesktop::id() const
{
    return d->id;
}

QString PlasmaVirtualDesktop::name() const
{
    return d->name;
}

bool PlasmaVirtualDesktop::isActive() const
{
    return d->active;
}

}
}
