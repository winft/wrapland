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

#include "wayland/global.h"

#include <QHash>
#include <QMap>
#include <QPointer>
#include <QVector>

#include <string>
#include <wayland-server.h>

namespace Wrapland
{
namespace Server
{

class DataDevice;
class TextInputInterface;

using Sender = std::function<void(wl_resource*)>;

class Seat::Private : public Wayland::Global<Seat>
{
public:
    Private(Seat* q, D_isplay* d);
    ~Private() override = default;

    void bindInit(Wayland::Client* client, uint32_t version, uint32_t id) override;

    uint32_t version() const override;

    void sendCapabilities();
    void sendCapabilities(Wayland::Client* client);
    void sendName();
    void sendName(Wayland::Client* client);

    uint32_t getCapabilities() const;

    QVector<Pointer*> pointersForSurface(Surface* surface) const;
    QVector<Keyboard*> keyboardsForSurface(Surface* surface) const;
    QVector<Touch*> touchsForSurface(Surface* surface) const;
    DataDevice* dataDeviceForSurface(Surface* surface) const;
    TextInputInterface* textInputForSurface(Surface* surface) const;
    void registerDataDevice(DataDevice* dataDevice);
    void registerTextInput(TextInputInterface* textInput);
    void endDrag(quint32 serial);
    void cancelPreviousSelection(DataDevice* newlySelectedDataDevice);

    std::string name;
    bool pointer = false;
    bool keyboard = false;
    bool touch = false;
    QList<wl_resource*> resources;
    quint32 timestamp = 0;
    QVector<Pointer*> pointers;
    QVector<Keyboard*> keyboards;
    QVector<Touch*> touchs;
    QVector<DataDevice*> dataDevices;
    QVector<TextInputInterface*> textInputs;
    DataDevice* currentSelection = nullptr;

    // Pointer related members
    struct SeatPointer {
        enum class State {
            Released,
            Pressed,
        };
        QHash<quint32, quint32> buttonSerials;
        QHash<quint32, State> buttonStates;
        QPointF pos;
        struct Focus {
            Surface* surface = nullptr;
            QVector<Pointer*> pointers;
            QMetaObject::Connection destroyConnection;
            QPointF offset = QPointF();
            QMatrix4x4 transformation;
            quint32 serial = 0;
        };
        Focus focus;
        QPointer<Surface> gestureSurface;
    };
    SeatPointer globalPointer;

    void updatePointerButtonSerial(quint32 button, quint32 serial);
    void updatePointerButtonState(quint32 button, SeatPointer::State state);

    // Keyboard related members
    struct SeatKeyboard {
        enum class State {
            Released,
            Pressed,
        };
        QHash<quint32, State> states;
        struct Keymap {
            int fd = -1;
            quint32 size = 0;
            bool xkbcommonCompatible = false;
        };
        Keymap keymap;
        struct Modifiers {
            quint32 depressed = 0;
            quint32 latched = 0;
            quint32 locked = 0;
            quint32 group = 0;
            quint32 serial = 0;
        };
        Modifiers modifiers;
        struct Focus {
            Surface* surface = nullptr;
            QVector<Keyboard*> keyboards;
            QMetaObject::Connection destroyConnection;
            quint32 serial = 0;
            DataDevice* selection = nullptr;
        };
        Focus focus;
        quint32 lastStateSerial = 0;
        struct {
            qint32 charactersPerSecond = 0;
            qint32 delay = 0;
        } keyRepeat;
    };
    SeatKeyboard keys;

    bool updateKey(quint32 key, SeatKeyboard::State state);

    struct TextInput {
        struct Focus {
            Surface* surface = nullptr;
            QMetaObject::Connection destroyConnection;
            quint32 serial = 0;
            TextInputInterface* textInput = nullptr;
        };
        Focus focus;
    };
    TextInput textInput;

    // Touch related members
    struct SeatTouch {
        struct Focus {
            Surface* surface = nullptr;
            QVector<Touch*> touchs;
            QMetaObject::Connection destroyConnection;
            QPointF offset = QPointF();
            QPointF firstTouchPos;
        };
        Focus focus;
        QMap<qint32, quint32> ids;
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
    };
    Drag drag;

    // legacy
    friend class SeatInterface;
    //

    Seat* q_ptr;

private:
    void getPointer(Client* client, uint32_t id, /*TODO legacy*/ wl_resource* resource);
    void getKeyboard(Client* client, uint32_t id, /*TODO legacy*/ wl_resource* resource);
    void getTouch(Client* client, uint32_t id, /*TODO legacy*/ wl_resource* resource);

    void updateSelection(DataDevice* dataDevice, bool set);
    void cleanupDataDevice(DataDevice* dataDevice);

    static void getPointerCallback(wl_client* wlClient, wl_resource* wlResource, uint32_t id);
    static void getKeyboardCallback(wl_client* wlClient, wl_resource* wlResource, uint32_t id);
    static void getTouchCallback(wl_client* wlClient, wl_resource* wlResource, uint32_t id);

    static const struct wl_seat_interface s_interface;
    static const quint32 s_version;

    static const qint32 s_pointerVersion;
    static const qint32 s_touchVersion;
    static const qint32 s_keyboardVersion;
};

}
}