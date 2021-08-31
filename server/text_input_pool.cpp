#include "text_input_pool.h"
#include "display.h"
#include "seat.h"
#include "seat_p.h"
#include "surface.h"
#include "text_input_v2.h"
#include "text_input_v2_p.h"
#include "text_input_v3.h"
#include "text_input_v3_p.h"
#include "utils.h"

namespace Wrapland::Server
{

text_input_pool::text_input_pool(Seat* seat)
    : seat{seat}
{
}

void text_input_pool::register_device(TextInputV2* ti)
{
    // Text input version 0 might call this multiple times.
    if (std::find(v2_devices.begin(), v2_devices.end(), ti) != v2_devices.end()) {
        return;
    }
    v2_devices.push_back(ti);
    if (focus.surface && focus.surface->client() == ti->d_ptr->client()->handle()) {
        // This is a text input for the currently focused text input surface.
        if (!v2.text_input) {
            v2.text_input = ti;
            ti->d_ptr->sendEnter(focus.surface, v2.serial);
            Q_EMIT seat->focusedTextInputChanged();
        }
    }
    QObject::connect(ti, &TextInputV2::resourceDestroyed, seat, [this, ti] {
        remove_one(v2_devices, ti);
        if (v2.text_input == ti) {
            v2.text_input = nullptr;
            Q_EMIT seat->focusedTextInputChanged();
        }
    });
}

void text_input_pool::register_device(text_input_v3* ti)
{
    // Text input version 0 might call this multiple times.
    if (std::find(v3_devices.begin(), v3_devices.end(), ti) != v3_devices.end()) {
        return;
    }
    v3_devices.push_back(ti);
    if (focus.surface && focus.surface->client() == ti->d_ptr->client()->handle()) {
        // This is a text input for the currently focused text input surface.
        if (!v3.text_input) {
            v3.text_input = ti;
            ti->d_ptr->send_enter(focus.surface);
            Q_EMIT seat->focusedTextInputChanged();
        }
    }
    QObject::connect(ti, &text_input_v3::resourceDestroyed, seat, [this, ti] {
        remove_one(v3_devices, ti);
        if (v3.text_input == ti) {
            v3.text_input = nullptr;
            Q_EMIT seat->focusedTextInputChanged();
        }
    });
}

bool text_input_pool::set_v2_focused_surface(Surface* surface)
{
    auto const serial = seat->d_ptr->display()->handle()->nextSerial();
    auto const old_ti = v2.text_input;

    if (old_ti) {
        // TODO(unknown author): setFocusedSurface like in other interfaces
        old_ti->d_ptr->sendLeave(serial, focus.surface);
    }

    auto ti = interfaceForSurface(surface, v2_devices);

    if (ti && !ti->d_ptr->resource()) {
        // TODO(romangg): can this check be removed?
        ti = nullptr;
    }

    v2.text_input = ti;

    if (surface) {
        v2.serial = serial;
    }

    if (ti) {
        // TODO(unknown author): setFocusedSurface like in other interfaces
        ti->d_ptr->sendEnter(surface, serial);
    }

    return old_ti != ti;
}

bool text_input_pool::set_v3_focused_surface(Surface* surface)
{
    auto const old_ti = v3.text_input;

    if (old_ti) {
        old_ti->d_ptr->send_leave(focus.surface);
    }

    auto ti = interfaceForSurface(surface, v3_devices);

    if (ti && !ti->d_ptr->resource()) {
        // TODO(romangg): can this check be removed?
        ti = nullptr;
    }

    v3.text_input = ti;

    if (ti) {
        ti->d_ptr->send_enter(surface);
    }

    return old_ti != ti;
}

void text_input_pool::set_focused_surface(Surface* surface)
{
    if (focus.surface) {
        QObject::disconnect(focus.destroy_connection);
    }

    auto changed = set_v2_focused_surface(surface) || set_v3_focused_surface(surface);
    focus = {};

    if (surface) {
        focus.surface = surface;
        focus.destroy_connection = QObject::connect(
            surface, &Surface::resourceDestroyed, seat, [this] { set_focused_surface(nullptr); });
    }

    if (changed) {
        Q_EMIT seat->focusedTextInputChanged();
    }
}

}
