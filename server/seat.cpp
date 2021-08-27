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
#include "data_device.h"
#include "data_source.h"
#include "display.h"
#include "input_method_v2.h"
#include "keyboard.h"
#include "keyboard_p.h"
#include "pointer.h"
#include "pointer_p.h"
#include "primary_selection.h"
#include "surface.h"
#include "text_input_v2_p.h"
#include "text_input_v3_p.h"
#include "touch.h"

#include <config-wrapland.h>

#ifndef WL_SEAT_NAME_SINCE_VERSION
#define WL_SEAT_NAME_SINCE_VERSION 2
#endif

#if HAVE_LINUX_INPUT_H
#include <linux/input.h>
#endif

#include <algorithm>
#include <unordered_set>

namespace Wrapland::Server
{

Seat::Private::Private(Seat* q, Display* display)
    : SeatGlobal(q, display, &wl_seat_interface, &s_interface)
    , q_ptr(q)
{
}

const struct wl_seat_interface Seat::Private::s_interface = {
    cb<getPointerCallback>,
    cb<getKeyboardCallback>,
    cb<getTouchCallback>,
    resourceDestroyCallback,
};

Seat::Seat(Display* display, QObject* parent)
    : QObject(parent)
    , d_ptr(new Private(this, display))
{
    connect(this, &Seat::nameChanged, this, [this] { d_ptr->sendName(); });

    auto sendCapabilities = [this] { d_ptr->sendCapabilities(); };
    connect(this, &Seat::hasPointerChanged, this, sendCapabilities);
    connect(this, &Seat::hasKeyboardChanged, this, sendCapabilities);
    connect(this, &Seat::hasTouchChanged, this, sendCapabilities);

    d_ptr->create();
}

Seat::~Seat() = default;

void Seat::Private::bindInit(SeatBind* bind)
{
    send<wl_seat_send_capabilities>(bind, getCapabilities());
    send<wl_seat_send_name, WL_SEAT_NAME_SINCE_VERSION>(bind, name.c_str());
}

void Seat::Private::updatePointerButtonSerial(uint32_t button, uint32_t serial)
{
    auto it = globalPointer.buttonSerials.find(button);
    if (it == globalPointer.buttonSerials.end()) {
        globalPointer.buttonSerials.insert(button, serial);
        return;
    }
    it.value() = serial;
}

void Seat::Private::updatePointerButtonState(uint32_t button, SeatPointer::State state)
{
    auto it = globalPointer.buttonStates.find(button);
    if (it == globalPointer.buttonStates.end()) {
        globalPointer.buttonStates.insert(button, state);
        return;
    }
    it.value() = state;
}

bool Seat::Private::updateKey(uint32_t key, SeatKeyboard::State state)
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

void Seat::Private::sendCapabilities()
{
    send<wl_seat_send_capabilities>(getCapabilities());
}

template<typename T>
static T* interfaceForSurface(Surface* surface, const std::vector<T*>& interfaces)
{
    if (!surface) {
        return nullptr;
    }

    auto it = std::find_if(interfaces.begin(), interfaces.end(), [surface](auto const* c) {
        return surface->client() == c->client();
    });
    return it == interfaces.end() ? nullptr : *it;
}

template<typename Surface, typename T>
static std::vector<T*> interfacesForSurface(Surface const* surface,
                                            std::vector<T*> const& interfaces)
{
    std::vector<T*> ret;
    if (!surface) {
        return ret;
    }

    std::copy_if(interfaces.cbegin(),
                 interfaces.cend(),
                 std::back_inserter(ret),
                 [surface](T* device) { return device->client() == surface->client(); });
    return ret;
}

template<typename Surface, typename Vector, typename UnaryFunction>
static void forEachInterface(Surface* surface, Vector const& interfaces, UnaryFunction method)
{
    if (!surface) {
        return;
    }
    for (auto it : interfaces) {
        if (it->client() == surface->client()) {
            method(it);
        }
    }
}

template<typename V, typename T>
bool remove_one(V& container, T const& arg)
{
    auto it = std::find(container.begin(), container.end(), arg);
    if (it == container.end()) {
        return false;
    }
    container.erase(it);
    return true;
}

std::vector<Pointer*> Seat::Private::pointersForSurface(Surface* surface) const
{
    return interfacesForSurface(surface, pointers);
}

std::vector<Keyboard*> Seat::Private::keyboardsForSurface(Surface* surface) const
{
    return interfacesForSurface(surface, keyboards);
}

std::vector<Touch*> Seat::Private::touchsForSurface(Surface* surface) const
{
    return interfacesForSurface(surface, touchs);
}

std::vector<DataDevice*> Seat::Private::dataDevicesForSurface(Surface* surface) const
{
    return interfacesForSurface(surface, dataDevices);
}

TextInputV2* Seat::Private::textInputV2ForSurface(Surface* surface) const
{
    return interfaceForSurface(surface, textInputs);
}

text_input_v3* Seat::Private::textInputV3ForSurface(Surface* surface) const
{
    return interfaceForSurface(surface, textInputsV3);
}

void Seat::Private::cleanupDataDevice(DataDevice* dataDevice)
{
    remove_one(dataDevices, dataDevice);
    remove_one(keys.focus.selections, dataDevice);
    if (currentSelection == dataDevice) {
        // current selection is cleared
        currentSelection = nullptr;
        Q_EMIT q_ptr->selectionChanged(nullptr);
        for (auto selection : keys.focus.selections) {
            selection->sendClearSelection();
        }
    }
}

template<>
void Seat::Private::register_device(DataDevice* device)
{
    registerDataDevice(device);
}

void Seat::Private::registerDataDevice(DataDevice* dataDevice)
{
    dataDevices.push_back(dataDevice);

    QObject::connect(dataDevice, &DataDevice::resourceDestroyed, q_ptr, [this, dataDevice] {
        cleanupDataDevice(dataDevice);
    });

    QObject::connect(dataDevice, &DataDevice::selectionChanged, q_ptr, [this, dataDevice] {
        updateSelection(dataDevice, true);
    });
    QObject::connect(dataDevice, &DataDevice::selectionCleared, q_ptr, [this, dataDevice] {
        updateSelection(dataDevice, false);
    });

    QObject::connect(dataDevice, &DataDevice::dragStarted, q_ptr, [this, dataDevice] {
        const auto dragSerial = dataDevice->dragImplicitGrabSerial();
        auto* dragSurface = dataDevice->origin();
        if (q_ptr->hasImplicitPointerGrab(dragSerial)) {
            drag.mode = Drag::Mode::Pointer;
            drag.sourcePointer = interfaceForSurface(dragSurface, pointers);
            drag.transformation = globalPointer.focus.transformation;
        } else if (q_ptr->hasImplicitTouchGrab(dragSerial)) {
            drag.mode = Drag::Mode::Touch;
            drag.sourceTouch = interfaceForSurface(dragSurface, touchs);
            // TODO(unknown author): touch transformation
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
            // TODO(unknown author): transformation needs to be either pointer or touch
            drag.transformation = globalPointer.focus.transformation;
        }
        drag.source = dataDevice;
        drag.sourcePointer = interfaceForSurface(originSurface, pointers);
        drag.destroyConnection
            = QObject::connect(dataDevice, &DataDevice::resourceDestroyed, q_ptr, [this] {
                  endDrag(display()->handle()->nextSerial());
              });
        if (dataDevice->dragSource()) {
            drag.dragSourceDestroyConnection = QObject::connect(
                dataDevice->dragSource(), &DataSource::resourceDestroyed, q_ptr, [this] {
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
    });

    // Is the new DataDevice for the current keyoard focus?
    if (keys.focus.surface) {
        // Same client?
        if (keys.focus.surface->client() == dataDevice->client()) {
            keys.focus.selections.push_back(dataDevice);
            if (currentSelection && currentSelection->selection()) {
                dataDevice->sendSelection(currentSelection);
            }
        }
    }
}

void Seat::Private::cleanupPrimarySelectionDevice(PrimarySelectionDevice* primarySelectionDevice)
{
    remove_one(primarySelectionDevices, primarySelectionDevice);
    remove_one(keys.focus.primarySelections, primarySelectionDevice);
    if (currentPrimarySelectionDevice == primarySelectionDevice) {
        // current selection is cleared
        currentPrimarySelectionDevice = nullptr;
        Q_EMIT q_ptr->primarySelectionChanged(nullptr);
        for (auto selection : keys.focus.primarySelections) {
            selection->sendClearSelection();
        }
    }
}

template<>
void Seat::Private::register_device(PrimarySelectionDevice* device)
{
    registerPrimarySelectionDevice(device);
}

void Seat::Private::registerPrimarySelectionDevice(PrimarySelectionDevice* primarySelectionDevice)
{
    primarySelectionDevices.push_back(primarySelectionDevice);

    QObject::connect(
        primarySelectionDevice,
        &PrimarySelectionDevice::resourceDestroyed,
        q_ptr,
        [this, primarySelectionDevice] { cleanupPrimarySelectionDevice(primarySelectionDevice); });
    QObject::connect(
        primarySelectionDevice,
        &PrimarySelectionDevice::selectionChanged,
        q_ptr,
        [this, primarySelectionDevice] { updateSelection(primarySelectionDevice, true); });
    QObject::connect(
        primarySelectionDevice,
        &PrimarySelectionDevice::selectionCleared,
        q_ptr,
        [this, primarySelectionDevice] { updateSelection(primarySelectionDevice, false); });

    // Is the new PrimarySelectionDevice for the current keyoard on focus?
    if (keys.focus.surface) {
        // Same client?
        if (keys.focus.surface->client() == primarySelectionDevice->client()) {
            keys.focus.primarySelections.push_back(primarySelectionDevice);
            if (currentPrimarySelectionDevice && currentPrimarySelectionDevice->selection()) {
                primarySelectionDevice->sendSelection(currentPrimarySelectionDevice);
            }
        }
    }
}

void Seat::Private::cancelPreviousSelection(PrimarySelectionDevice* dataDevice) const
{
    if (!currentPrimarySelectionDevice) {
        return;
    }
    if (auto s = currentPrimarySelectionDevice->selection()) {
        if (currentPrimarySelectionDevice != dataDevice) {
            s->cancel();
        }
    }
}

void Seat::Private::updateSelection(PrimarySelectionDevice* dataDevice, bool set)
{
    bool selChanged = currentPrimarySelectionDevice != dataDevice;
    if (keys.focus.surface && (keys.focus.surface->client() == dataDevice->client())) {
        // cancel the previous selection
        cancelPreviousSelection(dataDevice);
        // new selection on a data device belonging to current keyboard focus
        currentPrimarySelectionDevice = dataDevice;
    }
    if (dataDevice == currentPrimarySelectionDevice) {
        // need to send out the selection
        for (auto focussedDevice : keys.focus.primarySelections) {
            if (set) {
                focussedDevice->sendSelection(dataDevice);
            } else {
                focussedDevice->sendClearSelection();
                currentPrimarySelectionDevice = nullptr;
                selChanged = true;
            }
        }
    }
    if (selChanged) {
        Q_EMIT q_ptr->primarySelectionChanged(currentPrimarySelectionDevice);
    }
}

void Seat::Private::registerInputMethod(input_method_v2* im)
{
    assert(!input_method);
    input_method = im;

    QObject::connect(im, &input_method_v2::resourceDestroyed, q_ptr, [this] {
        input_method = nullptr;
        Q_EMIT q_ptr->input_method_v2_changed();
    });
    Q_EMIT q_ptr->input_method_v2_changed();
}

void Seat::Private::registerTextInput(TextInputV2* ti)
{
    // Text input version 0 might call this multiple times.
    if (std::find(textInputs.begin(), textInputs.end(), ti) != textInputs.end()) {
        return;
    }
    textInputs.push_back(ti);
    if (global_text_input.focus.surface
        && global_text_input.focus.surface->client() == ti->d_ptr->client()->handle()) {
        // This is a text input for the currently focused text input surface.
        if (!global_text_input.v2.text_input) {
            global_text_input.v2.text_input = ti;
            ti->d_ptr->sendEnter(global_text_input.focus.surface, global_text_input.v2.serial);
            Q_EMIT q_ptr->focusedTextInputChanged();
        }
    }
    QObject::connect(ti, &TextInputV2::resourceDestroyed, q_ptr, [this, ti] {
        remove_one(textInputs, ti);
        if (global_text_input.v2.text_input == ti) {
            global_text_input.v2.text_input = nullptr;
            Q_EMIT q_ptr->focusedTextInputChanged();
        }
    });
}

void Seat::Private::registerTextInput(text_input_v3* ti)
{
    // Text input version 0 might call this multiple times.
    if (find(textInputsV3.begin(), textInputsV3.end(), ti) != textInputsV3.end()) {
        return;
    }
    textInputsV3.push_back(ti);
    if (global_text_input.focus.surface
        && global_text_input.focus.surface->client() == ti->d_ptr->client()->handle()) {
        // This is a text input for the currently focused text input surface.
        if (!global_text_input.v3.text_input) {
            global_text_input.v3.text_input = ti;
            ti->d_ptr->send_enter(global_text_input.focus.surface);
            Q_EMIT q_ptr->focusedTextInputChanged();
        }
    }
    QObject::connect(ti, &text_input_v3::resourceDestroyed, q_ptr, [this, ti] {
        remove_one(textInputsV3, ti);
        if (global_text_input.v3.text_input == ti) {
            global_text_input.v3.text_input = nullptr;
            Q_EMIT q_ptr->focusedTextInputChanged();
        }
    });
}

void Seat::Private::endDrag(uint32_t serial)
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
}

void Seat::Private::cancelPreviousSelection(DataDevice* dataDevice) const
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

void Seat::Private::updateSelection(DataDevice* dataDevice, bool set)
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
        for (auto focussedDevice : keys.focus.selections) {
            if (set) {
                focussedDevice->sendSelection(dataDevice);
            } else {
                focussedDevice->sendClearSelection();
                currentSelection = nullptr;
                selChanged = true;
            }
        }
    }
    if (selChanged) {
        Q_EMIT q_ptr->selectionChanged(currentSelection);
    }
}

void Seat::setHasKeyboard(bool has)
{
    if (d_ptr->keyboard == has) {
        return;
    }
    d_ptr->keyboard = has;
    Q_EMIT hasKeyboardChanged(d_ptr->keyboard);
}

void Seat::setHasPointer(bool has)
{
    if (d_ptr->pointer == has) {
        return;
    }
    d_ptr->pointer = has;
    Q_EMIT hasPointerChanged(d_ptr->pointer);
}

void Seat::setHasTouch(bool has)
{
    if (d_ptr->touch == has) {
        return;
    }
    d_ptr->touch = has;
    Q_EMIT hasTouchChanged(d_ptr->touch);
}

void Seat::setName(const std::string& name)
{
    if (d_ptr->name == name) {
        return;
    }
    d_ptr->name = name;
    Q_EMIT nameChanged(d_ptr->name);
}

void Seat::Private::getPointerCallback(SeatBind* bind, uint32_t id)
{
    auto priv = bind->global()->handle()->d_ptr.get();

    priv->getPointer(bind, id);
}

void Seat::Private::getPointer(SeatBind* bind, uint32_t id)
{
    auto client = bind->client()->handle();
    auto pointer = new Pointer(client, bind->version(), id, q_ptr);

    pointers.push_back(pointer);

    if (globalPointer.focus.surface && globalPointer.focus.surface->client() == client) {
        // this is a pointer for the currently focused pointer surface
        globalPointer.focus.pointers.push_back(pointer);
        pointer->setFocusedSurface(globalPointer.focus.serial, globalPointer.focus.surface);
        pointer->d_ptr->sendFrame();
        if (globalPointer.focus.pointers.size() == 1) {
            // got a new pointer
            Q_EMIT q_ptr->focusedPointerChanged(pointer);
        }
    }

    connect(pointer, &Pointer::resourceDestroyed, q_ptr, [pointer, this] {
        remove_one(pointers, pointer);
        if (remove_one(globalPointer.focus.pointers, pointer)) {
            if (globalPointer.focus.pointers.empty()) {
                Q_EMIT q_ptr->focusedPointerChanged(nullptr);
            }
        }
    });

    Q_EMIT q_ptr->pointerCreated(pointer);
}

void Seat::Private::getKeyboardCallback(SeatBind* bind, uint32_t id)
{
    auto priv = bind->global()->handle()->d_ptr.get();

    priv->getKeyboard(bind, id);
}

void Seat::Private::getKeyboard(SeatBind* bind, uint32_t id)
{
    auto client = bind->client()->handle();

    // TODO(unknown author): only create if seat has keyboard?
    auto keyboard = new Keyboard(client, bind->version(), id, q_ptr);

    keyboard->repeatInfo(keys.keyRepeat.charactersPerSecond, keys.keyRepeat.delay);

    if (keys.keymap.xkbcommonCompatible) {
        keyboard->setKeymap(keys.keymap.content);
    }

    keyboards.push_back(keyboard);

    if (keys.focus.surface && keys.focus.surface->client() == client) {
        // this is a keyboard for the currently focused keyboard surface
        keys.focus.keyboards.push_back(keyboard);
        keyboard->setFocusedSurface(keys.focus.serial, keys.focus.surface);
    }

    QObject::connect(keyboard, &Keyboard::resourceDestroyed, q_ptr, [keyboard, this] {
        remove_one(keyboards, keyboard);
        remove_one(keys.focus.keyboards, keyboard);
    });
    Q_EMIT q_ptr->keyboardCreated(keyboard);
}

void Seat::Private::getTouchCallback(SeatBind* bind, uint32_t id)
{
    auto priv = bind->global()->handle()->d_ptr.get();

    priv->getTouch(bind, id);
}

void Seat::Private::getTouch(SeatBind* bind, uint32_t id)
{
    auto client = bind->client()->handle();

    // TODO(unknown author): only create if seat has touch?
    auto touch = new Touch(client, bind->version(), id, q_ptr);
    // TODO(romangg): check for error

    touchs.push_back(touch);

    if (globalTouch.focus.surface && globalTouch.focus.surface->client() == client) {
        // this is a touch for the currently focused touch surface
        globalTouch.focus.touchs.push_back(touch);
        if (!globalTouch.ids.empty()) {
            // TODO(unknown author): send out all the points
        }
    }
    QObject::connect(touch, &Touch::resourceDestroyed, q_ptr, [touch, this] {
        remove_one(touchs, touch);
        remove_one(globalTouch.focus.touchs, touch);
    });
    Q_EMIT q_ptr->touchCreated(touch);
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
}

uint32_t Seat::timestamp() const
{
    return d_ptr->timestamp;
}

void Seat::setTimestamp(uint32_t time)
{
    if (d_ptr->timestamp == time) {
        return;
    }
    d_ptr->timestamp = time;
    Q_EMIT timestampChanged(time);
}

void Seat::setDragTarget(Surface* surface,
                         const QPointF& globalPosition,
                         const QMatrix4x4& inputTransformation)
{
    if (surface == d_ptr->drag.surface) {
        // no change
        return;
    }
    auto const serial = d_ptr->display()->handle()->nextSerial();
    if (d_ptr->drag.target) {
        d_ptr->drag.target->updateDragTarget(nullptr, serial);
        QObject::disconnect(d_ptr->drag.target_destroy_connection);
        d_ptr->drag.target_destroy_connection = QMetaObject::Connection();
    }

    // In theory we can have multiple data devices and we should send the drag to all of them, but
    // that seems overly complicated. In practice so far the only case for multiple data devices is
    // for clipboard overriding.
    d_ptr->drag.target = nullptr;
    if (!d_ptr->dataDevicesForSurface(surface).empty()) {
        d_ptr->drag.target = d_ptr->dataDevicesForSurface(surface).front();
    }

    if (d_ptr->drag.mode == Private::Drag::Mode::Pointer) {
        setPointerPos(globalPosition);
    } else if (d_ptr->drag.mode == Private::Drag::Mode::Touch
               && d_ptr->globalTouch.focus.firstTouchPos != globalPosition) {
        touchMove(d_ptr->globalTouch.ids.firstKey(), globalPosition);
    }
    if (d_ptr->drag.target) {
        d_ptr->drag.surface = surface;
        d_ptr->drag.transformation = inputTransformation;
        d_ptr->drag.target->updateDragTarget(surface, serial);
        d_ptr->drag.target_destroy_connection
            = QObject::connect(d_ptr->drag.target, &DataDevice::resourceDestroyed, this, [this] {
                  QObject::disconnect(d_ptr->drag.target_destroy_connection);
                  d_ptr->drag.target_destroy_connection = QMetaObject::Connection();
                  d_ptr->drag.target = nullptr;
              });

    } else {
        d_ptr->drag.surface = nullptr;
    }
    Q_EMIT dragSurfaceChanged();
}

void Seat::setDragTarget(Surface* surface, const QMatrix4x4& inputTransformation)
{
    if (d_ptr->drag.mode == Private::Drag::Mode::Pointer) {
        setDragTarget(surface, pointerPos(), inputTransformation);
    } else {
        Q_ASSERT(d_ptr->drag.mode == Private::Drag::Mode::Touch);
        setDragTarget(surface, d_ptr->globalTouch.focus.firstTouchPos, inputTransformation);
    }
}

Surface* Seat::focusedPointerSurface() const
{
    return d_ptr->globalPointer.focus.surface;
}

void Seat::setFocusedPointerSurface(Surface* surface, const QPointF& surfacePosition)
{
    QMatrix4x4 m;
    m.translate(-static_cast<float>(surfacePosition.x()), -static_cast<float>(surfacePosition.y()));
    setFocusedPointerSurface(surface, m);

    if (d_ptr->globalPointer.focus.surface) {
        d_ptr->globalPointer.focus.offset = surfacePosition;
    }
}

void Seat::setFocusedPointerSurface(Surface* surface, const QMatrix4x4& transformation)
{
    if (d_ptr->drag.mode == Private::Drag::Mode::Pointer) {
        // ignore
        return;
    }

    auto const serial = d_ptr->display()->handle()->nextSerial();
    std::unordered_set<Pointer*> framePointers;

    for (auto pointer : d_ptr->globalPointer.focus.pointers) {
        pointer->setFocusedSurface(serial, nullptr);
        framePointers.insert(pointer);
    }

    if (d_ptr->globalPointer.focus.surface) {
        disconnect(d_ptr->globalPointer.focus.destroyConnection);
    }

    d_ptr->globalPointer.focus = Private::SeatPointer::Focus();
    d_ptr->globalPointer.focus.surface = surface;

    d_ptr->globalPointer.focus.pointers.clear();

    if (surface) {
        d_ptr->globalPointer.focus.pointers = d_ptr->pointersForSurface(surface);

        d_ptr->globalPointer.focus.destroyConnection
            = connect(surface, &Surface::resourceDestroyed, this, [this] {
                  d_ptr->globalPointer.focus = Private::SeatPointer::Focus();
                  Q_EMIT focusedPointerChanged(nullptr);
              });
        d_ptr->globalPointer.focus.offset = QPointF();
        d_ptr->globalPointer.focus.transformation = transformation;
        d_ptr->globalPointer.focus.serial = serial;
    }

    auto& pointers = d_ptr->globalPointer.focus.pointers;
    if (pointers.empty()) {
        Q_EMIT focusedPointerChanged(nullptr);
        for (auto p : framePointers) {
            p->d_ptr->sendFrame();
        }
        return;
    }

    // TODO(unknown author): signal with all pointers
    Q_EMIT focusedPointerChanged(pointers.front());

    for (auto p : pointers) {
        p->setFocusedSurface(serial, surface);
        framePointers.insert(p);
    }

    for (auto p : framePointers) {
        p->d_ptr->sendFrame();
    }
}

Pointer* Seat::focusedPointer() const
{
    if (d_ptr->globalPointer.focus.pointers.empty()) {
        return nullptr;
    }
    return d_ptr->globalPointer.focus.pointers.front();
}

void Seat::setFocusedPointerSurfacePosition(const QPointF& surfacePosition)
{
    if (d_ptr->globalPointer.focus.surface) {
        d_ptr->globalPointer.focus.offset = surfacePosition;
        d_ptr->globalPointer.focus.transformation = QMatrix4x4();
        d_ptr->globalPointer.focus.transformation.translate(
            -static_cast<float>(surfacePosition.x()), -static_cast<float>(surfacePosition.y()));
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
uint32_t qtToWaylandButton(Qt::MouseButton button)
{
#if HAVE_LINUX_INPUT_H
    static const QHash<Qt::MouseButton, uint32_t> s_buttons({
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

bool Seat::isPointerButtonPressed(uint32_t button) const
{
    auto it = d_ptr->globalPointer.buttonStates.constFind(button);
    if (it == d_ptr->globalPointer.buttonStates.constEnd()) {
        return false;
    }
    return it.value() == Private::SeatPointer::State::Pressed;
}

void Seat::pointerAxisV5(Qt::Orientation orientation,
                         qreal delta,
                         int32_t discreteDelta,
                         PointerAxisSource source)
{
    if (d_ptr->drag.mode == Private::Drag::Mode::Pointer) {
        // ignore
        return;
    }
    if (d_ptr->globalPointer.focus.surface) {
        for (auto pointer : d_ptr->globalPointer.focus.pointers) {
            pointer->axis(orientation, delta, discreteDelta, source);
        }
    }
}

void Seat::pointerAxis(Qt::Orientation orientation, uint32_t delta)
{
    if (d_ptr->drag.mode == Private::Drag::Mode::Pointer) {
        // ignore
        return;
    }
    if (d_ptr->globalPointer.focus.surface) {
        for (auto pointer : d_ptr->globalPointer.focus.pointers) {
            pointer->axis(orientation, delta);
        }
    }
}

void Seat::pointerButtonPressed(Qt::MouseButton button)
{
    auto const nativeButton = qtToWaylandButton(button);
    if (nativeButton == 0) {
        return;
    }
    pointerButtonPressed(nativeButton);
}

void Seat::pointerButtonPressed(uint32_t button)
{
    auto const serial = d_ptr->display()->handle()->nextSerial();
    d_ptr->updatePointerButtonSerial(button, serial);
    d_ptr->updatePointerButtonState(button, Private::SeatPointer::State::Pressed);
    if (d_ptr->drag.mode == Private::Drag::Mode::Pointer) {
        // ignore
        return;
    }
    if (d_ptr->globalPointer.focus.surface) {
        for (auto pointer : d_ptr->globalPointer.focus.pointers) {
            pointer->buttonPressed(serial, button);
        }
    }
}

void Seat::pointerButtonReleased(Qt::MouseButton button)
{
    auto const nativeButton = qtToWaylandButton(button);
    if (nativeButton == 0) {
        return;
    }
    pointerButtonReleased(nativeButton);
}

void Seat::pointerButtonReleased(uint32_t button)
{
    auto const serial = d_ptr->display()->handle()->nextSerial();
    auto const currentButtonSerial = pointerButtonSerial(button);
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
        for (auto pointer : d_ptr->globalPointer.focus.pointers) {
            pointer->buttonReleased(serial, button);
        }
    }
}

uint32_t Seat::pointerButtonSerial(Qt::MouseButton button) const
{
    return pointerButtonSerial(qtToWaylandButton(button));
}

uint32_t Seat::pointerButtonSerial(uint32_t button) const
{
    auto it = d_ptr->globalPointer.buttonSerials.constFind(button);
    if (it == d_ptr->globalPointer.buttonSerials.constEnd()) {
        return 0;
    }
    return it.value();
}

void Seat::relativePointerMotion(const QSizeF& delta,
                                 const QSizeF& deltaNonAccelerated,
                                 uint64_t microseconds)
{
    if (d_ptr->globalPointer.focus.surface) {
        for (auto pointer : d_ptr->globalPointer.focus.pointers) {
            pointer->relativeMotion(delta, deltaNonAccelerated, microseconds);
        }
    }
}

void Seat::startPointerSwipeGesture(uint32_t fingerCount)
{
    if (!d_ptr->globalPointer.gestureSurface.isNull()) {
        return;
    }
    d_ptr->globalPointer.gestureSurface = QPointer<Surface>(d_ptr->globalPointer.focus.surface);
    if (d_ptr->globalPointer.gestureSurface.isNull()) {
        return;
    }
    auto const serial = d_ptr->display()->handle()->nextSerial();
    forEachInterface(
        d_ptr->globalPointer.gestureSurface.data(),
        d_ptr->pointers,
        [serial, fingerCount](Pointer* p) { p->d_ptr->startSwipeGesture(serial, fingerCount); });
}

void Seat::updatePointerSwipeGesture(const QSizeF& delta)
{
    if (d_ptr->globalPointer.gestureSurface.isNull()) {
        return;
    }
    forEachInterface(d_ptr->globalPointer.gestureSurface.data(),
                     d_ptr->pointers,
                     [delta](Pointer* p) { p->d_ptr->updateSwipeGesture(delta); });
}

void Seat::endPointerSwipeGesture()
{
    if (d_ptr->globalPointer.gestureSurface.isNull()) {
        return;
    }
    auto const serial = d_ptr->display()->handle()->nextSerial();
    forEachInterface(d_ptr->globalPointer.gestureSurface.data(),
                     d_ptr->pointers,
                     [serial](Pointer* p) { p->d_ptr->endSwipeGesture(serial); });
    d_ptr->globalPointer.gestureSurface.clear();
}

void Seat::cancelPointerSwipeGesture()
{
    if (d_ptr->globalPointer.gestureSurface.isNull()) {
        return;
    }
    auto const serial = d_ptr->display()->handle()->nextSerial();
    forEachInterface(d_ptr->globalPointer.gestureSurface.data(),
                     d_ptr->pointers,
                     [serial](Pointer* p) { p->d_ptr->cancelSwipeGesture(serial); });
    d_ptr->globalPointer.gestureSurface.clear();
}

void Seat::startPointerPinchGesture(uint32_t fingerCount)
{
    if (!d_ptr->globalPointer.gestureSurface.isNull()) {
        return;
    }
    d_ptr->globalPointer.gestureSurface = QPointer<Surface>(d_ptr->globalPointer.focus.surface);
    if (d_ptr->globalPointer.gestureSurface.isNull()) {
        return;
    }
    auto const serial = d_ptr->display()->handle()->nextSerial();
    forEachInterface(
        d_ptr->globalPointer.gestureSurface.data(),
        d_ptr->pointers,
        [serial, fingerCount](Pointer* p) { p->d_ptr->startPinchGesture(serial, fingerCount); });
}

void Seat::updatePointerPinchGesture(const QSizeF& delta, qreal scale, qreal rotation)
{
    if (d_ptr->globalPointer.gestureSurface.isNull()) {
        return;
    }
    forEachInterface(d_ptr->globalPointer.gestureSurface.data(),
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
    auto const serial = d_ptr->display()->handle()->nextSerial();
    forEachInterface(d_ptr->globalPointer.gestureSurface.data(),
                     d_ptr->pointers,
                     [serial](Pointer* p) { p->d_ptr->endPinchGesture(serial); });
    d_ptr->globalPointer.gestureSurface.clear();
}

void Seat::cancelPointerPinchGesture()
{
    if (d_ptr->globalPointer.gestureSurface.isNull()) {
        return;
    }
    auto const serial = d_ptr->display()->handle()->nextSerial();
    forEachInterface(d_ptr->globalPointer.gestureSurface.data(),
                     d_ptr->pointers,
                     [serial](Pointer* p) { p->d_ptr->cancelPinchGesture(serial); });
    d_ptr->globalPointer.gestureSurface.clear();
}

void Seat::keyPressed(uint32_t key)
{
    d_ptr->keys.lastStateSerial = d_ptr->display()->handle()->nextSerial();
    if (!d_ptr->updateKey(key, Private::SeatKeyboard::State::Pressed)) {
        return;
    }
    if (d_ptr->keys.focus.surface) {
        for (auto kbd : d_ptr->keys.focus.keyboards) {
            kbd->keyPressed(d_ptr->keys.lastStateSerial, key);
        }
    }
}

void Seat::keyReleased(uint32_t key)
{
    d_ptr->keys.lastStateSerial = d_ptr->display()->handle()->nextSerial();
    if (!d_ptr->updateKey(key, Private::SeatKeyboard::State::Released)) {
        return;
    }
    if (d_ptr->keys.focus.surface) {
        for (auto kbd : d_ptr->keys.focus.keyboards) {
            kbd->keyReleased(d_ptr->keys.lastStateSerial, key);
        }
    }
}

Surface* Seat::focusedKeyboardSurface() const
{
    return d_ptr->keys.focus.surface;
}

void Seat::setFocusedKeyboardSurface(Surface* surface)
{
    auto const serial = d_ptr->display()->handle()->nextSerial();

    for (auto kbd : d_ptr->keys.focus.keyboards) {
        kbd->setFocusedSurface(serial, nullptr);
    }

    if (d_ptr->keys.focus.surface) {
        disconnect(d_ptr->keys.focus.destroyConnection);
    }

    d_ptr->keys.focus = Private::SeatKeyboard::Focus();
    d_ptr->keys.focus.surface = surface;
    d_ptr->keys.focus.keyboards = d_ptr->keyboardsForSurface(surface);

    if (d_ptr->keys.focus.surface) {
        d_ptr->keys.focus.destroyConnection
            = connect(surface, &Surface::resourceDestroyed, this, [this] {
                  d_ptr->keys.focus = Private::SeatKeyboard::Focus();
              });
        d_ptr->keys.focus.serial = serial;

        auto dataDevices = d_ptr->dataDevicesForSurface(surface);
        d_ptr->keys.focus.selections = dataDevices;
        for (auto dataDevice : dataDevices) {
            if (d_ptr->currentSelection) {
                dataDevice->sendSelection(d_ptr->currentSelection);
            } else {
                dataDevice->sendClearSelection();
            }
        }
        auto const primarySelectionDevices
            = interfacesForSurface(surface, d_ptr->primarySelectionDevices);
        d_ptr->keys.focus.primarySelections = primarySelectionDevices;
        for (auto dataDevice : primarySelectionDevices) {
            if (d_ptr->currentPrimarySelectionDevice) {
                dataDevice->sendSelection(d_ptr->currentPrimarySelectionDevice);
            } else {
                dataDevice->sendClearSelection();
            }
        }
    }

    for (auto kbd : d_ptr->keys.focus.keyboards) {
        kbd->setFocusedSurface(serial, surface);
    }

    // Focused text input surface follows keyboard.
    if (hasKeyboard()) {
        setFocusedTextInputSurface(surface);
    }
}

void Seat::setKeymap(std::string const& content)
{
    d_ptr->keys.keymap.xkbcommonCompatible = true;
    d_ptr->keys.keymap.content = content;
    for (auto kbd : d_ptr->keyboards) {
        kbd->setKeymap(content);
    }
}

void Seat::updateKeyboardModifiers(uint32_t depressed,
                                   uint32_t latched,
                                   uint32_t locked,
                                   uint32_t group)
{
    bool changed = false;

    auto& saved_mods = d_ptr->keys.modifiers;
    auto mods
        = Private::SeatKeyboard::Modifiers{depressed, latched, locked, group, saved_mods.serial};
    if (saved_mods != mods) {
        saved_mods = mods;
        changed = true;
    }

    if (!changed) {
        return;
    }

    auto const serial = d_ptr->display()->handle()->nextSerial();
    saved_mods.serial = serial;

    if (d_ptr->keys.focus.surface) {
        for (auto kbd : d_ptr->keys.focus.keyboards) {
            kbd->updateModifiers(serial, depressed, latched, locked, group);
        }
    }
}

void Seat::setKeyRepeatInfo(int32_t charactersPerSecond, int32_t delay)
{
    d_ptr->keys.keyRepeat.charactersPerSecond = qMax(charactersPerSecond, 0);
    d_ptr->keys.keyRepeat.delay = qMax(delay, 0);
    for (auto kbd : d_ptr->keyboards) {
        kbd->repeatInfo(d_ptr->keys.keyRepeat.charactersPerSecond, d_ptr->keys.keyRepeat.delay);
    }
}

int32_t Seat::keyRepeatDelay() const
{
    return d_ptr->keys.keyRepeat.delay;
}

int32_t Seat::keyRepeatRate() const
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

uint32_t Seat::keymapSize() const
{
    return d_ptr->keys.keymap.content.size();
}

uint32_t Seat::depressedModifiers() const
{
    return d_ptr->keys.modifiers.depressed;
}

uint32_t Seat::groupModifiers() const
{
    return d_ptr->keys.modifiers.group;
}

uint32_t Seat::latchedModifiers() const
{
    return d_ptr->keys.modifiers.latched;
}

uint32_t Seat::lockedModifiers() const
{
    return d_ptr->keys.modifiers.locked;
}

uint32_t Seat::lastModifiersSerial() const
{
    return d_ptr->keys.modifiers.serial;
}

std::vector<uint32_t> Seat::pressedKeys() const
{
    std::vector<uint32_t> keys;
    for (auto it = d_ptr->keys.states.constBegin(); it != d_ptr->keys.states.constEnd(); ++it) {
        if (it.value() == Private::SeatKeyboard::State::Pressed) {
            keys.push_back(it.key());
        }
    }
    return keys;
}

Keyboard* Seat::focusedKeyboard() const
{
    if (d_ptr->keys.focus.keyboards.empty()) {
        return nullptr;
    }
    return d_ptr->keys.focus.keyboards.front();
}

void Seat::cancelTouchSequence()
{
    for (auto touch : d_ptr->globalTouch.focus.touchs) {
        touch->cancel();
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

Touch* Seat::focusedTouch() const
{
    if (d_ptr->globalTouch.focus.touchs.empty()) {
        return nullptr;
    }
    return d_ptr->globalTouch.focus.touchs.front();
}

Surface* Seat::focusedTouchSurface() const
{
    return d_ptr->globalTouch.focus.surface;
}

QPointF Seat::focusedTouchSurfacePosition() const
{
    return d_ptr->globalTouch.focus.offset;
}

bool Seat::isTouchSequence() const
{
    return !d_ptr->globalTouch.ids.empty();
}

void Seat::setFocusedTouchSurface(Surface* surface, const QPointF& surfacePosition)
{
    if (isTouchSequence()) {
        // changing surface not allowed during a touch sequence
        return;
    }
    Q_ASSERT(!isDragTouch());

    if (d_ptr->globalTouch.focus.surface) {
        disconnect(d_ptr->globalTouch.focus.destroyConnection);
    }
    d_ptr->globalTouch.focus = Private::SeatTouch::Focus();
    d_ptr->globalTouch.focus.surface = surface;
    d_ptr->globalTouch.focus.offset = surfacePosition;
    d_ptr->globalTouch.focus.touchs = d_ptr->touchsForSurface(surface);
    if (d_ptr->globalTouch.focus.surface) {
        d_ptr->globalTouch.focus.destroyConnection
            = connect(surface, &Surface::resourceDestroyed, this, [this] {
                  if (isTouchSequence()) {
                      // Surface destroyed during touch sequence - send a cancel
                      for (auto touch : d_ptr->globalTouch.focus.touchs) {
                          touch->cancel();
                      }
                  }
                  d_ptr->globalTouch.focus = Private::SeatTouch::Focus();
              });
    }
}

void Seat::setFocusedTouchSurfacePosition(const QPointF& surfacePosition)
{
    d_ptr->globalTouch.focus.offset = surfacePosition;
}

int32_t Seat::touchDown(const QPointF& globalPosition)
{
    auto const id = d_ptr->globalTouch.ids.empty() ? 0 : d_ptr->globalTouch.ids.lastKey() + 1;
    auto const serial = d_ptr->display()->handle()->nextSerial();
    const auto pos = globalPosition - d_ptr->globalTouch.focus.offset;
    for (auto touch : d_ptr->globalTouch.focus.touchs) {
        touch->down(id, serial, pos);
    }

    if (id == 0) {
        d_ptr->globalTouch.focus.firstTouchPos = globalPosition;
    }

#if HAVE_LINUX_INPUT_H
    if (id == 0 && d_ptr->globalTouch.focus.touchs.empty()) {
        // If the client did not bind the touch interface fall back
        // to at least emulating touch through pointer events.
        forEachInterface(focusedTouchSurface(), d_ptr->pointers, [this, pos, serial](Pointer* p) {
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

void Seat::touchMove(int32_t id, const QPointF& globalPosition)
{
    Q_ASSERT(d_ptr->globalTouch.ids.contains(id));
    const auto pos = globalPosition - d_ptr->globalTouch.focus.offset;
    for (auto touch : d_ptr->globalTouch.focus.touchs) {
        touch->move(id, pos);
    }

    if (id == 0) {
        d_ptr->globalTouch.focus.firstTouchPos = globalPosition;
    }

    if (id == 0 && d_ptr->globalTouch.focus.touchs.empty()) {
        // Client did not bind touch, fall back to emulating with pointer events.
        forEachInterface(focusedTouchSurface(), d_ptr->pointers, [pos](Pointer* p) {
            p->d_ptr->sendMotion(pos);
        });
    }
    Q_EMIT touchMoved(id, d_ptr->globalTouch.ids[id], globalPosition);
}

void Seat::touchUp(int32_t id)
{
    Q_ASSERT(d_ptr->globalTouch.ids.contains(id));
    auto const serial = d_ptr->display()->handle()->nextSerial();
    if (d_ptr->drag.mode == Private::Drag::Mode::Touch
        && d_ptr->drag.source->dragImplicitGrabSerial() == d_ptr->globalTouch.ids.value(id)) {
        // the implicitly grabbing touch point has been upped
        d_ptr->endDrag(serial);
    }
    for (auto touch : d_ptr->globalTouch.focus.touchs) {
        touch->up(id, serial);
    }

#if HAVE_LINUX_INPUT_H
    if (id == 0 && d_ptr->globalTouch.focus.touchs.empty()) {
        // Client did not bind touch, fall back to emulating with pointer events.
        auto const serial = d_ptr->display()->handle()->nextSerial();
        forEachInterface(focusedTouchSurface(), d_ptr->pointers, [serial](Pointer* p) {
            p->buttonReleased(serial, BTN_LEFT);
        });
    }
#endif

    d_ptr->globalTouch.ids.remove(id);
}

void Seat::touchFrame()
{
    for (auto touch : d_ptr->globalTouch.focus.touchs) {
        touch->frame();
    }
}

bool Seat::hasImplicitTouchGrab(uint32_t serial) const
{
    if (!d_ptr->globalTouch.focus.surface) {
        // origin surface has been destroyed
        return false;
    }
    return d_ptr->globalTouch.ids.key(serial, -1) != -1;
}

input_method_v2* Seat::get_input_method_v2() const
{
    return d_ptr->input_method;
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

bool Seat::hasImplicitPointerGrab(uint32_t serial) const
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

Surface* Seat::dragSurface() const
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

DataDevice* Seat::dragSource() const
{
    return d_ptr->drag.source;
}

bool Seat::setFocusedTextInputV2Surface(Surface* surface)
{
    auto const serial = d_ptr->display()->handle()->nextSerial();
    auto const old_ti = d_ptr->global_text_input.v2.text_input;

    if (old_ti) {
        // TODO(unknown author): setFocusedSurface like in other interfaces
        old_ti->d_ptr->sendLeave(serial, d_ptr->global_text_input.focus.surface);
    }

    auto ti = d_ptr->textInputV2ForSurface(surface);

    if (ti && !ti->d_ptr->resource()) {
        // TODO(romangg): can this check be removed?
        ti = nullptr;
    }

    d_ptr->global_text_input.v2.text_input = ti;

    if (surface) {
        d_ptr->global_text_input.v2.serial = serial;
    }

    if (ti) {
        // TODO(unknown author): setFocusedSurface like in other interfaces
        ti->d_ptr->sendEnter(surface, serial);
    }

    return old_ti != ti;
}

bool Seat::setFocusedTextInputV3Surface(Surface* surface)
{
    auto const old_ti = d_ptr->global_text_input.v3.text_input;

    if (old_ti) {
        old_ti->d_ptr->send_leave(d_ptr->global_text_input.focus.surface);
    }

    auto ti = d_ptr->textInputV3ForSurface(surface);

    if (ti && !ti->d_ptr->resource()) {
        // TODO(romangg): can this check be removed?
        ti = nullptr;
    }

    d_ptr->global_text_input.v3.text_input = ti;

    if (ti) {
        ti->d_ptr->send_enter(surface);
    }

    return old_ti != ti;
}

void Seat::setFocusedTextInputSurface(Surface* surface)
{
    if (d_ptr->global_text_input.focus.surface) {
        disconnect(d_ptr->global_text_input.focus.destroy_connection);
    }

    auto changed = setFocusedTextInputV2Surface(surface) || setFocusedTextInputV3Surface(surface);
    d_ptr->global_text_input.focus = {};

    if (surface) {
        d_ptr->global_text_input.focus.surface = surface;
        d_ptr->global_text_input.focus.destroy_connection
            = connect(surface, &Surface::resourceDestroyed, this, [this] {
                  setFocusedTextInputSurface(nullptr);
              });
    }

    if (changed) {
        Q_EMIT focusedTextInputChanged();
    }
}

Surface* Seat::focusedTextInputSurface() const
{
    return d_ptr->global_text_input.focus.surface;
}

TextInputV2* Seat::focusedTextInputV2() const
{
    return d_ptr->global_text_input.v2.text_input;
}

text_input_v3* Seat::focusedTextInputV3() const
{
    return d_ptr->global_text_input.v3.text_input;
}

DataDevice* Seat::selection() const
{
    return d_ptr->currentSelection;
}

void Seat::setSelection(DataDevice* dataDevice)
{
    if (d_ptr->currentSelection == dataDevice) {
        return;
    }
    // cancel the previous selection
    d_ptr->cancelPreviousSelection(dataDevice);
    d_ptr->currentSelection = dataDevice;

    for (auto focusedDevice : d_ptr->keys.focus.selections) {
        if (dataDevice && dataDevice->selection()) {
            focusedDevice->sendSelection(dataDevice);
        } else {
            focusedDevice->sendClearSelection();
        }
    }
    Q_EMIT selectionChanged(dataDevice);
}

PrimarySelectionDevice* Seat::primarySelection() const
{
    return d_ptr->currentPrimarySelectionDevice;
}

void Seat::setPrimarySelection(PrimarySelectionDevice* dataDevice)
{
    if (d_ptr->currentPrimarySelectionDevice == dataDevice) {
        return;
    }
    // cancel the previous selection
    d_ptr->cancelPreviousSelection(dataDevice);
    d_ptr->currentPrimarySelectionDevice = dataDevice;

    for (auto focusedDevice : d_ptr->keys.focus.primarySelections) {
        if (dataDevice && dataDevice->selection()) {
            focusedDevice->sendSelection(dataDevice);
        } else {
            focusedDevice->sendClearSelection();
        }
    }
    Q_EMIT primarySelectionChanged(dataDevice);
}

}
