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
#pragma once

#include "seat.h"

#include "pointer_pool.h"

#include "wayland/global.h"

#include <QHash>
#include <QMap>
#include <QPointer>
#include <QVector>

#include <map>
#include <string>
#include <vector>
#include <wayland-server.h>

namespace Wrapland::Server
{

class DataDevice;
class TextInputV2;

constexpr uint32_t SeatVersion = 5;
using SeatGlobal = Wayland::Global<Seat, SeatVersion>;
using SeatBind = Wayland::Bind<SeatGlobal>;

class Seat::Private : public SeatGlobal
{
public:
    Private(Seat* q, Display* d);

    void bindInit(SeatBind* bind) override;

    void sendCapabilities();
    void sendName();

    uint32_t getCapabilities() const;

    std::vector<Keyboard*> keyboardsForSurface(Surface* surface) const;
    std::vector<Touch*> touchsForSurface(Surface* surface) const;
    std::vector<DataDevice*> dataDevicesForSurface(Surface* surface) const;

    TextInputV2* textInputV2ForSurface(Surface* surface) const;
    text_input_v3* textInputV3ForSurface(Surface* surface) const;

    template<typename Device>
    void register_device(Device*);

    void registerDataDevice(DataDevice* dataDevice);
    void registerPrimarySelectionDevice(PrimarySelectionDevice* primarySelectionDevice);
    void registerInputMethod(input_method_v2* im);
    void registerTextInput(TextInputV2* ti);
    void registerTextInput(text_input_v3* ti);
    void endDrag(uint32_t serial);
    void cancelPreviousSelection(DataDevice* newlySelectedDataDevice) const;
    void cancelPreviousSelection(PrimarySelectionDevice* dataDevice) const;

    std::string name;
    bool pointer = false;
    bool keyboard = false;
    bool touch = false;
    QList<wl_resource*> resources;
    uint32_t timestamp = 0;
    pointer_pool pointers;
    std::vector<Keyboard*> keyboards;
    std::vector<Touch*> touchs;
    std::vector<DataDevice*> dataDevices;
    std::vector<PrimarySelectionDevice*> primarySelectionDevices;
    input_method_v2* input_method{nullptr};
    std::vector<TextInputV2*> textInputs;
    std::vector<text_input_v3*> textInputsV3;
    DataDevice* currentSelection = nullptr;
    PrimarySelectionDevice* currentPrimarySelectionDevice = nullptr;

    // Keyboard related members
    struct SeatKeyboard {
        enum class State {
            Released,
            Pressed,
        };
        QHash<uint32_t, State> states;
        struct Keymap {
            int fd = -1;
            std::string content;
            bool xkbcommonCompatible = false;
        };
        Keymap keymap;
        struct Modifiers {
            bool operator==(Modifiers const& other) const
            {
                return depressed == other.depressed && latched == other.latched
                    && locked == other.locked && group == other.group && serial == other.serial;
            }
            bool operator!=(Modifiers const& other) const
            {
                return !(*this == other);
            }
            uint32_t depressed = 0;
            uint32_t latched = 0;
            uint32_t locked = 0;
            uint32_t group = 0;
            uint32_t serial = 0;
        };
        Modifiers modifiers;
        struct Focus {
            Surface* surface = nullptr;
            std::vector<Keyboard*> keyboards;
            QMetaObject::Connection destroyConnection;
            uint32_t serial = 0;
            std::vector<DataDevice*> selections;
            std::vector<PrimarySelectionDevice*> primarySelections;
        };
        Focus focus;
        uint32_t lastStateSerial = 0;
        struct {
            int32_t charactersPerSecond = 0;
            int32_t delay = 0;
        } keyRepeat;
    };
    SeatKeyboard keys;

    bool updateKey(uint32_t key, SeatKeyboard::State state);

    struct {
        struct {
            Surface* surface = nullptr;
            QMetaObject::Connection destroy_connection;
        } focus;

        // Both text inputs may be active at a time.
        // That doesn't make sense, but there's no reason to enforce only one.
        struct {
            uint32_t serial = 0;
            TextInputV2* text_input{nullptr};
        } v2;
        struct {
            text_input_v3* text_input{nullptr};
        } v3;
    } global_text_input;

    // Touch related members
    struct SeatTouch {
        struct Focus {
            Surface* surface = nullptr;
            std::vector<Touch*> touchs;
            QMetaObject::Connection destroyConnection;
            QPointF offset = QPointF();
            QPointF firstTouchPos;
        };
        Focus focus;

        // Key: Distinct id per touch point, Value: Wayland display serial.
        QMap<int32_t, uint32_t> ids;
    };
    SeatTouch globalTouch;

    struct Drag {
        enum class Mode {
            None,
            Pointer,
            Touch,
        };
        Mode mode = Mode::None;
        DataDevice* source = nullptr;
        DataDevice* target = nullptr;
        Surface* surface = nullptr;
        Pointer* sourcePointer = nullptr;
        Touch* sourceTouch = nullptr;
        QMatrix4x4 transformation;
        QMetaObject::Connection destroyConnection;
        QMetaObject::Connection dragSourceDestroyConnection;
        QMetaObject::Connection target_destroy_connection;
    };
    Drag drag;

    // legacy
    friend class SeatInterface;
    //

    Seat* q_ptr;

private:
    void getKeyboard(SeatBind* bind, uint32_t id);
    void getTouch(SeatBind* bind, uint32_t id);

    void updateSelection(DataDevice* dataDevice, bool set);
    void updateSelection(PrimarySelectionDevice* dataDevice, bool set);
    void cleanupDataDevice(DataDevice* dataDevice);
    void cleanupPrimarySelectionDevice(PrimarySelectionDevice* primarySelectionDevice);

    static void getPointerCallback(SeatBind* bind, uint32_t id);
    static void getKeyboardCallback(SeatBind* bind, uint32_t id);
    static void getTouchCallback(SeatBind* bind, uint32_t id);

    static const struct wl_seat_interface s_interface;
};

}
