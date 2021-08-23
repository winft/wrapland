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

#include "drag_pool.h"
#include "keyboard_pool.h"
#include "pointer_pool.h"
#include "selection_pool.h"
#include "touch_pool.h"

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

    TextInputV2* textInputV2ForSurface(Surface* surface) const;
    text_input_v3* textInputV3ForSurface(Surface* surface) const;

    void registerInputMethod(input_method_v2* im);
    void registerTextInput(TextInputV2* ti);
    void registerTextInput(text_input_v3* ti);

    std::string name;
    bool pointer = false;
    bool keyboard = false;
    bool touch = false;
    QList<wl_resource*> resources;
    uint32_t timestamp = 0;
    pointer_pool pointers;
    keyboard_pool keyboards;
    touch_pool touches;
    drag_pool drags;
    selection_pool<DataDevice, &Seat::selectionChanged> data_devices;
    selection_pool<PrimarySelectionDevice, &Seat::primarySelectionChanged>
        primary_selection_devices;
    input_method_v2* input_method{nullptr};
    std::vector<TextInputV2*> textInputs;
    std::vector<text_input_v3*> textInputsV3;

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

    // legacy
    friend class SeatInterface;
    //

    Seat* q_ptr;

private:
    static void getPointerCallback(SeatBind* bind, uint32_t id);
    static void getKeyboardCallback(SeatBind* bind, uint32_t id);
    static void getTouchCallback(SeatBind* bind, uint32_t id);

    static const struct wl_seat_interface s_interface;
};

}
