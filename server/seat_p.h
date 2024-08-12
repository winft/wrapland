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
#include "text_input_pool.h"
#include "touch_pool.h"

#include "wayland/global.h"

#include <QHash>
#include <QMap>
#include <QPointer>
#include <QVector>

#include <map>
#include <optional>
#include <string>
#include <vector>
#include <wayland-server.h>

namespace Wrapland::Server
{
class data_device;
class primary_selection_device;

constexpr uint32_t SeatVersion = 5;
using SeatGlobal = Wayland::Global<Seat, SeatVersion>;

class Seat::Private : public SeatGlobal
{
public:
    Private(Seat* q_ptr, Display* display);
    ~Private() override;

    void bindInit(SeatGlobal::bind_t* bind) override;

    void sendCapabilities();
    void sendName();

    uint32_t getCapabilities() const;

    template<typename Dev>
    void set_capability(uint32_t cap, std::optional<Dev>& dev, bool has)
    {
        if (static_cast<bool>(dev) == has) {
            return;
        }
        if (has) {
            prior_caps |= cap;
            dev = Dev(q_ptr);
        } else {
            dev.reset();
        }
        sendCapabilities();
    }

    std::string name;
    uint32_t timestamp = 0;

    std::optional<pointer_pool> pointers;
    std::optional<keyboard_pool> keyboards;
    std::optional<touch_pool> touches;
    uint32_t prior_caps{0};

    drag_pool drags;

    selection_pool<data_device, data_source, &Seat::selectionChanged> data_devices;
    selection_pool<primary_selection_device,
                   primary_selection_source,
                   &Seat::primarySelectionChanged>
        primary_selection_devices;

    input_method_v2* input_method{nullptr};
    text_input_pool text_inputs;

    Seat* q_ptr;

private:
    static void getPointerCallback(SeatGlobal::bind_t* bind, uint32_t id);
    static void getKeyboardCallback(SeatGlobal::bind_t* bind, uint32_t id);
    static void getTouchCallback(SeatGlobal::bind_t* bind, uint32_t id);

    static const struct wl_seat_interface s_interface;
};

}
