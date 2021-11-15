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

#include <cassert>
#include <wayland-server.h>

namespace Wrapland::Server
{

const struct org_kde_kwin_fake_input_interface FakeInput::Private::s_interface = {
    cb<authenticateCallback>,
    cb<pointerMotionCallback>,
    cb<buttonCallback>,
    cb<axisCallback>,
    cb<touchDownCallback>,
    cb<touchMotionCallback>,
    cb<touchUpCallback>,
    cb<touchCancelCallback>,
    cb<touchFrameCallback>,
    cb<pointerMotionAbsoluteCallback>,
    cb<keyboardKeyCallback>,
};

FakeInput::Private::Private(Display* display, FakeInput* q)
    : FakeInputGlobal(q, display, &org_kde_kwin_fake_input_interface, &s_interface)
{
    create();
}

FakeInput::Private::~Private()
{
    for (auto device : devices) {
        delete device;
    }
    devices.clear();
}

void FakeInput::Private::bindInit(FakeInputBind* bind)
{
    auto devicePriv = std::make_unique<FakeInputDevice::Private>(bind);
    auto device = new FakeInputDevice(std::move(devicePriv));
    devices.push_back(device);
    Q_EMIT handle()->deviceCreated(device);
}

void FakeInput::Private::prepareUnbind(FakeInputBind* bind)
{
    auto handle = bind->global()->handle();
    auto priv = handle->d_ptr.get();
    auto fakeDevice = device(bind);

    priv->devices.erase(std::remove(priv->devices.begin(), priv->devices.end(), fakeDevice),
                        priv->devices.end());

    Q_EMIT handle->device_destroyed(fakeDevice);

    delete fakeDevice;
}

FakeInputDevice* FakeInput::Private::device(wl_resource* wlResource)
{
    auto priv = handle(wlResource)->d_ptr.get();
    auto bind = priv->getBind(wlResource);
    return priv->device(bind);
}

FakeInputDevice* FakeInput::Private::device(FakeInputBind* bind) const
{
    auto it = std::find_if(devices.begin(), devices.end(), [bind](auto fakeDevice) {
        return fakeDevice->d_ptr->bind == bind;
    });
    if (it != devices.end()) {
        return *it;
    }
    return nullptr;
}

void FakeInput::Private::authenticateCallback(FakeInputBind* bind,
                                              const char* application,
                                              const char* reason)
{
    auto fakeDevice = device(bind->resource());
    Q_EMIT fakeDevice->authenticationRequested(QString::fromUtf8(application),
                                               QString::fromUtf8(reason));
}

bool check(FakeInputDevice* device)
{
    assert(device);
    return device->isAuthenticated();
}

void FakeInput::Private::pointerMotionCallback(FakeInputBind* bind,
                                               wl_fixed_t delta_x,
                                               wl_fixed_t delta_y)
{
    auto fakeDevice = device(bind->resource());
    if (!check(fakeDevice)) {
        return;
    }

    Q_EMIT fakeDevice->pointerMotionRequested(
        QSizeF(wl_fixed_to_double(delta_x), wl_fixed_to_double(delta_y)));
}

void FakeInput::Private::pointerMotionAbsoluteCallback(FakeInputBind* bind,
                                                       wl_fixed_t x,
                                                       wl_fixed_t y)
{
    auto fakeDevice = device(bind->resource());
    if (!check(fakeDevice)) {
        return;
    }

    Q_EMIT fakeDevice->pointerMotionAbsoluteRequested(
        QPointF(wl_fixed_to_double(x), wl_fixed_to_double(y)));
}

void FakeInput::Private::axisCallback(FakeInputBind* bind, uint32_t axis, wl_fixed_t value)
{
    auto fakeDevice = device(bind->resource());
    if (!check(fakeDevice)) {
        return;
    }

    Qt::Orientation orientation = Qt::Horizontal;
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

void FakeInput::Private::buttonCallback(FakeInputBind* bind, uint32_t button, uint32_t state)
{
    auto fakeDevice = device(bind->resource());
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

void FakeInput::Private::touchDownCallback(FakeInputBind* bind,
                                           quint32 id,
                                           wl_fixed_t x,
                                           wl_fixed_t y)
{
    auto fakeDevice = device(bind->resource());
    if (!check(fakeDevice)) {
        return;
    }
    auto priv = bind->global()->handle()->d_ptr.get();
    if (priv->touchIds.contains(id)) {
        return;
    }
    priv->touchIds << id;
    Q_EMIT fakeDevice->touchDownRequested(id,
                                          QPointF(wl_fixed_to_double(x), wl_fixed_to_double(y)));
}

void FakeInput::Private::touchMotionCallback(FakeInputBind* bind,
                                             quint32 id,
                                             wl_fixed_t x,
                                             wl_fixed_t y)
{
    auto fakeDevice = device(bind->resource());
    if (!check(fakeDevice)) {
        return;
    }
    auto priv = bind->global()->handle()->d_ptr.get();
    if (!priv->touchIds.contains(id)) {
        return;
    }
    Q_EMIT fakeDevice->touchMotionRequested(id,
                                            QPointF(wl_fixed_to_double(x), wl_fixed_to_double(y)));
}

void FakeInput::Private::touchUpCallback(FakeInputBind* bind, quint32 id)
{
    auto fakeDevice = device(bind->resource());
    if (!check(fakeDevice)) {
        return;
    }
    auto priv = bind->global()->handle()->d_ptr.get();
    if (!priv->touchIds.contains(id)) {
        return;
    }
    priv->touchIds.removeOne(id);
    Q_EMIT fakeDevice->touchUpRequested(id);
}

void FakeInput::Private::touchCancelCallback(FakeInputBind* bind)
{
    auto fakeDevice = device(bind->resource());
    if (!check(fakeDevice)) {
        return;
    }
    auto priv = bind->global()->handle()->d_ptr.get();
    priv->touchIds.clear();
    Q_EMIT fakeDevice->touchCancelRequested();
}

void FakeInput::Private::touchFrameCallback(FakeInputBind* bind)
{
    auto fakeDevice = device(bind->resource());
    if (!check(fakeDevice)) {
        return;
    }
    Q_EMIT fakeDevice->touchFrameRequested();
}

void FakeInput::Private::keyboardKeyCallback(FakeInputBind* bind, uint32_t button, uint32_t state)
{
    auto fakeDevice = device(bind->resource());
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

FakeInput::FakeInput(Display* display)
    : d_ptr(new Private(display, this))
{
}

FakeInput::~FakeInput() = default;

FakeInputDevice::Private::Private(FakeInputBind* bind)
    : bind(bind)
{
}

FakeInputDevice::FakeInputDevice(std::unique_ptr<FakeInputDevice::Private> p)
    : QObject(nullptr)
    , d_ptr(std::move(p))
{
}

FakeInputDevice::~FakeInputDevice() = default;

void FakeInputDevice::setAuthentication(bool authenticated)
{
    d_ptr->authenticated = authenticated;
}

bool FakeInputDevice::isAuthenticated() const
{
    return d_ptr->authenticated;
}

}
