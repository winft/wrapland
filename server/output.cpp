/********************************************************************
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
#include "output.h"
#include "output_p.h"

#include "display.h"

#include "wayland/client.h"
#include "wayland/display.h"

#include <cassert>
#include <functional>
#include <wayland-server.h>

namespace Wrapland::Server
{

WlOutput::Private::Private(WlOutput* q, Display* display)
    : WlOutputGlobal(q, display, &wl_output_interface, &s_interface)
    , displayHandle(display)
    , q_ptr(q)
{
}

const struct wl_output_interface WlOutput::Private::s_interface = {resourceDestroyCallback};

WlOutput::WlOutput(Display* display, QObject* parent)
    : QObject(parent)
    , d_ptr(new Private(this, display))
{
    connect(this, &WlOutput::currentModeChanged, this, [this] {
        auto currentModeIt
            = std::find_if(d_ptr->modes.cbegin(), d_ptr->modes.cend(), [](const Mode& mode) {
                  return mode.flags.testFlag(ModeFlag::Current);
              });

        if (currentModeIt == d_ptr->modes.cend()) {
            return;
        }

        d_ptr->sendMode(*currentModeIt);
        d_ptr->sendDone();

        wl_display_flush_clients(d_ptr->displayHandle->native());
    });

    connect(this, &WlOutput::subPixelChanged, this, [this] { d_ptr->updateGeometry(); });
    connect(this, &WlOutput::transformChanged, this, [this] { d_ptr->updateGeometry(); });
    connect(this, &WlOutput::globalPositionChanged, this, [this] { d_ptr->updateGeometry(); });
    connect(this, &WlOutput::modelChanged, this, [this] { d_ptr->updateGeometry(); });
    connect(this, &WlOutput::manufacturerChanged, this, [this] { d_ptr->updateGeometry(); });
    connect(this, &WlOutput::scaleChanged, this, [this] {
        d_ptr->sendScale();
        d_ptr->sendDone();
    });

    d_ptr->create();
}

WlOutput::~WlOutput()
{
    Q_EMIT removed();

    if (d_ptr->displayHandle) {
        d_ptr->displayHandle->removeOutput(this);
    }
}

QSize WlOutput::pixelSize() const
{
    auto it = std::find_if(d_ptr->modes.cbegin(), d_ptr->modes.cend(), [](const Mode& mode) {
        return mode.flags.testFlag(ModeFlag::Current);
    });
    if (it == d_ptr->modes.cend()) {
        return QSize();
    }
    return (*it).size;
}

int WlOutput::refreshRate() const
{
    auto it = std::find_if(d_ptr->modes.cbegin(), d_ptr->modes.cend(), [](const Mode& mode) {
        return mode.flags.testFlag(ModeFlag::Current);
    });
    if (it == d_ptr->modes.cend()) {
        return Mode::defaultRefreshRate;
    }
    return (*it).refreshRate;
}

void WlOutput::addMode(const QSize& size, WlOutput::ModeFlags flags, int refreshRate)
{
    auto currentModeIt
        = std::find_if(d_ptr->modes.begin(), d_ptr->modes.end(), [](const Mode& mode) {
              return mode.flags.testFlag(ModeFlag::Current);
          });
    if (currentModeIt == d_ptr->modes.end() && !flags.testFlag(ModeFlag::Current)) {
        // no mode with current flag - enforce
        flags |= ModeFlag::Current;
    }
    if (currentModeIt != d_ptr->modes.end() && flags.testFlag(ModeFlag::Current)) {
        // another mode has the current flag - remove
        (*currentModeIt).flags &= ~uint(ModeFlag::Current);
    }

    if (flags.testFlag(ModeFlag::Preferred)) {
        // remove from existing Preferred mode
        auto preferredIt
            = std::find_if(d_ptr->modes.begin(), d_ptr->modes.end(), [](const Mode& mode) {
                  return mode.flags.testFlag(ModeFlag::Preferred);
              });
        if (preferredIt != d_ptr->modes.end()) {
            (*preferredIt).flags &= ~uint(ModeFlag::Preferred);
        }
    }

    auto existingModeIt = std::find_if(
        d_ptr->modes.begin(), d_ptr->modes.end(), [size, refreshRate](const Mode& mode) {
            return mode.size == size && mode.refreshRate == refreshRate;
        });
    auto emitChanges = [this, flags, size, refreshRate] {
        emit modesChanged();
        if (flags.testFlag(ModeFlag::Current)) {
            emit refreshRateChanged(refreshRate);
            emit pixelSizeChanged(size);
            emit currentModeChanged();
        }
    };
    if (existingModeIt != d_ptr->modes.end()) {
        if ((*existingModeIt).flags == flags) {
            // nothing to do
            return;
        }
        (*existingModeIt).flags = flags;
        emitChanges();
        return;
    }
    Mode mode;
    mode.size = size;
    mode.refreshRate = refreshRate;
    mode.flags = flags;
    d_ptr->modes.push_back(mode);
    emitChanges();
}

void WlOutput::setCurrentMode(const QSize& size, int refreshRate)
{
    auto currentModeIt
        = std::find_if(d_ptr->modes.begin(), d_ptr->modes.end(), [](const Mode& mode) {
              return mode.flags.testFlag(ModeFlag::Current);
          });
    if (currentModeIt != d_ptr->modes.end()) {
        // another mode has the current flag - remove
        (*currentModeIt).flags &= ~uint(ModeFlag::Current);
    }

    auto existingModeIt = std::find_if(
        d_ptr->modes.begin(), d_ptr->modes.end(), [size, refreshRate](const Mode& mode) {
            return mode.size == size && mode.refreshRate == refreshRate;
        });

    Q_ASSERT(existingModeIt != d_ptr->modes.end());
    (*existingModeIt).flags |= ModeFlag::Current;
    emit modesChanged();
    emit refreshRateChanged((*existingModeIt).refreshRate);
    emit pixelSizeChanged((*existingModeIt).size);
    emit currentModeChanged();
}

int32_t WlOutput::Private::toTransform() const
{
    switch (transform) {
    case Transform::Normal:
        return WL_OUTPUT_TRANSFORM_NORMAL;
    case Transform::Rotated90:
        return WL_OUTPUT_TRANSFORM_90;
    case Transform::Rotated180:
        return WL_OUTPUT_TRANSFORM_180;
    case Transform::Rotated270:
        return WL_OUTPUT_TRANSFORM_270;
    case Transform::Flipped:
        return WL_OUTPUT_TRANSFORM_FLIPPED;
    case Transform::Flipped90:
        return WL_OUTPUT_TRANSFORM_FLIPPED_90;
    case Transform::Flipped180:
        return WL_OUTPUT_TRANSFORM_FLIPPED_180;
    case Transform::Flipped270:
        return WL_OUTPUT_TRANSFORM_FLIPPED_270;
    }
    abort();
}

int32_t WlOutput::Private::toSubPixel() const
{
    switch (subPixel) {
    case SubPixel::Unknown:
        return WL_OUTPUT_SUBPIXEL_UNKNOWN;
    case SubPixel::None:
        return WL_OUTPUT_SUBPIXEL_NONE;
    case SubPixel::HorizontalRGB:
        return WL_OUTPUT_SUBPIXEL_HORIZONTAL_RGB;
    case SubPixel::HorizontalBGR:
        return WL_OUTPUT_SUBPIXEL_HORIZONTAL_BGR;
    case SubPixel::VerticalRGB:
        return WL_OUTPUT_SUBPIXEL_VERTICAL_RGB;
    case SubPixel::VerticalBGR:
        return WL_OUTPUT_SUBPIXEL_VERTICAL_BGR;
    }
    abort();
}

void WlOutput::Private::bindInit(Wayland::Resource<WlOutput, WlOutputGlobal>* bind)
{
    send<wl_output_send_geometry>(bind, geometryArgs());
    send<wl_output_send_scale, 2>(bind, scale);

    auto currentModeIt = modes.cend();
    for (auto it = modes.cbegin(); it != modes.cend(); ++it) {
        const Mode& mode = *it;
        if (mode.flags.testFlag(ModeFlag::Current)) {
            // needs to be sent as last mode
            currentModeIt = it;
            continue;
        }
        sendMode(bind, mode);
    }

    if (currentModeIt != modes.cend()) {
        sendMode(bind, *currentModeIt);
    }

    send<wl_output_send_done, 2>(bind);
    bind->client()->flush();
}

int32_t getFlags(const WlOutput::Mode& mode)
{
    int32_t flags = 0;
    if (mode.flags.testFlag(WlOutput::ModeFlag::Current)) {
        flags |= WL_OUTPUT_MODE_CURRENT;
    }
    if (mode.flags.testFlag(WlOutput::ModeFlag::Preferred)) {
        flags |= WL_OUTPUT_MODE_PREFERRED;
    }
    return flags;
}

void WlOutput::Private::sendMode(Wayland::Resource<WlOutput, WlOutputGlobal>* bind,
                                 const Mode& mode)
{
    send<wl_output_send_mode>(
        bind, getFlags(mode), mode.size.width(), mode.size.height(), mode.refreshRate);
}

void WlOutput::Private::sendMode(const Mode& mode)
{
    send<wl_output_send_mode>(
        getFlags(mode), mode.size.width(), mode.size.height(), mode.refreshRate);
}

void WlOutput::Private::sendGeometry()
{
    send<wl_output_send_geometry>(geometryArgs());
}

void WlOutput::Private::sendScale()
{
    send<wl_output_send_scale, 2>(scale);
}

void WlOutput::Private::sendDone()
{
    send<wl_output_send_done>();
}

void WlOutput::Private::updateGeometry()
{
    sendGeometry();
    sendDone();
}

std::tuple<int32_t, int32_t, int32_t, int32_t, int32_t, const char*, const char*, int32_t>
WlOutput::Private::geometryArgs() const
{
    return std::make_tuple(globalPosition.x(),
                           globalPosition.y(),
                           physicalSize.width(),
                           physicalSize.height(),
                           toSubPixel(),
                           manufacturer.c_str(),
                           model.c_str(),
                           toTransform());
}

#define SETTER(setterName, type, argumentName)                                                     \
    void WlOutput::setterName(type arg)                                                            \
    {                                                                                              \
        if (d_ptr->argumentName == arg) {                                                          \
            return;                                                                                \
        }                                                                                          \
        d_ptr->argumentName = arg;                                                                 \
        emit argumentName##Changed(d_ptr->argumentName);                                           \
    }

SETTER(setPhysicalSize, const QSize&, physicalSize)
SETTER(setGlobalPosition, const QPoint&, globalPosition)
SETTER(setManufacturer, std::string const&, manufacturer)
SETTER(setModel, std::string const&, model)
SETTER(setScale, int, scale)
SETTER(setSubPixel, SubPixel, subPixel)
SETTER(setTransform, Transform, transform)

#undef SETTER

QSize WlOutput::physicalSize() const
{
    return d_ptr->physicalSize;
}

QPoint WlOutput::globalPosition() const
{
    return d_ptr->globalPosition;
}

std::string const& WlOutput::manufacturer() const
{

    return d_ptr->manufacturer;
}

std::string const& WlOutput::model() const
{
    return d_ptr->model;
}

int WlOutput::scale() const
{
    return d_ptr->scale;
}

WlOutput::SubPixel WlOutput::subPixel() const
{
    return d_ptr->subPixel;
}

WlOutput::Transform WlOutput::transform() const
{
    return d_ptr->transform;
}

std::vector<WlOutput::Mode> const& WlOutput::modes() const
{
    return d_ptr->modes;
}

void WlOutput::setDpmsMode(WlOutput::DpmsMode mode)
{
    if (d_ptr->dpms.mode == mode) {
        return;
    }
    d_ptr->dpms.mode = mode;
    Q_EMIT dpmsModeChanged();
}

void WlOutput::setDpmsSupported(bool supported)
{
    if (d_ptr->dpms.supported == supported) {
        return;
    }
    d_ptr->dpms.supported = supported;
    Q_EMIT dpmsSupportedChanged();
}

WlOutput::DpmsMode WlOutput::dpmsMode() const
{
    return d_ptr->dpms.mode;
}

bool WlOutput::isDpmsSupported() const
{
    return d_ptr->dpms.supported;
}

}
