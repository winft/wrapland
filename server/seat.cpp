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

#include "abstract_data_source.h"
#include "client.h"
#include "data_device.h"
#include "data_source.h"
#include "display.h"
#include "keyboard.h"
#include "keyboard_p.h"
#include "pointer.h"
#include "pointer_p.h"
#include "surface.h"
#include "text_input_v2_p.h"
#include "touch.h"

#include <config-wrapland.h>

#ifndef WL_SEAT_NAME_SINCE_VERSION
#define WL_SEAT_NAME_SINCE_VERSION 2
#endif

#if HAVE_LINUX_INPUT_H
#include <linux/input.h>
#endif

#include <functional>

namespace Wrapland::Server
{

Seat::Private::Private(Seat* q, Display* display)
    : SeatGlobal(q, display, &wl_seat_interface, &s_interface)
    , q_ptr(q)
{
}

const struct wl_seat_interface Seat::Private::s_interface = {
    getPointerCallback,
    getKeyboardCallback,
    getTouchCallback,
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

void Seat::Private::bindInit(Wayland::Resource<Seat, SeatGlobal>* bind)
{
    send<wl_seat_send_capabilities>(bind, getCapabilities());
    send<wl_seat_send_name, WL_SEAT_NAME_SINCE_VERSION>(bind, name.c_str());
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

namespace
{

template<typename T>
static T* interfaceForSurface(Surface* surface, const QVector<T*>& interfaces)
{
    if (!surface) {
        return nullptr;
    }

    for (auto it = interfaces.constBegin(); it != interfaces.constEnd(); ++it) {
        if ((*it)->client() == surface->client()) {
            return (*it);
        }
    }
    return nullptr;
}

template<typename T>
static QVector<T*> interfacesForSurface(Surface* surface, const QVector<T*>& interfaces)
{
    QVector<T*> ret;
    if (!surface) {
        return ret;
    }

    for (auto it = interfaces.constBegin(); it != interfaces.constEnd(); ++it) {
        if ((*it)->client() == surface->client()) {
            ret << *it;
        }
    }
    return ret;
}

template<typename T>
static bool
forEachInterface(Surface* surface, const QVector<T*>& interfaces, std::function<void(T*)> method)
{
    if (!surface) {
        return false;
    }
    bool calledAtLeastOne = false;
    for (auto it = interfaces.constBegin(); it != interfaces.constEnd(); ++it) {
        if ((*it)->client() == surface->client()) {
            method(*it);
            calledAtLeastOne = true;
        }
    }
    return calledAtLeastOne;
}

}

QVector<Pointer*> Seat::Private::pointersForSurface(Surface* surface) const
{
    return interfacesForSurface(surface, pointers);
}

QVector<Keyboard*> Seat::Private::keyboardsForSurface(Surface* surface) const
{
    return interfacesForSurface(surface, keyboards);
}

QVector<Touch*> Seat::Private::touchsForSurface(Surface* surface) const
{
    return interfacesForSurface(surface, touchs);
}

QVector<DataDevice*> Seat::Private::dataDevicesForSurface(Surface* surface) const
{
    return interfacesForSurface(surface, dataDevices);
}

TextInputV2* Seat::Private::textInputForSurface(Surface* surface) const
{
    return interfaceForSurface(surface, textInputs);
}

void Seat::Private::registerDataDevice(DataDevice* dataDevice)
{
    dataDevices << dataDevice;
    auto dataDeviceCleanup = [this, dataDevice] {
        dataDevices.removeOne(dataDevice);
        keys.focus.selections.removeOne(dataDevice);
    };

    QObject::connect(dataDevice, &DataDevice::resourceDestroyed, q_ptr, dataDeviceCleanup);
    QObject::connect(dataDevice, &DataDevice::selectionChanged, q_ptr, [this, dataDevice] {
        updateSelection(dataDevice);
    });
    QObject::connect(dataDevice, &DataDevice::selectionCleared, q_ptr, [this, dataDevice] {
        updateSelection(dataDevice);
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
            keys.focus.selections.append(dataDevice);
            if (currentSelection) {
                dataDevice->sendSelection(currentSelection);
            }
        }
    }
}

void Seat::Private::registerTextInput(TextInputV2* ti)
{
    // Text input version 0 might call this multiple times.
    if (textInputs.contains(ti)) {
        return;
    }
    textInputs << ti;
    if (textInput.focus.surface
        && textInput.focus.surface->client() == ti->d_ptr->client()->handle()) {
        // This is a text input for the currently focused text input surface.
        if (!textInput.focus.textInput) {
            textInput.focus.textInput = ti;
            ti->d_ptr->sendEnter(textInput.focus.surface, textInput.focus.serial);
            Q_EMIT q_ptr->focusedTextInputChanged();
        }
    }
    QObject::connect(ti, &TextInputV2::resourceDestroyed, q_ptr, [this, ti] {
        textInputs.removeAt(textInputs.indexOf(ti));
        if (textInput.focus.textInput == ti) {
            textInput.focus.textInput = nullptr;
            Q_EMIT q_ptr->focusedTextInputChanged();
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
}

void Seat::Private::updateSelection(DataDevice* dataDevice) const
{
    // if the update is from the focussed window we should inform the active client
    if (!(keys.focus.surface && (keys.focus.surface->client() == dataDevice->client()))) {
        return;
    }

    q_ptr->setSelection(dataDevice->selection());
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

void Seat::Private::getPointerCallback([[maybe_unused]] wl_client* wlClient,
                                       wl_resource* wlResource,
                                       uint32_t id)
{
    auto priv = handle(wlResource)->d_ptr.get();
    auto bind = priv->getBind(wlResource);

    priv->getPointer(bind, id);
}

void Seat::Private::getPointer(SeatBind* bind, uint32_t id)
{
    auto client = bind->client()->handle();
    auto pointer = new Pointer(client, bind->version(), id, q_ptr);

    pointers << pointer;

    if (globalPointer.focus.surface && globalPointer.focus.surface->client() == client) {
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

void Seat::Private::getKeyboardCallback([[maybe_unused]] wl_client* wlClient,
                                        wl_resource* wlResource,
                                        uint32_t id)
{
    auto priv = handle(wlResource)->d_ptr.get();
    auto bind = priv->getBind(wlResource);

    priv->getKeyboard(bind, id);
}

void Seat::Private::getKeyboard(SeatBind* bind, uint32_t id)
{
    auto client = bind->client()->handle();

    // TODO(unknown author): only create if seat has keyboard?
    auto keyboard = new Keyboard(client, bind->version(), id, q_ptr);

    keyboard->repeatInfo(keys.keyRepeat.charactersPerSecond, keys.keyRepeat.delay);

    if (keys.keymap.xkbcommonCompatible) {
        keyboard->setKeymap(keys.keymap.fd, keys.keymap.size);
    }

    keyboards << keyboard;

    if (keys.focus.surface && keys.focus.surface->client() == client) {
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

void Seat::Private::getTouchCallback([[maybe_unused]] wl_client* wlClient,
                                     wl_resource* wlResource,
                                     uint32_t id)
{
    auto priv = handle(wlResource)->d_ptr.get();
    auto bind = priv->getBind(wlResource);

    priv->getTouch(bind, id);
}

void Seat::Private::getTouch(SeatBind* bind, uint32_t id)
{
    auto client = bind->client()->handle();

    // TODO(unknown author): only create if seat has touch?
    auto touch = new Touch(client, bind->version(), id, q_ptr);
    // TODO(romangg): check for error

    touchs << touch;

    if (globalTouch.focus.surface && globalTouch.focus.surface->client() == client) {
        // this is a touch for the currently focused touch surface
        globalTouch.focus.touchs << touch;
        if (!globalTouch.ids.isEmpty()) {
            // TODO(unknown author): send out all the points
        }
    }
    QObject::connect(touch, &Touch::resourceDestroyed, q_ptr, [touch, this] {
        touchs.removeAt(touchs.indexOf(touch));
        globalTouch.focus.touchs.removeOne(touch);
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
}

void Seat::setDragTarget(Surface* surface,
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

    // In theory we can have multiple data devices and we should send the drag to all of them, but
    // that seems overly complicated. In practice so far the only case for multiple data devices is
    // for clipboard overriding.
    d_ptr->drag.target = nullptr;
    if (!d_ptr->dataDevicesForSurface(surface).empty()) {
        d_ptr->drag.target = d_ptr->dataDevicesForSurface(surface).first();
    }

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
    m.translate(-surfacePosition.x(), -surfacePosition.y());
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
    if (pointers.isEmpty()) {
        Q_EMIT focusedPointerChanged(nullptr);
        for (auto p : qAsConst(framePointers)) {
            p->d_ptr->sendFrame();
        }
        return;
    }

    // TODO(unknown author): signal with all pointers
    Q_EMIT focusedPointerChanged(pointers.first());

    for (auto it = pointers.constBegin(), end = pointers.constEnd(); it != end; ++it) {
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
quint32 qtToWaylandButton(Qt::MouseButton button)
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
    return it.value() == Private::SeatPointer::State::Pressed;
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
    d_ptr->globalPointer.gestureSurface = QPointer<Surface>(d_ptr->globalPointer.focus.surface);
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
    d_ptr->globalPointer.gestureSurface = QPointer<Surface>(d_ptr->globalPointer.focus.surface);
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

Surface* Seat::focusedKeyboardSurface() const
{
    return d_ptr->keys.focus.surface;
}

void Seat::setFocusedKeyboardSurface(Surface* surface)
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
    if (d_ptr->keys.modifiers.value != (value)) {                                                  \
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

Touch* Seat::focusedTouch() const
{
    if (d_ptr->globalTouch.focus.touchs.isEmpty()) {
        return nullptr;
    }
    return d_ptr->globalTouch.focus.touchs.first();
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
    return !d_ptr->globalTouch.ids.isEmpty();
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
                      for (auto it = d_ptr->globalTouch.focus.touchs.constBegin(),
                                end = d_ptr->globalTouch.focus.touchs.constEnd();
                           it != end;
                           ++it) {
                          (*it)->cancel();
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
        forEachInterface<Pointer>(focusedTouchSurface(), d_ptr->pointers, [pos](Pointer* p) {
            p->d_ptr->sendMotion(pos);
        });
    }
    Q_EMIT touchMoved(id, d_ptr->globalTouch.ids[id], globalPosition);
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
        forEachInterface<Pointer>(focusedTouchSurface(), d_ptr->pointers, [serial](Pointer* p) {
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

void Seat::setFocusedTextInputSurface(Surface* surface)
{
    const quint32 serial = d_ptr->display()->handle()->nextSerial();
    const auto old = d_ptr->textInput.focus.textInput;
    if (d_ptr->textInput.focus.textInput) {
        // TODO(unknown author): setFocusedSurface like in other interfaces
        d_ptr->textInput.focus.textInput->d_ptr->sendLeave(serial, d_ptr->textInput.focus.surface);
    }
    if (d_ptr->textInput.focus.surface) {
        disconnect(d_ptr->textInput.focus.destroyConnection);
    }
    d_ptr->textInput.focus = Private::structTextInput::Focus();
    d_ptr->textInput.focus.surface = surface;
    TextInputV2* t = d_ptr->textInputForSurface(surface);
    if (t && !t->d_ptr->resource()) {
        t = nullptr;
    }
    d_ptr->textInput.focus.textInput = t;
    if (d_ptr->textInput.focus.surface) {
        d_ptr->textInput.focus.destroyConnection
            = connect(surface, &Surface::resourceDestroyed, this, [this] {
                  setFocusedTextInputSurface(nullptr);
              });
        d_ptr->textInput.focus.serial = serial;
    }
    if (t) {
        // TODO(unknown author): setFocusedSurface like in other interfaces
        t->d_ptr->sendEnter(surface, serial);
    }
    if (old != t) {
        Q_EMIT focusedTextInputChanged();
    }
}

Surface* Seat::focusedTextInputSurface() const
{
    return d_ptr->textInput.focus.surface;
}

TextInputV2* Seat::focusedTextInput() const
{
    return d_ptr->textInput.focus.textInput;
}

AbstractDataSource* Seat::selection() const
{
    return d_ptr->currentSelection;
}

void Seat::setSelection(AbstractDataSource* selection)
{
    if (d_ptr->currentSelection == selection) {
        return;
    }
    // cancel the previous selection
    if (d_ptr->currentSelection) {
        d_ptr->currentSelection->cancel();
        disconnect(d_ptr->currentSelection, nullptr, this, nullptr);
    }

    if (selection) {
        auto cleanup = [this]() { setSelection(nullptr); };
        connect(selection, &DataSource::resourceDestroyed, this, cleanup);
    }

    d_ptr->currentSelection = selection;

    for (auto focussedSelection : qAsConst(d_ptr->keys.focus.selections)) {
        if (selection) {
            focussedSelection->sendSelection(selection);
        } else {
            focussedSelection->sendClearSelection();
        }
    }
    Q_EMIT selectionChanged(selection);
}

}
