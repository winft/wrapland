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
#include "fakeinput_interface.h"
#include "display.h"
#include "global_p.h"

#include <QSizeF>
#include <QPointF>

#include <wayland-server.h>
#include <wayland-fake-input-server-protocol.h>

namespace Wrapland
{
namespace Server
{

class FakeInputInterface::Private : public Global::Private
{
public:
    Private(FakeInputInterface *q, Display *d);
    ~Private() override;

    QList<FakeInputDevice*> devices;

private:
    void bind(wl_client *client, uint32_t version, uint32_t id) override;
    static void authenticateCallback(wl_client *client, wl_resource *resource, const char *application, const char *reason);
    static void pointerMotionCallback(wl_client *client, wl_resource *resource, wl_fixed_t delta_x, wl_fixed_t delta_y);
    static void pointerMotionAbsoluteCallback(wl_client *client, wl_resource *resource, wl_fixed_t x, wl_fixed_t y);
    static void buttonCallback(wl_client *client, wl_resource *resource, uint32_t button, uint32_t state);
    static void axisCallback(wl_client *client, wl_resource *resource, uint32_t axis, wl_fixed_t value);
    static void touchDownCallback(wl_client *client, wl_resource *resource, quint32 id, wl_fixed_t x, wl_fixed_t y);
    static void touchMotionCallback(wl_client *client, wl_resource *resource, quint32 id, wl_fixed_t x, wl_fixed_t y);
    static void touchUpCallback(wl_client *client, wl_resource *resource, quint32 id);
    static void touchCancelCallback(wl_client *client, wl_resource *resource);
    static void touchFrameCallback(wl_client *client, wl_resource *resource);
    static void keyboardKeyCallback(wl_client *client, wl_resource *resource, uint32_t button, uint32_t state);

    static void unbind(wl_resource *resource);
    static Private *cast(wl_resource *r) {
        return reinterpret_cast<Private*>(wl_resource_get_user_data(r));
    }
    static FakeInputDevice *device(wl_resource *r);

    FakeInputInterface *q;
    static const struct org_kde_kwin_fake_input_interface s_interface;
    static const quint32 s_version;
    static QList<quint32> touchIds;
};

class FakeInputDevice::Private
{
public:
    Private(wl_resource *resource, FakeInputInterface *interface, FakeInputDevice *q);
    wl_resource *resource;
    FakeInputInterface *interface;
    bool authenticated = false;

private:
    FakeInputDevice *q;
};

const quint32 FakeInputInterface::Private::s_version = 4;
QList<quint32> FakeInputInterface::Private::touchIds = QList<quint32>();

#ifndef DOXYGEN_SHOULD_SKIP_THIS
const struct org_kde_kwin_fake_input_interface FakeInputInterface::Private::s_interface = {
    authenticateCallback,
    pointerMotionCallback,
    buttonCallback,
    axisCallback,
    touchDownCallback,
    touchMotionCallback,
    touchUpCallback,
    touchCancelCallback,
    touchFrameCallback,
    pointerMotionAbsoluteCallback,
    keyboardKeyCallback
};
#endif

// TODO: This is currently a hack such that we don't segfault when the interface has been destroyed
//       by the compositor before shutdown and some clients still need to unbind from it.
static bool isDestroyed = false;

FakeInputInterface::Private::Private(FakeInputInterface *q, Display *d)
    : Global::Private(d, &org_kde_kwin_fake_input_interface, s_version)
    , q(q)
{
    isDestroyed = false;
}

FakeInputInterface::Private::~Private()
{
    for (auto *device : devices) {
        wl_resource_set_destructor(device->resource(), nullptr);
        wl_resource_set_user_data(device->resource(), nullptr);
        delete device;
    }
    devices.clear();
    isDestroyed = true;
}

void FakeInputInterface::Private::bind(wl_client *client, uint32_t version, uint32_t id)
{
    auto c = display->getConnection(client);
    wl_resource *resource = c->createResource(&org_kde_kwin_fake_input_interface, qMin(version, s_version), id);
    if (!resource) {
        wl_client_post_no_memory(client);
        return;
    }
    wl_resource_set_implementation(resource, &s_interface, this, unbind);
    FakeInputDevice *device = new FakeInputDevice(resource, q);
    devices << device;
    QObject::connect(device, &FakeInputDevice::destroyed, q, [device, this] { devices.removeAll(device); });
    emit q->deviceCreated(device);
}

void FakeInputInterface::Private::unbind(wl_resource *resource)
{
    if (isDestroyed) {
        return;
    }
    if (FakeInputDevice *d = device(resource)) {
        cast(resource)->devices.removeAll(d);
        d->deleteLater();
    }
}

FakeInputDevice *FakeInputInterface::Private::device(wl_resource *r)
{
    Private *p = cast(r);
    auto it = std::find_if(p->devices.constBegin(), p->devices.constEnd(), [r] (FakeInputDevice *device) { return device->resource() == r; } );
    if (it != p->devices.constEnd()) {
        return *it;
    }
    return nullptr;
}

void FakeInputInterface::Private::authenticateCallback(wl_client *client, wl_resource *resource, const char *application, const char *reason)
{
    Q_UNUSED(client)
    FakeInputDevice *d = device(resource);
    if (!d) {
        return;
    }
    emit d->authenticationRequested(QString::fromUtf8(application), QString::fromUtf8(reason));
}

void FakeInputInterface::Private::pointerMotionCallback(wl_client *client, wl_resource *resource, wl_fixed_t delta_x, wl_fixed_t delta_y)
{
    Q_UNUSED(client)
    FakeInputDevice *d = device(resource);
    if (!d || !d->isAuthenticated()) {
        return;
    }
    emit d->pointerMotionRequested(QSizeF(wl_fixed_to_double(delta_x), wl_fixed_to_double(delta_y)));
}

void FakeInputInterface::Private::pointerMotionAbsoluteCallback(wl_client *client, wl_resource *resource, wl_fixed_t x, wl_fixed_t y)
{
    Q_UNUSED(client)
    FakeInputDevice *d = device(resource);
    if (!d || !d->isAuthenticated()) {
        return;
    }
    emit d->pointerMotionAbsoluteRequested(QPointF(wl_fixed_to_double(x), wl_fixed_to_double(y)));
}

void FakeInputInterface::Private::axisCallback(wl_client *client, wl_resource *resource, uint32_t axis, wl_fixed_t value)
{
    Q_UNUSED(client)
    FakeInputDevice *d = device(resource);
    if (!d || !d->isAuthenticated()) {
        return;
    }
    Qt::Orientation orientation;
    switch (axis) {
    case WL_POINTER_AXIS_HORIZONTAL_SCROLL:
        orientation = Qt::Horizontal;
        break;
    case WL_POINTER_AXIS_VERTICAL_SCROLL:
        orientation = Qt::Vertical;
        break;
    default:
        // invalid
        return;
    }
    emit d->pointerAxisRequested(orientation, wl_fixed_to_double(value));
}

void FakeInputInterface::Private::buttonCallback(wl_client *client, wl_resource *resource, uint32_t button, uint32_t state)
{
    Q_UNUSED(client)
    FakeInputDevice *d = device(resource);
    if (!d || !d->isAuthenticated()) {
        return;
    }
    switch (state) {
    case WL_POINTER_BUTTON_STATE_PRESSED:
        emit d->pointerButtonPressRequested(button);
        break;
    case WL_POINTER_BUTTON_STATE_RELEASED:
        emit d->pointerButtonReleaseRequested(button);
        break;
    default:
        // nothing
        break;
    }
}

void FakeInputInterface::Private::touchDownCallback(wl_client *client, wl_resource *resource, quint32 id, wl_fixed_t x, wl_fixed_t y)
{
    Q_UNUSED(client)
    FakeInputDevice *d = device(resource);
    if (!d || !d->isAuthenticated()) {
        return;
    }
    if (touchIds.contains(id)) {
        return;
    }
    touchIds << id;
    emit d->touchDownRequested(id, QPointF(wl_fixed_to_double(x), wl_fixed_to_double(y)));
}

void FakeInputInterface::Private::touchMotionCallback(wl_client *client, wl_resource *resource, quint32 id, wl_fixed_t x, wl_fixed_t y)
{
    Q_UNUSED(client)
    FakeInputDevice *d = device(resource);
    if (!d || !d->isAuthenticated()) {
        return;
    }
    if (!touchIds.contains(id)) {
        return;
    }
    emit d->touchMotionRequested(id, QPointF(wl_fixed_to_double(x), wl_fixed_to_double(y)));
}

void FakeInputInterface::Private::touchUpCallback(wl_client *client, wl_resource *resource, quint32 id)
{
    Q_UNUSED(client)
    FakeInputDevice *d = device(resource);
    if (!d || !d->isAuthenticated()) {
        return;
    }
    if (!touchIds.contains(id)) {
        return;
    }
    touchIds.removeOne(id);
    emit d->touchUpRequested(id);
}

void FakeInputInterface::Private::touchCancelCallback(wl_client *client, wl_resource *resource)
{
    Q_UNUSED(client)
    FakeInputDevice *d = device(resource);
    if (!d || !d->isAuthenticated()) {
        return;
    }
    touchIds.clear();
    emit d->touchCancelRequested();
}

void FakeInputInterface::Private::touchFrameCallback(wl_client *client, wl_resource *resource)
{
    Q_UNUSED(client)
    FakeInputDevice *d = device(resource);
    if (!d || !d->isAuthenticated()) {
        return;
    }
    emit d->touchFrameRequested();
}

void FakeInputInterface::Private::keyboardKeyCallback(wl_client *client, wl_resource *resource, uint32_t button, uint32_t state)
{
    Q_UNUSED(client)
    FakeInputDevice *d = device(resource);
    if (!d || !d->isAuthenticated()) {
        return;
    }
    switch (state) {
    case WL_KEYBOARD_KEY_STATE_PRESSED:
        emit d->keyboardKeyPressRequested(button);
        break;
    case WL_KEYBOARD_KEY_STATE_RELEASED:
        emit d->keyboardKeyReleaseRequested(button);
        break;
    default:
        // nothing
        break;
    }
}

FakeInputInterface::FakeInputInterface(Display *display, QObject *parent)
    : Global(new Private(this, display), parent)
{
}

FakeInputInterface::~FakeInputInterface() = default;

FakeInputDevice::Private::Private(wl_resource *resource, FakeInputInterface *interface, FakeInputDevice *q)
    : resource(resource)
    , interface(interface)
    , q(q)
{
}

FakeInputDevice::FakeInputDevice(wl_resource *resource, FakeInputInterface *parent)
    : QObject(parent)
    , d(new Private(resource, parent, this))
{
}

FakeInputDevice::~FakeInputDevice() = default;

void FakeInputDevice::setAuthentication(bool authenticated)
{
    d->authenticated = authenticated;
}

wl_resource *FakeInputDevice::resource()
{
    return d->resource;
}

bool FakeInputDevice::isAuthenticated() const
{
    return d->authenticated;
}

}
}
