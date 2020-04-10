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

// legacy
#include "../src/server/display.h"
#include "../src/server/output_interface.h"
#include "wayland/resource.h"

#include <cassert>
#include <functional>
#include <wayland-server.h>

namespace Wrapland
{
namespace Server
{

const quint32 Output::Private::s_version = 3;

Output::Private::Private(Output* q, D_isplay* display)
    : Wayland::Global<Output>(q, display, &wl_output_interface, &s_interface)
    , displayHandle(display)
    , q_ptr(q)
{
}

Output::Private::~Private() = default;

const struct wl_output_interface Output::Private::s_interface = {resourceDestroyCallback};

Output::Mode fromOutputInterfaceMode(OutputInterface::Mode mode)
{
    Output::Mode ret;
    ret.flags = (Output::ModeFlags)(int)mode.flags;
    ret.refreshRate = mode.refreshRate;
    ret.size = mode.size;
    return ret;
}

Output::Output(D_isplay* display, QObject* parent)
    : QObject(parent)
    , d_ptr(new Private(this, display))
{
    legacy = new Server::OutputInterface(display->legacy, this);
    legacy->newOutput = this;

    connect(
        legacy, &OutputInterface::dpmsModeRequested, this, [this](OutputInterface::DpmsMode mode) {
            Q_EMIT dpmsModeRequested((Output::DpmsMode)(int)mode);
        });

    connect(this, &Output::currentModeChanged, this, [this] {
        auto currentModeIt
            = std::find_if(d_ptr->modes.cbegin(), d_ptr->modes.cend(), [](const Mode& mode) {
                  return mode.flags.testFlag(ModeFlag::Current);
              });

        if (currentModeIt == d_ptr->modes.cend()) {
            return;
        }

        d_ptr->sendMode(*currentModeIt);
        d_ptr->sendDone();

        wl_display_flush_clients(*(d_ptr->displayHandle));
    });

    connect(this, &Output::subPixelChanged, this, [this] { d_ptr->updateGeometry(); });
    connect(this, &Output::transformChanged, this, [this] { d_ptr->updateGeometry(); });
    connect(this, &Output::globalPositionChanged, this, [this] { d_ptr->updateGeometry(); });
    connect(this, &Output::modelChanged, this, [this] { d_ptr->updateGeometry(); });
    connect(this, &Output::manufacturerChanged, this, [this] { d_ptr->updateGeometry(); });
    connect(this, &Output::scaleChanged, this, [this] {
        d_ptr->sendScale();
        d_ptr->sendDone();
    });

    d_ptr->create();
}

Output::~Output()
{
    if (legacy) {
        delete legacy;
    }
    d_ptr->displayHandle->removeOutput(this);
    delete d_ptr;
}

QSize Output::pixelSize() const
{
    auto it = std::find_if(d_ptr->modes.cbegin(), d_ptr->modes.cend(), [](const Mode& mode) {
        return mode.flags.testFlag(ModeFlag::Current);
    });
    if (it == d_ptr->modes.cend()) {
        return QSize();
    }
    return (*it).size;
}

int Output::refreshRate() const
{
    auto it = std::find_if(d_ptr->modes.cbegin(), d_ptr->modes.cend(), [](const Mode& mode) {
        return mode.flags.testFlag(ModeFlag::Current);
    });
    if (it == d_ptr->modes.cend()) {
        return 60000;
    }
    return (*it).refreshRate;
}

void Output::addMode(const QSize& size, Output::ModeFlags flags, int refreshRate)
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

void Output::setCurrentMode(const QSize& size, int refreshRate)
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

int32_t Output::Private::toTransform() const
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

int32_t Output::Private::toSubPixel() const
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

void Output::Private::bindInit(Wayland::Client* client, uint32_t version, uint32_t id)
{
    Q_UNUSED(version)
    Q_UNUSED(id)

    sendGeometry(client);
    sendScale(client);

    auto currentModeIt = modes.cend();
    for (auto it = modes.cbegin(); it != modes.cend(); ++it) {
        const Mode& mode = *it;
        if (mode.flags.testFlag(ModeFlag::Current)) {
            // needs to be sent as last mode
            currentModeIt = it;
            continue;
        }
        sendMode(client, mode);
    }

    if (currentModeIt != modes.cend()) {
        sendMode(client, *currentModeIt);
    }

    sendDone(client);
    client->flush();
}

uint32_t Output::Private::version() const
{
    return s_version;
}

int32_t Output::Private::getFlags(const Mode& mode)
{
    int32_t flags = 0;
    if (mode.flags.testFlag(ModeFlag::Current)) {
        flags |= WL_OUTPUT_MODE_CURRENT;
    }
    if (mode.flags.testFlag(ModeFlag::Preferred)) {
        flags |= WL_OUTPUT_MODE_PREFERRED;
    }
    return flags;
}

void Output::Private::sendMode(Wayland::Client* client, const Mode& mode)
{
    send(client, modeFunctor(getFlags(mode), mode));
}

void Output::Private::sendMode(const Mode& mode)
{
    send(modeFunctor(getFlags(mode), mode));
}

void Output::Private::sendGeometry(Wayland::Client* client)
{
    send(client, geometryFunctor());
}

void Output::Private::sendGeometry()
{
    send(geometryFunctor());
}

void Output::Private::sendScale(Wayland::Client* client)
{
    send(client, scaleFunctor(), 2);
}

void Output::Private::sendScale()
{
    send(scaleFunctor(), 2);
}

void Output::Private::sendDone(Wayland::Client* client)
{
    send(client, doneFunctor(), 2);
}

void Output::Private::sendDone()
{
    send(doneFunctor(), 2);
}

void Output::Private::updateGeometry()
{
    sendGeometry();
    sendDone();
}

Sender Output::Private::modeFunctor(int32_t flags, const Mode& mode)
{
    return [this, flags, &mode](wl_resource* resource) {
        wl_output_send_mode(
            resource, flags, mode.size.width(), mode.size.height(), mode.refreshRate);
    };
}

Sender Output::Private::geometryFunctor()
{
    return [this](wl_resource* resource) {
        wl_output_send_geometry(resource,
                                globalPosition.x(),
                                globalPosition.y(),
                                physicalSize.width(),
                                physicalSize.height(),
                                toSubPixel(),
                                manufacturer.c_str(),
                                model.c_str(),
                                toTransform());
    };
}

Sender Output::Private::scaleFunctor()
{
    return [this](wl_resource* resource) { wl_output_send_scale(resource, this->scale); };
}

Sender Output::Private::doneFunctor()
{
    return [](wl_resource* resource) { wl_output_send_done(resource); };
}

#define SETTER(setterName, type, argumentName)                                                     \
    void Output::setterName(type arg)                                                              \
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

QSize Output::physicalSize() const
{
    return d_ptr->physicalSize;
}

QPoint Output::globalPosition() const
{
    return d_ptr->globalPosition;
}

std::string const& Output::manufacturer() const
{

    return d_ptr->manufacturer;
}

std::string const& Output::model() const
{
    return d_ptr->model;
}

int Output::scale() const
{
    return d_ptr->scale;
}

Output::SubPixel Output::subPixel() const
{
    return d_ptr->subPixel;
}

Output::Transform Output::transform() const
{
    return d_ptr->transform;
}

std::vector<Output::Mode> const& Output::modes() const
{
    return d_ptr->modes;
}

void Output::setDpmsMode(Output::DpmsMode mode)
{
    if (d_ptr->dpms.mode == mode) {
        return;
    }
    d_ptr->dpms.mode = mode;
    Q_EMIT dpmsModeChanged();
    Q_EMIT legacy->dpmsModeChanged();
}

void Output::setDpmsSupported(bool supported)
{
    if (d_ptr->dpms.supported == supported) {
        return;
    }
    d_ptr->dpms.supported = supported;
    Q_EMIT dpmsSupportedChanged();
    Q_EMIT legacy->dpmsSupportedChanged();
}

Output::DpmsMode Output::dpmsMode() const
{
    return d_ptr->dpms.mode;
}

bool Output::isDpmsSupported() const
{
    return d_ptr->dpms.supported;
}

Output* Output::get(void* data)
{
    auto resource = reinterpret_cast<Wayland::Resource<Output, Wayland::Global<Output>>*>(data);
    auto outputPriv = static_cast<Output::Private*>(resource->global());
    return outputPriv->q_ptr;
}

QVector<wl_resource*> Output::getResources(Client* client)
{
    return d_ptr->getResources(client);
}

}
}
