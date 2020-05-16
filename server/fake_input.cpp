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
#include "display.h"
#include "fake_input_p.h"

#include <QPointF>
#include <QSizeF>

#include <wayland-server.h>

namespace Wrapland::Server
{

QList<quint32> FakeInput::Private::touchIds = QList<quint32>();

const struct org_kde_kwin_fake_input_interface FakeInput::Private::s_interface = {
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
    keyboardKeyCallback,
};

FakeInput::Private::Private(D_isplay* display, FakeInput* q)
    : FakeInputGlobal(q, display, &org_kde_kwin_fake_input_interface, &s_interface)
{
    create();
}

FakeInput::Private::~Private()
{
    for (auto* device : devices) {
        delete device;
    }
    devices.clear();
}

void FakeInput::Private::bindInit(FakeInputBind* bind)
{
    auto parent = handle();
    auto device = new FakeInputDevice(bind->resource(), parent);
    devices.push_back(device);

    QObject::connect(device, &FakeInputDevice::destroyed, parent, [device, this] {
        devices.erase(std::remove(devices.begin(), devices.end(), device), devices.end());
    });

    Q_EMIT parent->deviceCreated(device);
}

void FakeInput::Private::prepareUnbind(FakeInputBind* bind)
{
    if (auto fakeDevice = device(bind->resource())) {
        auto priv = bind->handle()->d_ptr.get();

        priv->devices.erase(std::remove(priv->devices.begin(), priv->devices.end(), fakeDevice),
                            priv->devices.end());

        delete fakeDevice;
    }
}

FakeInputDevice* FakeInput::Private::device(wl_resource* wlResource)
{
    auto priv = handle(wlResource)->d_ptr.get();

    auto it
        = std::find_if(priv->devices.begin(), priv->devices.end(), [wlResource](auto fakeDevice) {
              return fakeDevice->resource() == wlResource;
          });
    if (it != priv->devices.end()) {
        return *it;
    }
    return nullptr;
}

void FakeInput::Private::authenticateCallback([[maybe_unused]] wl_client* wlClient,
                                              wl_resource* wlResource,
                                              const char* application,
                                              const char* reason)
{
    auto fakeDevice = device(wlResource);
    if (!fakeDevice) {
        return;
    }
    Q_EMIT fakeDevice->authenticationRequested(QString::fromUtf8(application),
                                               QString::fromUtf8(reason));
}

bool check(FakeInputDevice* device)
{
    return device && device->isAuthenticated();
}

void FakeInput::Private::pointerMotionCallback([[maybe_unused]] wl_client* wlClient,
                                               wl_resource* wlResource,
                                               wl_fixed_t delta_x,
                                               wl_fixed_t delta_y)
{
    auto fakeDevice = device(wlResource);
    if (!check(fakeDevice)) {
        return;
    }

    Q_EMIT fakeDevice->pointerMotionRequested(
        QSizeF(wl_fixed_to_double(delta_x), wl_fixed_to_double(delta_y)));
}

void FakeInput::Private::pointerMotionAbsoluteCallback([[maybe_unused]] wl_client* wlClient,
                                                       wl_resource* wlResource,
                                                       wl_fixed_t x,
                                                       wl_fixed_t y)
{
    auto fakeDevice = device(wlResource);
    if (!check(fakeDevice)) {
        return;
    }

    Q_EMIT fakeDevice->pointerMotionAbsoluteRequested(
        QPointF(wl_fixed_to_double(x), wl_fixed_to_double(y)));
}

void FakeInput::Private::axisCallback([[maybe_unused]] wl_client* wlClient,
                                      wl_resource* wlResource,
                                      uint32_t axis,
                                      wl_fixed_t value)
{
    auto fakeDevice = device(wlResource);
    if (!check(fakeDevice)) {
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
    Q_EMIT fakeDevice->pointerAxisRequested(orientation, wl_fixed_to_double(value));
}

void FakeInput::Private::buttonCallback([[maybe_unused]] wl_client* wlClient,
                                        wl_resource* wlResource,
                                        uint32_t button,
                                        uint32_t state)
{
    auto fakeDevice = device(wlResource);
    if (!check(fakeDevice)) {
        return;
    }

    switch (state) {
    case WL_POINTER_BUTTON_STATE_PRESSED:
        Q_EMIT fakeDevice->pointerButtonPressRequested(button);
        break;
    case WL_POINTER_BUTTON_STATE_RELEASED:
        Q_EMIT fakeDevice->pointerButtonReleaseRequested(button);
        break;
    default:
        // nothing
        break;
    }
}

void FakeInput::Private::touchDownCallback([[maybe_unused]] wl_client* wlClient,
                                           wl_resource* wlResource,
                                           quint32 id,
                                           wl_fixed_t x,
                                           wl_fixed_t y)
{
    auto fakeDevice = device(wlResource);
    if (!check(fakeDevice)) {
        return;
    }
    if (touchIds.contains(id)) {
        return;
    }
    touchIds << id;
    Q_EMIT fakeDevice->touchDownRequested(id,
                                          QPointF(wl_fixed_to_double(x), wl_fixed_to_double(y)));
}

void FakeInput::Private::touchMotionCallback([[maybe_unused]] wl_client* wlClient,
                                             wl_resource* wlResource,
                                             quint32 id,
                                             wl_fixed_t x,
                                             wl_fixed_t y)
{
    auto fakeDevice = device(wlResource);
    if (!check(fakeDevice)) {
        return;
    }
    if (!touchIds.contains(id)) {
        return;
    }
    Q_EMIT fakeDevice->touchMotionRequested(id,
                                            QPointF(wl_fixed_to_double(x), wl_fixed_to_double(y)));
}

void FakeInput::Private::touchUpCallback([[maybe_unused]] wl_client* wlClient,
                                         wl_resource* wlResource,
                                         quint32 id)
{
    auto fakeDevice = device(wlResource);
    if (!check(fakeDevice)) {
        return;
    }
    if (!touchIds.contains(id)) {
        return;
    }
    touchIds.removeOne(id);
    Q_EMIT fakeDevice->touchUpRequested(id);
}

void FakeInput::Private::touchCancelCallback([[maybe_unused]] wl_client* wlClient,
                                             wl_resource* wlResource)
{
    auto fakeDevice = device(wlResource);
    if (!check(fakeDevice)) {
        return;
    }
    touchIds.clear();
    Q_EMIT fakeDevice->touchCancelRequested();
}

void FakeInput::Private::touchFrameCallback([[maybe_unused]] wl_client* wlClient,
                                            wl_resource* wlResource)
{
    auto fakeDevice = device(wlResource);
    if (!check(fakeDevice)) {
        return;
    }
    Q_EMIT fakeDevice->touchFrameRequested();
}

void FakeInput::Private::keyboardKeyCallback([[maybe_unused]] wl_client* wlClient,
                                             wl_resource* wlResource,
                                             uint32_t button,
                                             uint32_t state)
{
    auto fakeDevice = device(wlResource);
    if (!check(fakeDevice)) {
        return;
    }
    switch (state) {
    case WL_KEYBOARD_KEY_STATE_PRESSED:
        Q_EMIT fakeDevice->keyboardKeyPressRequested(button);
        break;
    case WL_KEYBOARD_KEY_STATE_RELEASED:
        Q_EMIT fakeDevice->keyboardKeyReleaseRequested(button);
        break;
    default:
        // nothing
        break;
    }
}

FakeInput::FakeInput(D_isplay* display, QObject* parent)
    : QObject(parent)
    , d_ptr(new Private(display, this))
{
}

FakeInput::~FakeInput() = default;

FakeInputDevice::Private::Private(wl_resource* wlResource, FakeInput* interface)
    : resource(wlResource)
    , interface(interface)
{
}

FakeInputDevice::FakeInputDevice(wl_resource* wlResource, FakeInput* parent)
    : QObject(nullptr)
    , d_ptr(new Private(wlResource, parent))
{
}

FakeInputDevice::~FakeInputDevice() = default;

void FakeInputDevice::setAuthentication(bool authenticated)
{
    d_ptr->authenticated = authenticated;
}

wl_resource* FakeInputDevice::resource()
{
    return d_ptr->resource;
}

bool FakeInputDevice::isAuthenticated() const
{
    return d_ptr->authenticated;
}

}
