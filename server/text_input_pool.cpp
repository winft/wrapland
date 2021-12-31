#include "text_input_pool.h"
#include "display.h"
#include "input_method_v2.h"
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

void text_input_pool::register_device(text_input_v2* ti)
{
    // Text input version 0 might call this multiple times.
    if (std::find(v2_devices.begin(), v2_devices.end(), ti) != v2_devices.end()) {
        return;
    }
    v2_devices.push_back(ti);
    if (focus.surface && focus.surface->client() == ti->d_ptr->client->handle) {
        // This is a text input for the currently focused text input surface.
        if (!v2.text_input) {
            v2.text_input = ti;
            ti->d_ptr->send_enter(focus.surface, v2.serial);
            Q_EMIT seat->focusedTextInputChanged();
        }
    }
    QObject::connect(ti, &text_input_v2::resourceDestroyed, seat, [this, ti] {
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
    if (focus.surface && focus.surface->client() == ti->d_ptr->client->handle) {
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
    auto const serial = seat->d_ptr->display()->handle->nextSerial();
    auto const old_ti = v2.text_input;

    if (old_ti) {
        old_ti->d_ptr->send_leave(serial, focus.surface);
    }

    v2.text_input = nullptr;

    if (!v3.text_input) {
        // Only text-input v3 not set, we allow v2 to be active.
        v2.text_input = interfaceForSurface(surface, v2_devices);
    }

    if (surface) {
        v2.serial = serial;
    }

    if (v2.text_input) {
        v2.text_input->d_ptr->send_enter(surface, serial);
    }

    return old_ti != v2.text_input;
}

bool text_input_pool::set_v3_focused_surface(Surface* surface)
{
    auto const old_ti = v3.text_input;

    if (old_ti) {
        old_ti->d_ptr->send_leave(focus.surface);
    }

    v3.text_input = interfaceForSurface(surface, v3_devices);

    if (v3.text_input) {
        v3.text_input->d_ptr->send_enter(surface);
    }

    return old_ti != v3.text_input;
}

void text_input_pool::set_focused_surface(Surface* surface)
{
    if (focus.surface) {
        QObject::disconnect(focus.destroy_connection);
    }

    auto changed = set_v3_focused_surface(surface);
    changed |= set_v2_focused_surface(surface);

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

void sync_to_text_input_v2(text_input_v2* ti,
                           input_method_v2_state const& prev,
                           input_method_v2_state const& next)
{
    if (!ti) {
        return;
    }

    if (next.delete_surrounding_text.update) {
        auto const& text = next.delete_surrounding_text;
        ti->delete_surrounding_text(text.before_length, text.after_length);
    }
    if (prev.preedit_string.data != next.preedit_string.data) {
        ti->set_preedit_string(next.preedit_string.data, "");
    }
    if (prev.preedit_string.cursor_begin != next.preedit_string.cursor_begin
        || prev.preedit_string.cursor_end != next.preedit_string.cursor_end) {
        ti->set_cursor_position(static_cast<int32_t>(next.preedit_string.cursor_begin),
                                static_cast<int32_t>(next.preedit_string.cursor_end));
    }
    if (prev.commit_string.data != next.commit_string.data) {
        ti->commit(next.commit_string.data);
    }
}

// TODO(romangg): With C++20's default comparision operator compare prev members with next members
// and maybe remove the "update" members in the state. Or are the string comparisons to expensive?
void sync_to_text_input_v3(text_input_v3* ti,
                           input_method_v2_state const& /*prev*/,
                           input_method_v2_state const& next)
{
    if (!ti) {
        return;
    }

    auto has_update{false};

    if (next.delete_surrounding_text.update) {
        auto const& text = next.delete_surrounding_text;
        ti->delete_surrounding_text(text.before_length, text.after_length);
        has_update = true;
    }
    if (next.preedit_string.update) {
        auto const& preedit = next.preedit_string;
        ti->set_preedit_string(preedit.data, preedit.cursor_begin, preedit.cursor_end);
        has_update = true;
    }
    if (next.commit_string.update) {
        ti->commit_string(next.commit_string.data);
        has_update = true;
    }

    if (has_update) {
        ti->done();
    }
}

void text_input_pool::sync_to_text_input(input_method_v2_state const& prev,
                                         input_method_v2_state const& next) const
{
    sync_to_text_input_v2(v2.text_input, prev, next);
    sync_to_text_input_v3(v3.text_input, prev, next);
}

text_input_v3_content_hints convert_hints_v2_to_v3(text_input_v2_content_hints hints)
{
    auto hints_number = static_cast<uint32_t>(hints);
    return static_cast<text_input_v3_content_hints>(hints_number);
}

text_input_v3_content_purpose convert_purpose_v2_to_v3(text_input_v2_content_purpose purpose)
{
    return static_cast<text_input_v3_content_purpose>(purpose);
}

void sync_to_input_method_v2(input_method_v2* im,
                             text_input_v2_state const& prev,
                             text_input_v2_state const& next)
{
    if (!im) {
        return;
    }

    auto has_update{false};

    if (prev.enabled != next.enabled) {
        im->set_active(next.enabled);
        has_update = true;
    }
    if (next.surrounding_text.data != prev.surrounding_text.data
        || next.surrounding_text.cursor_position != prev.surrounding_text.cursor_position
        || next.surrounding_text.selection_anchor != prev.surrounding_text.selection_anchor) {
        auto const& text = next.surrounding_text;
        im->set_surrounding_text(text.data,
                                 text.cursor_position,
                                 text.selection_anchor,
                                 text_input_v3_change_cause::input_method);
        has_update = true;
    }
    if (prev.content.hints != next.content.hints || prev.content.purpose != next.content.purpose) {
        im->set_content_type(convert_hints_v2_to_v3(next.content.hints),
                             convert_purpose_v2_to_v3(next.content.purpose));
        has_update = true;
    }

    if (has_update) {
        im->done();
    }

    if (prev.cursor_rectangle != next.cursor_rectangle) {
        for (auto popup : im->get_popups()) {
            popup->set_text_input_rectangle(next.cursor_rectangle);
        }
    }
}

void sync_to_input_method_v2(input_method_v2* im,
                             text_input_v3_state const& prev,
                             text_input_v3_state const& next)
{
    if (!im) {
        return;
    }

    auto has_update{false};

    if (prev.enabled != next.enabled) {
        im->set_active(next.enabled);
        has_update = true;
    }
    if (next.surrounding_text.update) {
        auto const& text = next.surrounding_text;
        im->set_surrounding_text(
            text.data, text.cursor_position, text.selection_anchor, text.change_cause);
        has_update = true;
    }
    if (prev.content.hints != next.content.hints || prev.content.purpose != next.content.purpose) {
        im->set_content_type(next.content.hints, next.content.purpose);
        has_update = true;
    }

    if (has_update) {
        im->done();
    }

    if (prev.cursor_rectangle != next.cursor_rectangle) {
        for (auto popup : im->get_popups()) {
            popup->set_text_input_rectangle(next.cursor_rectangle);
        }
    }
}

void text_input_pool::sync_to_input_method(text_input_v2_state const& prev,
                                           text_input_v2_state const& next) const
{
    if (prev.enabled != next.enabled) {
        Q_EMIT seat->text_input_v2_enabled_changed(next.enabled);
    }

    sync_to_input_method_v2(seat->get_input_method_v2(), prev, next);
}

void text_input_pool::sync_to_input_method(text_input_v3_state const& prev,
                                           text_input_v3_state const& next) const
{
    if (prev.enabled != next.enabled) {
        Q_EMIT seat->text_input_v3_enabled_changed(next.enabled);
    }

    sync_to_input_method_v2(seat->get_input_method_v2(), prev, next);
}

}
