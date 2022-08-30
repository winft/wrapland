/****************************************************************************
Copyright © 2020 Adrien Faveraux <ad1rie3@hotmail.fr>

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
****************************************************************************/
#include "display.h"
#include "wayland/client.h"
#include "wayland/global.h"
#include "wayland/resource.h"

#include "seat_p.h"
#include "surface.h"
#include "surface_p.h"

#include "text_input_v2_p.h"

namespace Wrapland::Server
{

const struct zwp_text_input_manager_v2_interface text_input_manager_v2::Private::s_interface = {
    resourceDestroyCallback,
    cb<get_text_input_callback>,
};

text_input_manager_v2::Private::Private(Display* display, text_input_manager_v2* q_ptr)
    : text_input_manager_v2_global(q_ptr,
                                   display,
                                   &zwp_text_input_manager_v2_interface,
                                   &s_interface)
{
    create();
}

void text_input_manager_v2::Private::get_text_input_callback(text_input_manager_v2_bind* bind,
                                                             uint32_t id,
                                                             wl_resource* wlSeat)
{
    auto seat = SeatGlobal::get_handle(wlSeat);

    auto textInput = new text_input_v2(bind->client->handle, bind->version, id);

    textInput->d_ptr->seat = seat;

    seat->d_ptr->text_inputs.register_device(textInput);
}

text_input_manager_v2::text_input_manager_v2(Display* display)
    : d_ptr(new Private(display, this))
{
}

text_input_manager_v2::~text_input_manager_v2() = default;

const struct zwp_text_input_v2_interface text_input_v2::Private::s_interface = {
    destroyCallback,
    enable_callback,
    disable_callback,
    show_input_panel_callback,
    hide_input_panel_callback,
    set_surrounding_text_callback,
    set_content_type_callback,
    set_cursor_rectangle_callback,
    set_preferred_language_callback,
    update_state_callback,
};

text_input_v2::Private::Private(Client* client, uint32_t version, uint32_t id, text_input_v2* q_ptr)
    : Wayland::Resource<text_input_v2>(client,
                                       version,
                                       id,
                                       &zwp_text_input_v2_interface,
                                       &s_interface,
                                       q_ptr)
    , q_ptr{q_ptr}
{
}

void text_input_v2::Private::sync(text_input_v2_state const& old)
{
    if (seat->text_inputs().v2.text_input == q_ptr) {
        seat->text_inputs().sync_to_input_method(old, state);
    }
}

void text_input_v2::Private::enable(Surface* surface)
{
    auto changed = this->surface.data() != surface || !state.enabled;
    auto const old = state;

    this->surface = QPointer<Surface>(surface);
    state.enabled = true;

    if (changed) {
        sync(old);
    }
}

void text_input_v2::Private::disable()
{
    auto changed = state.enabled;
    auto const old = state;

    surface.clear();
    state.enabled = false;

    if (changed) {
        sync(old);
    }
}

void text_input_v2::Private::send_enter(Surface* surface, uint32_t serial)
{
    if (!surface) {
        return;
    }
    send<zwp_text_input_v2_send_enter>(serial, surface->d_ptr->resource);
}

void text_input_v2::Private::send_leave(uint32_t serial, Surface* surface)
{
    if (!surface) {
        return;
    }
    send<zwp_text_input_v2_send_leave>(serial, surface->d_ptr->resource);
}

void text_input_v2::Private::enable_callback(wl_client* /*wlClient*/,
                                             wl_resource* wlResource,
                                             wl_resource* surface)
{
    auto priv = get_handle(wlResource)->d_ptr;
    priv->enable(Wayland::Resource<Surface>::get_handle(surface));
}

void text_input_v2::Private::disable_callback(wl_client* /*wlClient*/,
                                              wl_resource* wlResource,
                                              wl_resource* /*surface*/)
{
    auto priv = get_handle(wlResource)->d_ptr;
    priv->disable();
}

void text_input_v2::Private::update_state_callback(wl_client* /*wlClient*/,
                                                   wl_resource* wlResource,
                                                   uint32_t /*serial*/,
                                                   uint32_t reason)
{
    auto priv = get_handle(wlResource)->d_ptr;

    // TODO(unknown author): use other reason values reason
    if (reason == ZWP_TEXT_INPUT_V2_UPDATE_STATE_RESET) {
        Q_EMIT priv->handle->reset_requested();
    }
}

void text_input_v2::Private::show_input_panel_callback(wl_client* /*wlClient*/,
                                                       wl_resource* wlResource)
{
    auto priv = get_handle(wlResource)->d_ptr;
    Q_EMIT priv->handle->show_input_panel_requested();
}

void text_input_v2::Private::hide_input_panel_callback(wl_client* /*wlClient*/,
                                                       wl_resource* wlResource)
{
    auto priv = get_handle(wlResource)->d_ptr;
    Q_EMIT priv->handle->hide_input_panel_requested();
}

void text_input_v2::Private::set_surrounding_text_callback(wl_client* /*wlClient*/,
                                                           wl_resource* wlResource,
                                                           char const* text,
                                                           int32_t cursor,
                                                           int32_t anchor)
{
    auto priv = get_handle(wlResource)->d_ptr;

    if (priv->state.surrounding_text.data == text
        && priv->state.surrounding_text.cursor_position == cursor
        && priv->state.surrounding_text.selection_anchor == anchor) {
        return;
    }

    auto const old = priv->state;
    priv->state.surrounding_text.data = text;
    priv->state.surrounding_text.cursor_position = cursor;
    priv->state.surrounding_text.selection_anchor = anchor;

    priv->sync(old);
    Q_EMIT priv->handle->surrounding_text_changed();
}

text_input_v2_content_hints convert_hint(uint32_t hint)
{
    auto const hints = static_cast<zwp_text_input_v2_content_hint>(hint);
    text_input_v2_content_hints ret = text_input_v2_content_hint::none;

    if (hints & ZWP_TEXT_INPUT_V2_CONTENT_HINT_AUTO_COMPLETION) {
        ret |= text_input_v2_content_hint::completion;
    }
    if (hints & ZWP_TEXT_INPUT_V2_CONTENT_HINT_AUTO_CORRECTION) {
        ret |= text_input_v2_content_hint::correction;
    }
    if (hints & ZWP_TEXT_INPUT_V2_CONTENT_HINT_AUTO_CAPITALIZATION) {
        ret |= text_input_v2_content_hint::capitalization;
    }
    if (hints & ZWP_TEXT_INPUT_V2_CONTENT_HINT_LOWERCASE) {
        ret |= text_input_v2_content_hint::lowercase;
    }
    if (hints & ZWP_TEXT_INPUT_V2_CONTENT_HINT_UPPERCASE) {
        ret |= text_input_v2_content_hint::uppercase;
    }
    if (hints & ZWP_TEXT_INPUT_V2_CONTENT_HINT_TITLECASE) {
        ret |= text_input_v2_content_hint::titlecase;
    }
    if (hints & ZWP_TEXT_INPUT_V2_CONTENT_HINT_HIDDEN_TEXT) {
        ret |= text_input_v2_content_hint::hidden_text;
    }
    if (hints & ZWP_TEXT_INPUT_V2_CONTENT_HINT_SENSITIVE_DATA) {
        ret |= text_input_v2_content_hint::sensitive_data;
    }
    if (hints & ZWP_TEXT_INPUT_V2_CONTENT_HINT_LATIN) {
        ret |= text_input_v2_content_hint::latin;
    }
    if (hints & ZWP_TEXT_INPUT_V2_CONTENT_HINT_MULTILINE) {
        ret |= text_input_v2_content_hint::multiline;
    }
    return ret;
}

text_input_v2_content_purpose convert_purpose(uint32_t purpose)
{
    auto const wlPurpose = static_cast<zwp_text_input_v2_content_purpose>(purpose);

    switch (wlPurpose) {
    case ZWP_TEXT_INPUT_V2_CONTENT_PURPOSE_ALPHA:
        return text_input_v2_content_purpose::alpha;
    case ZWP_TEXT_INPUT_V2_CONTENT_PURPOSE_DIGITS:
        return text_input_v2_content_purpose::digits;
    case ZWP_TEXT_INPUT_V2_CONTENT_PURPOSE_NUMBER:
        return text_input_v2_content_purpose::number;
    case ZWP_TEXT_INPUT_V2_CONTENT_PURPOSE_PHONE:
        return text_input_v2_content_purpose::phone;
    case ZWP_TEXT_INPUT_V2_CONTENT_PURPOSE_URL:
        return text_input_v2_content_purpose::url;
    case ZWP_TEXT_INPUT_V2_CONTENT_PURPOSE_EMAIL:
        return text_input_v2_content_purpose::email;
    case ZWP_TEXT_INPUT_V2_CONTENT_PURPOSE_NAME:
        return text_input_v2_content_purpose::name;
    case ZWP_TEXT_INPUT_V2_CONTENT_PURPOSE_PASSWORD:
        return text_input_v2_content_purpose::password;
    case ZWP_TEXT_INPUT_V2_CONTENT_PURPOSE_DATE:
        return text_input_v2_content_purpose::date;
    case ZWP_TEXT_INPUT_V2_CONTENT_PURPOSE_TIME:
        return text_input_v2_content_purpose::time;
    case ZWP_TEXT_INPUT_V2_CONTENT_PURPOSE_DATETIME:
        return text_input_v2_content_purpose::datetime;
    case ZWP_TEXT_INPUT_V2_CONTENT_PURPOSE_TERMINAL:
        return text_input_v2_content_purpose::terminal;
    case ZWP_TEXT_INPUT_V2_CONTENT_PURPOSE_NORMAL:
    default:
        return text_input_v2_content_purpose::normal;
    }
}

void text_input_v2::Private::set_content_type_callback(wl_client* /*wlClient*/,
                                                       wl_resource* wlResource,
                                                       uint32_t hint,
                                                       uint32_t purpose)
{
    auto priv = get_handle(wlResource)->d_ptr;
    auto const content_hints = convert_hint(hint);
    auto const content_purpose = convert_purpose(purpose);

    if (content_hints == priv->state.content.hints
        && content_purpose == priv->state.content.purpose) {
        return;
    }

    auto const old = priv->state;
    priv->state.content.hints = content_hints;
    priv->state.content.purpose = content_purpose;

    priv->sync(old);
    Q_EMIT priv->handle->content_type_changed();
}

void text_input_v2::Private::set_cursor_rectangle_callback(wl_client* /*wlClient*/,
                                                           wl_resource* wlResource,
                                                           int32_t pos_x,
                                                           int32_t pos_y,
                                                           int32_t width,
                                                           int32_t height)
{
    auto priv = get_handle(wlResource)->d_ptr;
    auto const rect = QRect(pos_x, pos_y, width, height);

    if (priv->state.cursor_rectangle == rect) {
        return;
    }

    auto const old = priv->state;
    priv->state.cursor_rectangle = rect;

    priv->sync(old);
    Q_EMIT priv->handle->cursor_rectangle_changed();
}

void text_input_v2::Private::set_preferred_language_callback(wl_client* /*wlClient*/,
                                                             wl_resource* wlResource,
                                                             char const* language)
{
    auto priv = get_handle(wlResource)->d_ptr;

    if (priv->state.preferred_language == language) {
        return;
    }

    auto const old = priv->state;
    priv->state.preferred_language = language;

    priv->sync(old);
    Q_EMIT priv->handle->preferred_language_changed();
}

text_input_v2::text_input_v2(Client* client, uint32_t version, uint32_t id)
    : QObject(nullptr)
    , d_ptr(new Private(client, version, id, this))
{
}

text_input_v2_state const& text_input_v2::state() const
{
    return d_ptr->state;
}

void text_input_v2::set_preedit_string(std::string const& text, std::string const& commit)
{
    d_ptr->send<zwp_text_input_v2_send_preedit_string>(text.c_str(), commit.c_str());
}

void text_input_v2::commit(std::string const& text)
{
    d_ptr->send<zwp_text_input_v2_send_commit_string>(text.c_str());
}

void text_input_v2::keysym_pressed(uint32_t keysym, Qt::KeyboardModifiers /*modifiers*/)
{
    d_ptr->send<zwp_text_input_v2_send_keysym>(
        d_ptr->seat ? d_ptr->seat->timestamp() : 0, keysym, WL_KEYBOARD_KEY_STATE_PRESSED, 0);
}

void text_input_v2::keysym_released(uint32_t keysym, Qt::KeyboardModifiers /*modifiers*/)
{
    d_ptr->send<zwp_text_input_v2_send_keysym>(
        d_ptr->seat ? d_ptr->seat->timestamp() : 0, keysym, WL_KEYBOARD_KEY_STATE_RELEASED, 0);
}

void text_input_v2::delete_surrounding_text(uint32_t beforeLength, uint32_t afterLength)
{
    d_ptr->send<zwp_text_input_v2_send_delete_surrounding_text>(beforeLength, afterLength);
}

void text_input_v2::set_cursor_position(int32_t index, int32_t anchor)
{
    d_ptr->send<zwp_text_input_v2_send_cursor_position>(index, anchor);
}

void text_input_v2::set_text_direction(Qt::LayoutDirection direction)
{
    auto wlDirection = ZWP_TEXT_INPUT_V2_TEXT_DIRECTION_AUTO;

    switch (direction) {
    case Qt::LeftToRight:
        wlDirection = ZWP_TEXT_INPUT_V2_TEXT_DIRECTION_LTR;
        break;
    case Qt::RightToLeft:
        wlDirection = ZWP_TEXT_INPUT_V2_TEXT_DIRECTION_RTL;
        break;
    case Qt::LayoutDirectionAuto:
        wlDirection = ZWP_TEXT_INPUT_V2_TEXT_DIRECTION_AUTO;
        break;
    default:
        Q_UNREACHABLE();
        break;
    }

    d_ptr->send<zwp_text_input_v2_send_text_direction>(wlDirection);
}

void text_input_v2::set_preedit_cursor(int32_t index)
{
    d_ptr->send<zwp_text_input_v2_send_preedit_cursor>(index);
}

void text_input_v2::set_input_panel_state(bool visible, QRect const& overlapped_surface_area)
{
    if (d_ptr->input_panel_visible == visible
        && d_ptr->overlapped_surface_area == overlapped_surface_area) {
        // Not changed.
        return;
    }

    d_ptr->input_panel_visible = visible;
    d_ptr->overlapped_surface_area = overlapped_surface_area;

    d_ptr->send<zwp_text_input_v2_send_input_panel_state>(
        visible ? ZWP_TEXT_INPUT_V2_INPUT_PANEL_VISIBILITY_VISIBLE
                : ZWP_TEXT_INPUT_V2_INPUT_PANEL_VISIBILITY_HIDDEN,
        overlapped_surface_area.x(),
        overlapped_surface_area.y(),
        overlapped_surface_area.width(),
        overlapped_surface_area.height());
}

void text_input_v2::set_language(std::string const& language_tag)
{
    if (d_ptr->language == language_tag) {
        return;
    }
    d_ptr->language = language_tag;
    d_ptr->send<zwp_text_input_v2_send_language>(language_tag.c_str());
}

QPointer<Surface> text_input_v2::surface() const
{
    return d_ptr->surface;
}

Client* text_input_v2::client() const
{
    return d_ptr->client->handle;
}

}
