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
#include "seat.h"
#include "seat_p.h"

#include "client.h"
#include "display.h"
#include "keyboard.h"
#include "keyboard_p.h"
#include "pointer.h"
#include "pointer_p.h"

// legacy
#include "datadevice_interface.h"
#include "datasource_interface.h"
#include "seat_interface.h"
#include "surface_interface.h"
#include "textinput_interface_p.h"

#include <config-wrapland.h>

#ifndef WL_SEAT_NAME_SINCE_VERSION
#define WL_SEAT_NAME_SINCE_VERSION 2
#endif

#if HAVE_LINUX_INPUT_H
#include <linux/input.h>
#endif

#include <functional>

namespace Wrapland
{

namespace Server
{

const quint32 Seat::Private::s_version = 5;
const qint32 Seat::Private::s_pointerVersion = 5;
const qint32 Seat::Private::s_touchVersion = 5;
const qint32 Seat::Private::s_keyboardVersion = 5;

Seat::Private::Private(Seat* q, D_isplay* display)
    : Wayland::Global<Seat>(q, display, &wl_seat_interface, &s_interface)
    , q_ptr(q)
{
}

const struct wl_seat_interface Seat::Private::s_interface = {
    getPointerCallback,
    getKeyboardCallback,
    getTouchCallback,
    resourceDestroyCallback,
};

Seat::Seat(D_isplay* display, QObject* parent)
    : QObject(parent)
    , d_ptr(new Private(this, display))
{
    legacy = new Server::SeatInterface(display->legacy, this);
    legacy->newSeat = this;

    connect(this, &Seat::nameChanged, this, [this] { d_ptr->sendName(); });

    auto sendCapabilities = [this] { d_ptr->sendCapabilities(); };
    connect(this, &Seat::hasPointerChanged, this, sendCapabilities);
    connect(this, &Seat::hasKeyboardChanged, this, sendCapabilities);
    connect(this, &Seat::hasTouchChanged, this, sendCapabilities);

    d_ptr->create();
}

Seat::~Seat()
{
    // Need to unset all focused surfaces.
    setFocusedKeyboardSurface(nullptr);
    setFocusedTextInputSurface(nullptr);
    setFocusedTouchSurface(nullptr);
    setFocusedPointerSurface(nullptr);

    if (legacy) {
        delete legacy;
    }

    delete d_ptr;
}

void Seat::Private::bindInit(Wayland::Client* client,
                             [[maybe_unused]] uint32_t version,
                             [[maybe_unused]] uint32_t id)
{
    sendCapabilities(client);
    sendName(client);
}

uint32_t Seat::Private::version() const
{
    return s_version;
}

void Seat::Private::updatePointerButtonSerial(quint32 button, quint32 serial)
{
    auto it = globalPointer.buttonSerials.find(button);
    if (it == globalPointer.buttonSerials.end()) {
        globalPointer.buttonSerials.insert(button, serial);
        return;
    }
    it.value() = serial;
}

void Seat::Private::updatePointerButtonState(quint32 button, SeatPointer::State state)
{
    auto it = globalPointer.buttonStates.find(button);
    if (it == globalPointer.buttonStates.end()) {
        globalPointer.buttonStates.insert(button, state);
        return;
    }
    it.value() = state;
}

bool Seat::Private::updateKey(quint32 key, SeatKeyboard::State state)
{
    auto it = keys.states.find(key);
    if (it == keys.states.end()) {
        keys.states.insert(key, state);
        return true;
    }
    if (it.value() == state) {
        return false;
    }
    it.value() = state;
    return true;
}

void Seat::Private::sendName(Wayland::Client* client)
{
    send<wl_seat_send_name, WL_SEAT_NAME_SINCE_VERSION>(client, name.c_str());
}
void Seat::Private::sendName()
{
    send<wl_seat_send_name, WL_SEAT_NAME_SINCE_VERSION>(name.c_str());
}

uint32_t Seat::Private::getCapabilities() const
{
    uint32_t capabilities = 0;
    if (pointer) {
        capabilities |= WL_SEAT_CAPABILITY_POINTER;
    }
    if (keyboard) {
        capabilities |= WL_SEAT_CAPABILITY_KEYBOARD;
    }
    if (touch) {
        capabilities |= WL_SEAT_CAPABILITY_TOUCH;
    }
    return capabilities;
}

void Seat::Private::sendCapabilities(Wayland::Client* client)
{
    send<wl_seat_send_capabilities>(client, getCapabilities());
}

void Seat::Private::sendCapabilities()
{
    send<wl_seat_send_capabilities>(getCapabilities());
}

Seat::Private* Seat::Private::cast(wl_resource* r)
{
    return r ? reinterpret_cast<Seat::Private*>(wl_resource_get_user_data(r)) : nullptr;
}

namespace
{

template<typename T>
static T* interfaceForSurface(SurfaceInterface* surface, const QVector<T*>& interfaces)
{
    if (!surface) {
        return nullptr;
    }

    for (auto it = interfaces.constBegin(); it != interfaces.constEnd(); ++it) {
        if constexpr (std::is_same_v<T, Pointer>) {
            if ((*it)->client()->legacy == surface->client()) {
                return (*it);
            }
        } else if constexpr (std::is_same_v<T, Keyboard>) {
            if ((*it)->client()->legacy == surface->client()) {
                return (*it);
            }
        } else {
            // TODO: remove when all in new model
            if ((*it)->client() == surface->client()) {
                return (*it);
            }
        }
    }
    return nullptr;
}

template<typename T>
static QVector<T*> interfacesForSurface(SurfaceInterface* surface, const QVector<T*>& interfaces)
{
    QVector<T*> ret;
    if (!surface) {
        return ret;
    }

    for (auto it = interfaces.constBegin(); it != interfaces.constEnd(); ++it) {
        if constexpr (std::is_same_v<T, Pointer>) {
            if ((*it)->client()->legacy == surface->client()) {
                ret << *it;
            }
        } else if constexpr (std::is_same_v<T, Keyboard>) {
            if ((*it)->client()->legacy == surface->client()) {
                ret << *it;
            }
        } else {
            // TODO: remove when all in new model
            if ((*it)->client() == surface->client()) {
                ret << *it;
            }
        }
    }
    return ret;
}

template<typename T>
static bool forEachInterface(SurfaceInterface* surface,
                             const QVector<T*>& interfaces,
                             std::function<void(T*)> method)
{
    if (!surface) {
        return false;
    }
    bool calledAtLeastOne = false;
    for (auto it = interfaces.constBegin(); it != interfaces.constEnd(); ++it) {
        if ((*it)->client()->legacy == surface->client()) {
            method(*it);
            calledAtLeastOne = true;
        }
    }
    return calledAtLeastOne;
}

}

QVector<Pointer*> Seat::Private::pointersForSurface(SurfaceInterface* surface) const
{
    return interfacesForSurface(surface, pointers);
}

QVector<Keyboard*> Seat::Private::keyboardsForSurface(SurfaceInterface* surface) const
{
    return interfacesForSurface(surface, keyboards);
}

QVector<TouchInterface*> Seat::Private::touchsForSurface(SurfaceInterface* surface) const
{
    return interfacesForSurface(surface, touchs);
}

DataDeviceInterface* Seat::Private::dataDeviceForSurface(SurfaceInterface* surface) const
{
    return interfaceForSurface(surface, dataDevices);
}

TextInputInterface* Seat::Private::textInputForSurface(SurfaceInterface* surface) const
{
    return interfaceForSurface(surface, textInputs);
}

void Seat::Private::registerDataDevice(DataDeviceInterface* dataDevice)
{
    Q_ASSERT(dataDevice->seat() == q_ptr->legacy);
    dataDevices << dataDevice;
    auto dataDeviceCleanup = [this, dataDevice] {
        dataDevices.removeOne(dataDevice);
        if (keys.focus.selection == dataDevice) {
            keys.focus.selection = nullptr;
        }
        if (currentSelection == dataDevice) {
            // current selection is cleared
            currentSelection = nullptr;
            Q_EMIT q_ptr->selectionChanged(nullptr);
            Q_EMIT q_ptr->legacy->selectionChanged(nullptr);
            if (keys.focus.selection) {
                keys.focus.selection->sendClearSelection();
            }
        }
    };
    QObject::connect(dataDevice, &QObject::destroyed, q_ptr, dataDeviceCleanup);
    QObject::connect(dataDevice, &Resource::unbound, q_ptr, dataDeviceCleanup);
    QObject::connect(dataDevice, &DataDeviceInterface::selectionChanged, q_ptr, [this, dataDevice] {
        updateSelection(dataDevice, true);
    });
    QObject::connect(dataDevice, &DataDeviceInterface::selectionCleared, q_ptr, [this, dataDevice] {
        updateSelection(dataDevice, false);
    });
    QObject::connect(dataDevice, &DataDeviceInterface::dragStarted, q_ptr, [this, dataDevice] {
        const auto dragSerial = dataDevice->dragImplicitGrabSerial();
        auto* dragSurface = dataDevice->origin();
        if (q_ptr->hasImplicitPointerGrab(dragSerial)) {
            drag.mode = Drag::Mode::Pointer;
            drag.sourcePointer = interfaceForSurface(dragSurface, pointers);
            drag.transformation = globalPointer.focus.transformation;
        } else if (q_ptr->hasImplicitTouchGrab(dragSerial)) {
            drag.mode = Drag::Mode::Touch;
            drag.sourceTouch = interfaceForSurface(dragSurface, touchs);
            // TODO: touch transformation
        } else {
            // no implicit grab, abort drag
            return;
        }
        auto* originSurface = dataDevice->origin();
        const bool proxied = originSurface->dataProxy();
        if (!proxied) {
            // origin surface
            drag.target = dataDevice;
            drag.surface = originSurface;
            // TODO: transformation needs to be either pointer or touch
            drag.transformation = globalPointer.focus.transformation;
        }
        drag.source = dataDevice;
        drag.sourcePointer = interfaceForSurface(originSurface, pointers);
        drag.destroyConnection = QObject::connect(dataDevice, &QObject::destroyed, q_ptr, [this] {
            endDrag(display()->handle()->nextSerial());
        });
        if (dataDevice->dragSource()) {
            drag.dragSourceDestroyConnection = QObject::connect(
                dataDevice->dragSource(), &Resource::aboutToBeUnbound, q_ptr, [this] {
                    const auto serial = display()->handle()->nextSerial();
                    if (drag.target) {
                        drag.target->updateDragTarget(nullptr, serial);
                        drag.target = nullptr;
                    }
                    endDrag(serial);
                });
        } else {
            drag.dragSourceDestroyConnection = QMetaObject::Connection();
        }
        dataDevice->updateDragTarget(proxied ? nullptr : originSurface,
                                     dataDevice->dragImplicitGrabSerial());
        Q_EMIT q_ptr->dragStarted();
        Q_EMIT q_ptr->dragSurfaceChanged();
        Q_EMIT q_ptr->legacy->dragStarted();
        Q_EMIT q_ptr->legacy->dragSurfaceChanged();
    });

    // Is the new DataDevice for the current keyoard focus?
    if (keys.focus.surface && !keys.focus.selection) {
        // Same client?
        if (keys.focus.surface->client() == dataDevice->client()) {
            keys.focus.selection = dataDevice;
            if (currentSelection && currentSelection->selection()) {
                dataDevice->sendSelection(currentSelection);
            }
        }
    }
}

void Seat::Private::registerTextInput(TextInputInterface* ti)
{
    // Text input version 0 might call this multiple times.
    if (textInputs.contains(ti)) {
        return;
    }
    textInputs << ti;
    if (textInput.focus.surface && textInput.focus.surface->client() == ti->client()) {
        // This is a text input for the currently focused text input surface.
        if (!textInput.focus.textInput) {
            textInput.focus.textInput = ti;
            ti->d_func()->sendEnter(textInput.focus.surface, textInput.focus.serial);
            Q_EMIT q_ptr->focusedTextInputChanged();
            Q_EMIT q_ptr->legacy->focusedTextInputChanged();
        }
    }
    QObject::connect(ti, &QObject::destroyed, q_ptr, [this, ti] {
        textInputs.removeAt(textInputs.indexOf(ti));
        if (textInput.focus.textInput == ti) {
            textInput.focus.textInput = nullptr;
            Q_EMIT q_ptr->focusedTextInputChanged();
            Q_EMIT q_ptr->legacy->focusedTextInputChanged();
        }
    });
}

void Seat::Private::endDrag(quint32 serial)
{
    auto target = drag.target;
    QObject::disconnect(drag.destroyConnection);
    QObject::disconnect(drag.dragSourceDestroyConnection);
    if (drag.source && drag.source->dragSource()) {
        drag.source->dragSource()->dropPerformed();
    }
    if (target) {
        target->drop();
        target->updateDragTarget(nullptr, serial);
    }
    drag = Drag();
    Q_EMIT q_ptr->dragSurfaceChanged();
    Q_EMIT q_ptr->dragEnded();
    Q_EMIT q_ptr->legacy->dragSurfaceChanged();
    Q_EMIT q_ptr->legacy->dragEnded();
}

void Seat::Private::cancelPreviousSelection(DataDeviceInterface* dataDevice)
{
    if (!currentSelection) {
        return;
    }
    if (auto s = currentSelection->selection()) {
        if (currentSelection != dataDevice) {
            // only if current selection is not on the same device
            // that would cancel the newly set source
            s->cancel();
        }
    }
}

void Seat::Private::updateSelection(DataDeviceInterface* dataDevice, bool set)
{
    bool selChanged = currentSelection != dataDevice;
    if (keys.focus.surface && (keys.focus.surface->client() == dataDevice->client())) {
        // cancel the previous selection
        cancelPreviousSelection(dataDevice);
        // new selection on a data device belonging to current keyboard focus
        currentSelection = dataDevice;
    }
    if (dataDevice == currentSelection) {
        // need to send out the selection
        if (keys.focus.selection) {
            if (set) {
                keys.focus.selection->sendSelection(dataDevice);
            } else {
                keys.focus.selection->sendClearSelection();
                currentSelection = nullptr;
                selChanged = true;
            }
        }
    }
    if (selChanged) {
        Q_EMIT q_ptr->selectionChanged(currentSelection);
        Q_EMIT q_ptr->legacy->selectionChanged(currentSelection);
    }
}

void Seat::setHasKeyboard(bool has)
{
    if (d_ptr->keyboard == has) {
        return;
    }
    d_ptr->keyboard = has;
    Q_EMIT hasKeyboardChanged(d_ptr->keyboard);
    Q_EMIT legacy->hasKeyboardChanged(d_ptr->keyboard);
}

void Seat::setHasPointer(bool has)
{
    if (d_ptr->pointer == has) {
        return;
    }
    d_ptr->pointer = has;
    Q_EMIT hasPointerChanged(d_ptr->pointer);
    Q_EMIT legacy->hasPointerChanged(d_ptr->pointer);
}

void Seat::setHasTouch(bool has)
{
    if (d_ptr->touch == has) {
        return;
    }
    d_ptr->touch = has;
    Q_EMIT hasTouchChanged(d_ptr->touch);
    Q_EMIT legacy->hasTouchChanged(d_ptr->touch);
}

void Seat::setName(const std::string& name)
{
    if (d_ptr->name == name) {
        return;
    }
    d_ptr->name = name;
    Q_EMIT nameChanged(d_ptr->name);
    Q_EMIT legacy->nameChanged(QString::fromStdString(d_ptr->name));
}

void Seat::Private::getPointerCallback(wl_client* wlClient, wl_resource* wlResource, uint32_t id)
{
    auto handle = fromResource(wlResource);
    auto client = handle->d_ptr->display()->handle()->getClient(wlClient);

    handle->d_ptr->getPointer(client, id, wlResource);
}

void Seat::Private::getPointer(Client* client, uint32_t id, wl_resource* resource)
{
    auto pointer
        = new Pointer(client, qMin(wl_resource_get_version(resource), s_pointerVersion), id, q_ptr);

    pointers << pointer;

    if (globalPointer.focus.surface && globalPointer.focus.surface->client() == client->legacy) {
        // this is a pointer for the currently focused pointer surface
        globalPointer.focus.pointers << pointer;
        pointer->setFocusedSurface(globalPointer.focus.serial, globalPointer.focus.surface);
        pointer->d_ptr->sendFrame();
        if (globalPointer.focus.pointers.count() == 1) {
            // got a new pointer
            Q_EMIT q_ptr->focusedPointerChanged(pointer);
        }
    }

    connect(pointer, &Pointer::resourceDestroyed, q_ptr, [pointer, this] {
        pointers.removeAt(pointers.indexOf(pointer));
        if (globalPointer.focus.pointers.removeOne(pointer)) {
            if (globalPointer.focus.pointers.isEmpty()) {
                Q_EMIT q_ptr->focusedPointerChanged(nullptr);
            }
        }
    });

    Q_EMIT q_ptr->pointerCreated(pointer);
}

void Seat::Private::getKeyboardCallback(wl_client* wlClient, wl_resource* wlResource, uint32_t id)
{
    auto handle = fromResource(wlResource);
    auto client = handle->d_ptr->display()->handle()->getClient(wlClient);

    handle->d_ptr->getKeyboard(client, id, wlResource);
}

void Seat::Private::getKeyboard(Client* client, uint32_t id, wl_resource* resource)
{
    // TODO: only create if seat has keyboard?
    auto keyboard = new Keyboard(
        client, qMin(wl_resource_get_version(resource), s_keyboardVersion), id, q_ptr);

    keyboard->repeatInfo(keys.keyRepeat.charactersPerSecond, keys.keyRepeat.delay);

    if (keys.keymap.xkbcommonCompatible) {
        keyboard->setKeymap(keys.keymap.fd, keys.keymap.size);
    }

    keyboards << keyboard;

    if (keys.focus.surface && keys.focus.surface->client() == client->legacy) {
        // this is a keyboard for the currently focused keyboard surface
        keys.focus.keyboards << keyboard;
        keyboard->setFocusedSurface(keys.focus.serial, keys.focus.surface);
    }

    QObject::connect(keyboard, &Keyboard::resourceDestroyed, q_ptr, [keyboard, this] {
        keyboards.removeAt(keyboards.indexOf(keyboard));
        keys.focus.keyboards.removeOne(keyboard);
    });
    Q_EMIT q_ptr->keyboardCreated(keyboard);
}

void Seat::Private::getTouchCallback(wl_client* wlClient, wl_resource* wlResource, uint32_t id)
{
    auto handle = fromResource(wlResource);
    auto client = handle->d_ptr->display()->handle()->getClient(wlClient);

    handle->d_ptr->getTouch(client, id, wlResource);
}

void Seat::Private::getTouch(Client* client, uint32_t id, wl_resource* resource)
{
    // TODO: only create if seat has touch?
    auto touch = new TouchInterface(q_ptr->legacy, resource);

    touch->create(client->legacy, qMin(wl_resource_get_version(resource), s_touchVersion), id);
    if (!touch->resource()) {
        wl_resource_post_no_memory(resource);
        delete touch;
        return;
    }

    touchs << touch;

    if (globalTouch.focus.surface && globalTouch.focus.surface->client() == client->legacy) {
        // this is a touch for the currently focused touch surface
        globalTouch.focus.touchs << touch;
        if (!globalTouch.ids.isEmpty()) {
            // TODO: send out all the points
        }
    }
    QObject::connect(touch, &QObject::destroyed, q_ptr, [touch, this] {
        touchs.removeAt(touchs.indexOf(touch));
        globalTouch.focus.touchs.removeOne(touch);
    });
    Q_EMIT q_ptr->touchCreated(touch);
    Q_EMIT q_ptr->legacy->touchCreated(touch);
}

std::string Seat::name() const
{
    return d_ptr->name;
}

bool Seat::hasPointer() const
{
    return d_ptr->pointer;
}

bool Seat::hasKeyboard() const
{
    return d_ptr->keyboard;
}

bool Seat::hasTouch() const
{
    return d_ptr->touch;
}

Seat* Seat::get(void* data)
{
    auto resource = reinterpret_cast<Wayland::Resource<Seat, Wayland::Global<Seat>>*>(data);
    auto seatPriv = static_cast<Seat::Private*>(resource->global());
    return seatPriv->q_ptr;
}

QPointF Seat::pointerPos() const
{
    return d_ptr->globalPointer.pos;
}

void Seat::setPointerPos(const QPointF& pos)
{
    if (d_ptr->globalPointer.pos == pos) {
        return;
    }
    d_ptr->globalPointer.pos = pos;
    Q_EMIT pointerPosChanged(pos);
    Q_EMIT legacy->pointerPosChanged(pos);
}

quint32 Seat::timestamp() const
{
    return d_ptr->timestamp;
}

void Seat::setTimestamp(quint32 time)
{
    if (d_ptr->timestamp == time) {
        return;
    }
    d_ptr->timestamp = time;
    Q_EMIT timestampChanged(time);
    Q_EMIT legacy->timestampChanged(time);
}

void Seat::setDragTarget(SurfaceInterface* surface,
                         const QPointF& globalPosition,
                         const QMatrix4x4& inputTransformation)
{
    if (surface == d_ptr->drag.surface) {
        // no change
        return;
    }
    const quint32 serial = d_ptr->display()->handle()->nextSerial();
    if (d_ptr->drag.target) {
        d_ptr->drag.target->updateDragTarget(nullptr, serial);
    }
    d_ptr->drag.target = d_ptr->dataDeviceForSurface(surface);
    if (d_ptr->drag.mode == Private::Drag::Mode::Pointer) {
        setPointerPos(globalPosition);
    } else if (d_ptr->drag.mode == Private::Drag::Mode::Touch
               && d_ptr->globalTouch.focus.firstTouchPos != globalPosition) {
        touchMove(d_ptr->globalTouch.ids.first(), globalPosition);
    }
    if (d_ptr->drag.target) {
        d_ptr->drag.surface = surface;
        d_ptr->drag.transformation = inputTransformation;
        d_ptr->drag.target->updateDragTarget(surface, serial);
    } else {
        d_ptr->drag.surface = nullptr;
    }
    Q_EMIT dragSurfaceChanged();
    Q_EMIT legacy->dragSurfaceChanged();
    return;
}

void Seat::setDragTarget(SurfaceInterface* surface, const QMatrix4x4& inputTransformation)
{
    if (d_ptr->drag.mode == Private::Drag::Mode::Pointer) {
        setDragTarget(surface, pointerPos(), inputTransformation);
    } else {
        Q_ASSERT(d_ptr->drag.mode == Private::Drag::Mode::Touch);
        setDragTarget(surface, d_ptr->globalTouch.focus.firstTouchPos, inputTransformation);
    }
}

SurfaceInterface* Seat::focusedPointerSurface() const
{
    return d_ptr->globalPointer.focus.surface;
}

void Seat::setFocusedPointerSurface(SurfaceInterface* surface, const QPointF& surfacePosition)
{
    QMatrix4x4 m;
    m.translate(-surfacePosition.x(), -surfacePosition.y());
    setFocusedPointerSurface(surface, m);

    if (d_ptr->globalPointer.focus.surface) {
        d_ptr->globalPointer.focus.offset = surfacePosition;
    }
}

void Seat::setFocusedPointerSurface(SurfaceInterface* surface, const QMatrix4x4& transformation)
{
    if (d_ptr->drag.mode == Private::Drag::Mode::Pointer) {
        // ignore
        return;
    }

    const quint32 serial = d_ptr->display()->handle()->nextSerial();
    QSet<Pointer*> framePointers;

    for (auto it = d_ptr->globalPointer.focus.pointers.constBegin(),
              end = d_ptr->globalPointer.focus.pointers.constEnd();
         it != end;
         ++it) {
        (*it)->setFocusedSurface(serial, nullptr);
        framePointers << *it;
    }

    if (d_ptr->globalPointer.focus.surface) {
        disconnect(d_ptr->globalPointer.focus.destroyConnection);
    }

    d_ptr->globalPointer.focus = Private::SeatPointer::Focus();
    d_ptr->globalPointer.focus.surface = surface;

    auto p = d_ptr->pointersForSurface(surface);
    d_ptr->globalPointer.focus.pointers = p;

    if (d_ptr->globalPointer.focus.surface) {
        d_ptr->globalPointer.focus.destroyConnection
            = connect(surface, &QObject::destroyed, this, [this] {
                  d_ptr->globalPointer.focus = Private::SeatPointer::Focus();
                  Q_EMIT focusedPointerChanged(nullptr);
              });
        d_ptr->globalPointer.focus.offset = QPointF();
        d_ptr->globalPointer.focus.transformation = transformation;
        d_ptr->globalPointer.focus.serial = serial;
    }

    if (p.isEmpty()) {
        Q_EMIT focusedPointerChanged(nullptr);
        for (auto p : qAsConst(framePointers)) {
            p->d_ptr->sendFrame();
        }
        return;
    }

    // TODO: signal with all pointers
    Q_EMIT focusedPointerChanged(p.first());

    for (auto it = p.constBegin(), end = p.constEnd(); it != end; ++it) {
        (*it)->setFocusedSurface(serial, surface);
        framePointers << *it;
    }

    for (auto p : qAsConst(framePointers)) {
        p->d_ptr->sendFrame();
    }
}

Pointer* Seat::focusedPointer() const
{
    if (d_ptr->globalPointer.focus.pointers.isEmpty()) {
        return nullptr;
    }
    return d_ptr->globalPointer.focus.pointers.first();
}

void Seat::setFocusedPointerSurfacePosition(const QPointF& surfacePosition)
{
    if (d_ptr->globalPointer.focus.surface) {
        d_ptr->globalPointer.focus.offset = surfacePosition;
        d_ptr->globalPointer.focus.transformation = QMatrix4x4();
        d_ptr->globalPointer.focus.transformation.translate(-surfacePosition.x(),
                                                            -surfacePosition.y());
    }
}

QPointF Seat::focusedPointerSurfacePosition() const
{
    return d_ptr->globalPointer.focus.offset;
}

void Seat::setFocusedPointerSurfaceTransformation(const QMatrix4x4& transformation)
{

    if (d_ptr->globalPointer.focus.surface) {
        d_ptr->globalPointer.focus.transformation = transformation;
    }
}

QMatrix4x4 Seat::focusedPointerSurfaceTransformation() const
{
    return d_ptr->globalPointer.focus.transformation;
}

namespace
{
static quint32 qtToWaylandButton(Qt::MouseButton button)
{
#if HAVE_LINUX_INPUT_H
    static const QHash<Qt::MouseButton, quint32> s_buttons({
        {Qt::LeftButton, BTN_LEFT},
        {Qt::RightButton, BTN_RIGHT},
        {Qt::MiddleButton, BTN_MIDDLE},
        {Qt::ExtraButton1, BTN_BACK},    // note: QtWayland maps BTN_SIDE
        {Qt::ExtraButton2, BTN_FORWARD}, // note: QtWayland maps BTN_EXTRA
        {Qt::ExtraButton3, BTN_TASK},    // note: QtWayland maps BTN_FORWARD
        {Qt::ExtraButton4, BTN_EXTRA},   // note: QtWayland maps BTN_BACK
        {Qt::ExtraButton5, BTN_SIDE},    // note: QtWayland maps BTN_TASK
        {Qt::ExtraButton6, BTN_TASK + 1},
        {Qt::ExtraButton7, BTN_TASK + 2},
        {Qt::ExtraButton8, BTN_TASK + 3},
        {Qt::ExtraButton9, BTN_TASK + 4},
        {Qt::ExtraButton10, BTN_TASK + 5},
        {Qt::ExtraButton11, BTN_TASK + 6},
        {Qt::ExtraButton12, BTN_TASK + 7},
        {Qt::ExtraButton13, BTN_TASK + 8}
        // further mapping not possible, 0x120 is BTN_JOYSTICK
    });
    return s_buttons.value(button, 0);
#else
    return 0;
#endif
}
}

bool Seat::isPointerButtonPressed(Qt::MouseButton button) const
{
    return isPointerButtonPressed(qtToWaylandButton(button));
}

bool Seat::isPointerButtonPressed(quint32 button) const
{
    auto it = d_ptr->globalPointer.buttonStates.constFind(button);
    if (it == d_ptr->globalPointer.buttonStates.constEnd()) {
        return false;
    }
    return it.value() == Private::SeatPointer::State::Pressed ? true : false;
}

void Seat::pointerAxisV5(Qt::Orientation orientation,
                         qreal delta,
                         qint32 discreteDelta,
                         PointerAxisSource source)
{
    if (d_ptr->drag.mode == Private::Drag::Mode::Pointer) {
        // ignore
        return;
    }
    if (d_ptr->globalPointer.focus.surface) {
        for (auto it = d_ptr->globalPointer.focus.pointers.constBegin(),
                  end = d_ptr->globalPointer.focus.pointers.constEnd();
             it != end;
             ++it) {
            (*it)->axis(orientation, delta, discreteDelta, source);
        }
    }
}

void Seat::pointerAxis(Qt::Orientation orientation, quint32 delta)
{
    if (d_ptr->drag.mode == Private::Drag::Mode::Pointer) {
        // ignore
        return;
    }
    if (d_ptr->globalPointer.focus.surface) {
        for (auto it = d_ptr->globalPointer.focus.pointers.constBegin(),
                  end = d_ptr->globalPointer.focus.pointers.constEnd();
             it != end;
             ++it) {
            (*it)->axis(orientation, delta);
        }
    }
}

void Seat::pointerButtonPressed(Qt::MouseButton button)
{
    const quint32 nativeButton = qtToWaylandButton(button);
    if (nativeButton == 0) {
        return;
    }
    pointerButtonPressed(nativeButton);
}

void Seat::pointerButtonPressed(quint32 button)
{
    const quint32 serial = d_ptr->display()->handle()->nextSerial();
    d_ptr->updatePointerButtonSerial(button, serial);
    d_ptr->updatePointerButtonState(button, Private::SeatPointer::State::Pressed);
    if (d_ptr->drag.mode == Private::Drag::Mode::Pointer) {
        // ignore
        return;
    }
    if (auto* focusSurface = d_ptr->globalPointer.focus.surface) {
        for (auto it = d_ptr->globalPointer.focus.pointers.constBegin(),
                  end = d_ptr->globalPointer.focus.pointers.constEnd();
             it != end;
             ++it) {
            (*it)->buttonPressed(serial, button);
        }
        if (focusSurface == d_ptr->keys.focus.surface) {
            // update the focused child surface
            auto p = focusedPointer();
            if (p) {
                for (auto it = d_ptr->keys.focus.keyboards.constBegin(),
                          end = d_ptr->keys.focus.keyboards.constEnd();
                     it != end;
                     ++it) {
                    (*it)->d_ptr->focusChildSurface(serial, p->d_ptr->focusedChildSurface);
                }
            }
        }
    }
}

void Seat::pointerButtonReleased(Qt::MouseButton button)
{
    const quint32 nativeButton = qtToWaylandButton(button);
    if (nativeButton == 0) {
        return;
    }
    pointerButtonReleased(nativeButton);
}

void Seat::pointerButtonReleased(quint32 button)
{
    const quint32 serial = d_ptr->display()->handle()->nextSerial();
    const quint32 currentButtonSerial = pointerButtonSerial(button);
    d_ptr->updatePointerButtonSerial(button, serial);
    d_ptr->updatePointerButtonState(button, Private::SeatPointer::State::Released);
    if (d_ptr->drag.mode == Private::Drag::Mode::Pointer) {
        if (d_ptr->drag.source->dragImplicitGrabSerial() != currentButtonSerial) {
            // not our drag button - ignore
            return;
        }
        d_ptr->endDrag(serial);
        return;
    }
    if (d_ptr->globalPointer.focus.surface) {
        for (auto it = d_ptr->globalPointer.focus.pointers.constBegin(),
                  end = d_ptr->globalPointer.focus.pointers.constEnd();
             it != end;
             ++it) {
            (*it)->buttonReleased(serial, button);
        }
    }
}

quint32 Seat::pointerButtonSerial(Qt::MouseButton button) const
{
    return pointerButtonSerial(qtToWaylandButton(button));
}

quint32 Seat::pointerButtonSerial(quint32 button) const
{
    auto it = d_ptr->globalPointer.buttonSerials.constFind(button);
    if (it == d_ptr->globalPointer.buttonSerials.constEnd()) {
        return 0;
    }
    return it.value();
}

void Seat::relativePointerMotion(const QSizeF& delta,
                                 const QSizeF& deltaNonAccelerated,
                                 quint64 microseconds)
{
    if (d_ptr->globalPointer.focus.surface) {
        for (auto it = d_ptr->globalPointer.focus.pointers.constBegin(),
                  end = d_ptr->globalPointer.focus.pointers.constEnd();
             it != end;
             ++it) {
            (*it)->relativeMotion(delta, deltaNonAccelerated, microseconds);
        }
    }
}

void Seat::startPointerSwipeGesture(quint32 fingerCount)
{
    if (!d_ptr->globalPointer.gestureSurface.isNull()) {
        return;
    }
    d_ptr->globalPointer.gestureSurface
        = QPointer<SurfaceInterface>(d_ptr->globalPointer.focus.surface);
    if (d_ptr->globalPointer.gestureSurface.isNull()) {
        return;
    }
    const quint32 serial = d_ptr->display()->handle()->nextSerial();
    forEachInterface<Pointer>(
        d_ptr->globalPointer.gestureSurface.data(),
        d_ptr->pointers,
        [serial, fingerCount](Pointer* p) { p->d_ptr->startSwipeGesture(serial, fingerCount); });
}

void Seat::updatePointerSwipeGesture(const QSizeF& delta)
{
    if (d_ptr->globalPointer.gestureSurface.isNull()) {
        return;
    }
    forEachInterface<Pointer>(d_ptr->globalPointer.gestureSurface.data(),
                              d_ptr->pointers,
                              [delta](Pointer* p) { p->d_ptr->updateSwipeGesture(delta); });
}

void Seat::endPointerSwipeGesture()
{
    if (d_ptr->globalPointer.gestureSurface.isNull()) {
        return;
    }
    const quint32 serial = d_ptr->display()->handle()->nextSerial();
    forEachInterface<Pointer>(d_ptr->globalPointer.gestureSurface.data(),
                              d_ptr->pointers,
                              [serial](Pointer* p) { p->d_ptr->endSwipeGesture(serial); });
    d_ptr->globalPointer.gestureSurface.clear();
}

void Seat::cancelPointerSwipeGesture()
{
    if (d_ptr->globalPointer.gestureSurface.isNull()) {
        return;
    }
    const quint32 serial = d_ptr->display()->handle()->nextSerial();
    forEachInterface<Pointer>(d_ptr->globalPointer.gestureSurface.data(),
                              d_ptr->pointers,
                              [serial](Pointer* p) { p->d_ptr->cancelSwipeGesture(serial); });
    d_ptr->globalPointer.gestureSurface.clear();
}

void Seat::startPointerPinchGesture(quint32 fingerCount)
{
    if (!d_ptr->globalPointer.gestureSurface.isNull()) {
        return;
    }
    d_ptr->globalPointer.gestureSurface
        = QPointer<SurfaceInterface>(d_ptr->globalPointer.focus.surface);
    if (d_ptr->globalPointer.gestureSurface.isNull()) {
        return;
    }
    const quint32 serial = d_ptr->display()->handle()->nextSerial();
    forEachInterface<Pointer>(
        d_ptr->globalPointer.gestureSurface.data(),
        d_ptr->pointers,
        [serial, fingerCount](Pointer* p) { p->d_ptr->startPinchGesture(serial, fingerCount); });
}

void Seat::updatePointerPinchGesture(const QSizeF& delta, qreal scale, qreal rotation)
{
    if (d_ptr->globalPointer.gestureSurface.isNull()) {
        return;
    }
    forEachInterface<Pointer>(d_ptr->globalPointer.gestureSurface.data(),
                              d_ptr->pointers,
                              [delta, scale, rotation](Pointer* p) {
                                  p->d_ptr->updatePinchGesture(delta, scale, rotation);
                              });
}

void Seat::endPointerPinchGesture()
{
    if (d_ptr->globalPointer.gestureSurface.isNull()) {
        return;
    }
    const quint32 serial = d_ptr->display()->handle()->nextSerial();
    forEachInterface<Pointer>(d_ptr->globalPointer.gestureSurface.data(),
                              d_ptr->pointers,
                              [serial](Pointer* p) { p->d_ptr->endPinchGesture(serial); });
    d_ptr->globalPointer.gestureSurface.clear();
}

void Seat::cancelPointerPinchGesture()
{
    if (d_ptr->globalPointer.gestureSurface.isNull()) {
        return;
    }
    const quint32 serial = d_ptr->display()->handle()->nextSerial();
    forEachInterface<Pointer>(d_ptr->globalPointer.gestureSurface.data(),
                              d_ptr->pointers,
                              [serial](Pointer* p) { p->d_ptr->cancelPinchGesture(serial); });
    d_ptr->globalPointer.gestureSurface.clear();
}

void Seat::keyPressed(quint32 key)
{
    d_ptr->keys.lastStateSerial = d_ptr->display()->handle()->nextSerial();
    if (!d_ptr->updateKey(key, Private::SeatKeyboard::State::Pressed)) {
        return;
    }
    if (d_ptr->keys.focus.surface) {
        for (auto it = d_ptr->keys.focus.keyboards.constBegin(),
                  end = d_ptr->keys.focus.keyboards.constEnd();
             it != end;
             ++it) {
            (*it)->keyPressed(d_ptr->keys.lastStateSerial, key);
        }
    }
}

void Seat::keyReleased(quint32 key)
{
    d_ptr->keys.lastStateSerial = d_ptr->display()->handle()->nextSerial();
    if (!d_ptr->updateKey(key, Private::SeatKeyboard::State::Released)) {
        return;
    }
    if (d_ptr->keys.focus.surface) {
        for (auto it = d_ptr->keys.focus.keyboards.constBegin(),
                  end = d_ptr->keys.focus.keyboards.constEnd();
             it != end;
             ++it) {
            (*it)->keyReleased(d_ptr->keys.lastStateSerial, key);
        }
    }
}

SurfaceInterface* Seat::focusedKeyboardSurface() const
{
    return d_ptr->keys.focus.surface;
}

void Seat::setFocusedKeyboardSurface(SurfaceInterface* surface)
{
    const quint32 serial = d_ptr->display()->handle()->nextSerial();

    for (auto it = d_ptr->keys.focus.keyboards.constBegin(),
              end = d_ptr->keys.focus.keyboards.constEnd();
         it != end;
         ++it) {
        (*it)->setFocusedSurface(serial, nullptr);
    }

    if (d_ptr->keys.focus.surface) {
        disconnect(d_ptr->keys.focus.destroyConnection);
    }

    d_ptr->keys.focus = Private::SeatKeyboard::Focus();
    d_ptr->keys.focus.surface = surface;
    d_ptr->keys.focus.keyboards = d_ptr->keyboardsForSurface(surface);

    if (d_ptr->keys.focus.surface) {
        d_ptr->keys.focus.destroyConnection = connect(surface, &QObject::destroyed, this, [this] {
            d_ptr->keys.focus = Private::SeatKeyboard::Focus();
        });
        d_ptr->keys.focus.serial = serial;

        // selection?
        d_ptr->keys.focus.selection = d_ptr->dataDeviceForSurface(surface);
        if (d_ptr->keys.focus.selection) {
            if (d_ptr->currentSelection && d_ptr->currentSelection->selection()) {
                d_ptr->keys.focus.selection->sendSelection(d_ptr->currentSelection);
            } else {
                d_ptr->keys.focus.selection->sendClearSelection();
            }
        }
    }

    for (auto it = d_ptr->keys.focus.keyboards.constBegin(),
              end = d_ptr->keys.focus.keyboards.constEnd();
         it != end;
         ++it) {
        (*it)->setFocusedSurface(serial, surface);
    }

    // Focused text input surface follows keyboard.
    if (hasKeyboard()) {
        setFocusedTextInputSurface(surface);
    }
}

void Seat::setKeymap(int fd, quint32 size)
{
    d_ptr->keys.keymap.xkbcommonCompatible = true;
    d_ptr->keys.keymap.fd = fd;
    d_ptr->keys.keymap.size = size;
    for (auto it = d_ptr->keyboards.constBegin(); it != d_ptr->keyboards.constEnd(); ++it) {
        (*it)->setKeymap(fd, size);
    }
}

void Seat::updateKeyboardModifiers(quint32 depressed,
                                   quint32 latched,
                                   quint32 locked,
                                   quint32 group)
{
    bool changed = false;
#define UPDATE(value)                                                                              \
    if (d_ptr->keys.modifiers.value != value) {                                                    \
        d_ptr->keys.modifiers.value = value;                                                       \
        changed = true;                                                                            \
    }
    UPDATE(depressed)
    UPDATE(latched)
    UPDATE(locked)
    UPDATE(group)
    if (!changed) {
        return;
    }
    const quint32 serial = d_ptr->display()->handle()->nextSerial();
    d_ptr->keys.modifiers.serial = serial;
    if (d_ptr->keys.focus.surface) {
        for (auto it = d_ptr->keys.focus.keyboards.constBegin(),
                  end = d_ptr->keys.focus.keyboards.constEnd();
             it != end;
             ++it) {
            (*it)->updateModifiers(serial, depressed, latched, locked, group);
        }
    }
}

void Seat::setKeyRepeatInfo(qint32 charactersPerSecond, qint32 delay)
{
    d_ptr->keys.keyRepeat.charactersPerSecond = qMax(charactersPerSecond, 0);
    d_ptr->keys.keyRepeat.delay = qMax(delay, 0);
    for (auto it = d_ptr->keyboards.constBegin(); it != d_ptr->keyboards.constEnd(); ++it) {
        (*it)->repeatInfo(d_ptr->keys.keyRepeat.charactersPerSecond, d_ptr->keys.keyRepeat.delay);
    }
}

qint32 Seat::keyRepeatDelay() const
{
    return d_ptr->keys.keyRepeat.delay;
}

qint32 Seat::keyRepeatRate() const
{
    return d_ptr->keys.keyRepeat.charactersPerSecond;
}

bool Seat::isKeymapXkbCompatible() const
{
    return d_ptr->keys.keymap.xkbcommonCompatible;
}

int Seat::keymapFileDescriptor() const
{
    return d_ptr->keys.keymap.fd;
}

quint32 Seat::keymapSize() const
{
    return d_ptr->keys.keymap.size;
}

quint32 Seat::depressedModifiers() const
{
    return d_ptr->keys.modifiers.depressed;
}

quint32 Seat::groupModifiers() const
{
    return d_ptr->keys.modifiers.group;
}

quint32 Seat::latchedModifiers() const
{
    return d_ptr->keys.modifiers.latched;
}

quint32 Seat::lockedModifiers() const
{
    return d_ptr->keys.modifiers.locked;
}

quint32 Seat::lastModifiersSerial() const
{
    return d_ptr->keys.modifiers.serial;
}

QVector<quint32> Seat::pressedKeys() const
{
    QVector<quint32> keys;
    for (auto it = d_ptr->keys.states.constBegin(); it != d_ptr->keys.states.constEnd(); ++it) {
        if (it.value() == Private::SeatKeyboard::State::Pressed) {
            keys << it.key();
        }
    }
    return keys;
}

Keyboard* Seat::focusedKeyboard() const
{
    if (d_ptr->keys.focus.keyboards.isEmpty()) {
        return nullptr;
    }
    return d_ptr->keys.focus.keyboards.first();
}

void Seat::cancelTouchSequence()
{
    for (auto it = d_ptr->globalTouch.focus.touchs.constBegin(),
              end = d_ptr->globalTouch.focus.touchs.constEnd();
         it != end;
         ++it) {
        (*it)->cancel();
    }
    if (d_ptr->drag.mode == Private::Drag::Mode::Touch) {
        // cancel the drag, don't drop.
        if (d_ptr->drag.target) {
            // remove the current target
            d_ptr->drag.target->updateDragTarget(nullptr, 0);
            d_ptr->drag.target = nullptr;
        }
        // and end the drag for the source, serial does not matter
        d_ptr->endDrag(0);
    }
    d_ptr->globalTouch.ids.clear();
}

TouchInterface* Seat::focusedTouch() const
{
    if (d_ptr->globalTouch.focus.touchs.isEmpty()) {
        return nullptr;
    }
    return d_ptr->globalTouch.focus.touchs.first();
}

SurfaceInterface* Seat::focusedTouchSurface() const
{
    return d_ptr->globalTouch.focus.surface;
}

QPointF Seat::focusedTouchSurfacePosition() const
{
    return d_ptr->globalTouch.focus.offset;
}

bool Seat::isTouchSequence() const
{
    return !d_ptr->globalTouch.ids.isEmpty();
}

void Seat::setFocusedTouchSurface(SurfaceInterface* surface, const QPointF& surfacePosition)
{
    if (isTouchSequence()) {
        // changing surface not allowed during a touch sequence
        return;
    }
    Q_ASSERT(!isDragTouch());

    if (d_ptr->globalTouch.focus.surface) {
        disconnect(d_ptr->globalTouch.focus.destroyConnection);
    }
    d_ptr->globalTouch.focus = Private::Touch::Focus();
    d_ptr->globalTouch.focus.surface = surface;
    d_ptr->globalTouch.focus.offset = surfacePosition;
    d_ptr->globalTouch.focus.touchs = d_ptr->touchsForSurface(surface);
    if (d_ptr->globalTouch.focus.surface) {
        d_ptr->globalTouch.focus.destroyConnection
            = connect(surface, &QObject::destroyed, this, [this] {
                  if (isTouchSequence()) {
                      // Surface destroyed during touch sequence - send a cancel
                      for (auto it = d_ptr->globalTouch.focus.touchs.constBegin(),
                                end = d_ptr->globalTouch.focus.touchs.constEnd();
                           it != end;
                           ++it) {
                          (*it)->cancel();
                      }
                  }
                  d_ptr->globalTouch.focus = Private::Touch::Focus();
              });
    }
}

void Seat::setFocusedTouchSurfacePosition(const QPointF& surfacePosition)
{
    d_ptr->globalTouch.focus.offset = surfacePosition;
}

qint32 Seat::touchDown(const QPointF& globalPosition)
{
    const qint32 id = d_ptr->globalTouch.ids.isEmpty() ? 0 : d_ptr->globalTouch.ids.lastKey() + 1;
    const qint32 serial = d_ptr->display()->handle()->nextSerial();
    const auto pos = globalPosition - d_ptr->globalTouch.focus.offset;
    for (auto it = d_ptr->globalTouch.focus.touchs.constBegin(),
              end = d_ptr->globalTouch.focus.touchs.constEnd();
         it != end;
         ++it) {
        (*it)->down(id, serial, pos);
    }

    if (id == 0) {
        d_ptr->globalTouch.focus.firstTouchPos = globalPosition;
    }

#if HAVE_LINUX_INPUT_H
    if (id == 0 && d_ptr->globalTouch.focus.touchs.isEmpty()) {
        // If the client did not bind the touch interface fall back
        // to at least emulating touch through pointer events.
        forEachInterface<Pointer>(
            focusedTouchSurface(), d_ptr->pointers, [this, pos, serial](Pointer* p) {
                p->d_ptr->sendEnter(serial, focusedTouchSurface(), pos);
                p->d_ptr->sendMotion(pos);
                p->buttonPressed(serial, BTN_LEFT);
                p->d_ptr->sendFrame();
            });
    }
#endif

    d_ptr->globalTouch.ids[id] = serial;
    return id;
}

void Seat::touchMove(qint32 id, const QPointF& globalPosition)
{
    Q_ASSERT(d_ptr->globalTouch.ids.contains(id));
    const auto pos = globalPosition - d_ptr->globalTouch.focus.offset;
    for (auto it = d_ptr->globalTouch.focus.touchs.constBegin(),
              end = d_ptr->globalTouch.focus.touchs.constEnd();
         it != end;
         ++it) {
        (*it)->move(id, pos);
    }

    if (id == 0) {
        d_ptr->globalTouch.focus.firstTouchPos = globalPosition;
    }

    if (id == 0 && d_ptr->globalTouch.focus.touchs.isEmpty()) {
        // Client did not bind touch, fall back to emulating with pointer events.
        forEachInterface<Pointer>(focusedTouchSurface(), d_ptr->pointers, [this, pos](Pointer* p) {
            p->d_ptr->sendMotion(pos);
        });
    }
    Q_EMIT touchMoved(id, d_ptr->globalTouch.ids[id], globalPosition);
    Q_EMIT legacy->touchMoved(id, d_ptr->globalTouch.ids[id], globalPosition);
}

void Seat::touchUp(qint32 id)
{
    Q_ASSERT(d_ptr->globalTouch.ids.contains(id));
    const qint32 serial = d_ptr->display()->handle()->nextSerial();
    if (d_ptr->drag.mode == Private::Drag::Mode::Touch
        && d_ptr->drag.source->dragImplicitGrabSerial() == d_ptr->globalTouch.ids.value(id)) {
        // the implicitly grabbing touch point has been upped
        d_ptr->endDrag(serial);
    }
    for (auto it = d_ptr->globalTouch.focus.touchs.constBegin(),
              end = d_ptr->globalTouch.focus.touchs.constEnd();
         it != end;
         ++it) {
        (*it)->up(id, serial);
    }

#if HAVE_LINUX_INPUT_H
    if (id == 0 && d_ptr->globalTouch.focus.touchs.isEmpty()) {
        // Client did not bind touch, fall back to emulating with pointer events.
        const quint32 serial = d_ptr->display()->handle()->nextSerial();
        forEachInterface<Pointer>(
            focusedTouchSurface(), d_ptr->pointers, [this, serial](Pointer* p) {
                p->buttonReleased(serial, BTN_LEFT);
            });
    }
#endif

    d_ptr->globalTouch.ids.remove(id);
}

void Seat::touchFrame()
{
    for (auto it = d_ptr->globalTouch.focus.touchs.constBegin(),
              end = d_ptr->globalTouch.focus.touchs.constEnd();
         it != end;
         ++it) {
        (*it)->frame();
    }
}

bool Seat::hasImplicitTouchGrab(quint32 serial) const
{
    if (!d_ptr->globalTouch.focus.surface) {
        // origin surface has been destroyed
        return false;
    }
    return d_ptr->globalTouch.ids.key(serial, -1) != -1;
}

bool Seat::isDrag() const
{
    return d_ptr->drag.mode != Private::Drag::Mode::None;
}

bool Seat::isDragPointer() const
{
    return d_ptr->drag.mode == Private::Drag::Mode::Pointer;
}

bool Seat::isDragTouch() const
{
    return d_ptr->drag.mode == Private::Drag::Mode::Touch;
}

bool Seat::hasImplicitPointerGrab(quint32 serial) const
{
    const auto& serials = d_ptr->globalPointer.buttonSerials;
    for (auto it = serials.constBegin(), end = serials.constEnd(); it != end; it++) {
        if (it.value() == serial) {
            return isPointerButtonPressed(it.key());
        }
    }
    return false;
}

QMatrix4x4 Seat::dragSurfaceTransformation() const
{
    return d_ptr->drag.transformation;
}

SurfaceInterface* Seat::dragSurface() const
{
    return d_ptr->drag.surface;
}

Pointer* Seat::dragPointer() const
{
    if (d_ptr->drag.mode != Private::Drag::Mode::Pointer) {
        return nullptr;
    }

    return d_ptr->drag.sourcePointer;
}

DataDeviceInterface* Seat::dragSource() const
{
    return d_ptr->drag.source;
}

void Seat::setFocusedTextInputSurface(SurfaceInterface* surface)
{
    const quint32 serial = d_ptr->display()->handle()->nextSerial();
    const auto old = d_ptr->textInput.focus.textInput;
    if (d_ptr->textInput.focus.textInput) {
        // TODO: setFocusedSurface like in other interfaces
        d_ptr->textInput.focus.textInput->d_func()->sendLeave(serial,
                                                              d_ptr->textInput.focus.surface);
    }
    if (d_ptr->textInput.focus.surface) {
        disconnect(d_ptr->textInput.focus.destroyConnection);
    }
    d_ptr->textInput.focus = Private::TextInput::Focus();
    d_ptr->textInput.focus.surface = surface;
    TextInputInterface* t = d_ptr->textInputForSurface(surface);
    if (t && !t->resource()) {
        t = nullptr;
    }
    d_ptr->textInput.focus.textInput = t;
    if (d_ptr->textInput.focus.surface) {
        d_ptr->textInput.focus.destroyConnection
            = connect(surface, &Resource::aboutToBeUnbound, this, [this] {
                  setFocusedTextInputSurface(nullptr);
              });
        d_ptr->textInput.focus.serial = serial;
    }
    if (t) {
        // TODO: setFocusedSurface like in other interfaces
        t->d_func()->sendEnter(surface, serial);
    }
    if (old != t) {
        Q_EMIT focusedTextInputChanged();
        Q_EMIT legacy->focusedTextInputChanged();
    }
}

SurfaceInterface* Seat::focusedTextInputSurface() const
{
    return d_ptr->textInput.focus.surface;
}

TextInputInterface* Seat::focusedTextInput() const
{
    return d_ptr->textInput.focus.textInput;
}

DataDeviceInterface* Seat::selection() const
{
    return d_ptr->currentSelection;
}

void Seat::setSelection(DataDeviceInterface* dataDevice)
{
    if (d_ptr->currentSelection == dataDevice) {
        return;
    }
    // cancel the previous selection
    d_ptr->cancelPreviousSelection(dataDevice);
    d_ptr->currentSelection = dataDevice;
    if (d_ptr->keys.focus.selection) {
        if (dataDevice && dataDevice->selection()) {
            d_ptr->keys.focus.selection->sendSelection(dataDevice);
        } else {
            d_ptr->keys.focus.selection->sendClearSelection();
        }
    }
    Q_EMIT selectionChanged(dataDevice);
    Q_EMIT legacy->selectionChanged(dataDevice);
}

}
}
