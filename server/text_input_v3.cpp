/*
    SPDX-FileCopyrightText: 2018 Roman Glig <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2020 Bhushan Shah <bshah@kde.org>
    SPDX-FileCopyrightText: 2021 dcz <gihuac.dcz@porcupinefactory.org>
    SPDX-FileCopyrightText: 2021 Roman Glig <subdiff@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#include "text_input_v3_p.h"

#include "display.h"
#include "seat_p.h"
#include "surface_p.h"

namespace Wrapland::Server
{

struct zwp_text_input_manager_v3_interface const text_input_manager_v3::Private::s_interface = {
    resourceDestroyCallback,
    cb<get_text_input_callback>,
};

text_input_v3_content_hints convert_content_hint(uint32_t hint)
{
    auto const hints = zwp_text_input_v3_content_hint(hint);
    text_input_v3_content_hints ret = text_input_v3_content_hint::none;

    if (hints & ZWP_TEXT_INPUT_V3_CONTENT_HINT_COMPLETION) {
        ret |= text_input_v3_content_hint::completion;
    }
    if (hints & ZWP_TEXT_INPUT_V3_CONTENT_HINT_SPELLCHECK) {
        ret |= text_input_v3_content_hint::spellcheck;
    }
    if (hints & ZWP_TEXT_INPUT_V3_CONTENT_HINT_AUTO_CAPITALIZATION) {
        ret |= text_input_v3_content_hint::auto_capitalization;
    }
    if (hints & ZWP_TEXT_INPUT_V3_CONTENT_HINT_LOWERCASE) {
        ret |= text_input_v3_content_hint::lowercase;
    }
    if (hints & ZWP_TEXT_INPUT_V3_CONTENT_HINT_UPPERCASE) {
        ret |= text_input_v3_content_hint::uppercase;
    }
    if (hints & ZWP_TEXT_INPUT_V3_CONTENT_HINT_TITLECASE) {
        ret |= text_input_v3_content_hint::titlecase;
    }
    if (hints & ZWP_TEXT_INPUT_V3_CONTENT_HINT_HIDDEN_TEXT) {
        ret |= text_input_v3_content_hint::hidden_text;
    }
    if (hints & ZWP_TEXT_INPUT_V3_CONTENT_HINT_SENSITIVE_DATA) {
        ret |= text_input_v3_content_hint::sensitive_data;
    }
    if (hints & ZWP_TEXT_INPUT_V3_CONTENT_HINT_LATIN) {
        ret |= text_input_v3_content_hint::latin;
    }
    if (hints & ZWP_TEXT_INPUT_V3_CONTENT_HINT_MULTILINE) {
        ret |= text_input_v3_content_hint::multiline;
    }
    return ret;
}

uint32_t convert_content_hints(text_input_v3_content_hints hints)
{
    uint32_t ret = 0;

    if (hints & text_input_v3_content_hint::completion) {
        ret |= ZWP_TEXT_INPUT_V3_CONTENT_HINT_COMPLETION;
    }
    if (hints & text_input_v3_content_hint::spellcheck) {
        ret |= ZWP_TEXT_INPUT_V3_CONTENT_HINT_SPELLCHECK;
    }
    if (hints & text_input_v3_content_hint::auto_capitalization) {
        ret |= ZWP_TEXT_INPUT_V3_CONTENT_HINT_AUTO_CAPITALIZATION;
    }
    if (hints & text_input_v3_content_hint::lowercase) {
        ret |= ZWP_TEXT_INPUT_V3_CONTENT_HINT_LOWERCASE;
    }
    if (hints & text_input_v3_content_hint::uppercase) {
        ret |= ZWP_TEXT_INPUT_V3_CONTENT_HINT_UPPERCASE;
    }
    if (hints & text_input_v3_content_hint::titlecase) {
        ret |= ZWP_TEXT_INPUT_V3_CONTENT_HINT_TITLECASE;
    }
    if (hints & text_input_v3_content_hint::hidden_text) {
        ret |= ZWP_TEXT_INPUT_V3_CONTENT_HINT_HIDDEN_TEXT;
    }
    if (hints & text_input_v3_content_hint::sensitive_data) {
        ret |= ZWP_TEXT_INPUT_V3_CONTENT_HINT_SENSITIVE_DATA;
    }
    if (hints & text_input_v3_content_hint::latin) {
        ret |= ZWP_TEXT_INPUT_V3_CONTENT_HINT_LATIN;
    }
    if (hints & text_input_v3_content_hint::multiline) {
        ret |= ZWP_TEXT_INPUT_V3_CONTENT_HINT_MULTILINE;
    }
    return ret;
}

static text_input_v3_content_purpose convert_content_purpose(uint32_t purpose)
{
    auto const wlPurpose = zwp_text_input_v3_content_purpose(purpose);

    switch (wlPurpose) {
    case ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_ALPHA:
        return text_input_v3_content_purpose::alpha;
    case ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_DIGITS:
        return text_input_v3_content_purpose::digits;
    case ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_NUMBER:
        return text_input_v3_content_purpose::number;
    case ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_PHONE:
        return text_input_v3_content_purpose::phone;
    case ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_URL:
        return text_input_v3_content_purpose::url;
    case ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_EMAIL:
        return text_input_v3_content_purpose::email;
    case ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_NAME:
        return text_input_v3_content_purpose::name;
    case ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_PASSWORD:
        return text_input_v3_content_purpose::password;
    case ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_DATE:
        return text_input_v3_content_purpose::date;
    case ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_TIME:
        return text_input_v3_content_purpose::time;
    case ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_DATETIME:
        return text_input_v3_content_purpose::datetime;
    case ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_TERMINAL:
        return text_input_v3_content_purpose::terminal;
    case ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_NORMAL:
    default:
        return text_input_v3_content_purpose::normal;
    }
}

uint32_t convert_content_purpose(text_input_v3_content_purpose purpose)
{
    switch (purpose) {
    case text_input_v3_content_purpose::alpha:
        return ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_ALPHA;
    case text_input_v3_content_purpose::digits:
        return ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_DIGITS;
    case text_input_v3_content_purpose::number:
        return ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_NUMBER;
    case text_input_v3_content_purpose::phone:
        return ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_PHONE;
    case text_input_v3_content_purpose::url:
        return ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_URL;
    case text_input_v3_content_purpose::email:
        return ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_EMAIL;
    case text_input_v3_content_purpose::name:
        return ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_NAME;
    case text_input_v3_content_purpose::password:
        return ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_PASSWORD;
    case text_input_v3_content_purpose::date:
        return ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_DATE;
    case text_input_v3_content_purpose::time:
        return ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_TIME;
    case text_input_v3_content_purpose::datetime:
        return ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_DATETIME;
    case text_input_v3_content_purpose::terminal:
        return ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_TERMINAL;
    case text_input_v3_content_purpose::normal:
    default:
        return ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_NORMAL;
    }
}

text_input_v3_change_cause convert_change_cause(uint32_t cause)
{
    auto const wlCause = zwp_text_input_v3_change_cause(cause);

    switch (wlCause) {
    case ZWP_TEXT_INPUT_V3_CHANGE_CAUSE_INPUT_METHOD:
        return text_input_v3_change_cause::input_method;
    case ZWP_TEXT_INPUT_V3_CHANGE_CAUSE_OTHER:
    default:
        return text_input_v3_change_cause::other;
    }
}

uint32_t convert_change_cause(text_input_v3_change_cause cause)
{
    switch (cause) {
    case text_input_v3_change_cause::input_method:
        return ZWP_TEXT_INPUT_V3_CHANGE_CAUSE_INPUT_METHOD;
    case text_input_v3_change_cause::other:
    default:
        return ZWP_TEXT_INPUT_V3_CHANGE_CAUSE_OTHER;
    }
}

text_input_manager_v3::Private::Private(Display* display, text_input_manager_v3* q)
    : text_input_manager_v3_global(q, display, &zwp_text_input_manager_v3_interface, &s_interface)
{
    create();
}

void text_input_manager_v3::Private::get_text_input_callback(text_input_manager_v3_bind* bind,
                                                             uint32_t id,
                                                             wl_resource* wlSeat)
{
    auto seat = SeatGlobal::get_handle(wlSeat);

    auto textInput = new text_input_v3(bind->client->handle, bind->version, id);
    textInput->d_ptr->seat = seat;
    seat->d_ptr->text_inputs.register_device(textInput);
}

text_input_manager_v3::text_input_manager_v3(Display* display)
    : d_ptr(new Private(display, this))
{
}

text_input_manager_v3::~text_input_manager_v3() = default;

const struct zwp_text_input_v3_interface text_input_v3::Private::s_interface = {
    destroyCallback,
    enable_callback,
    disable_callback,
    set_surrounding_text_callback,
    set_text_change_cause_callback,
    set_content_type_callback,
    set_cursor_rectangle_callback,
    set_commit_callback,
};

text_input_v3::Private::Private(Client* client, uint32_t version, uint32_t id, text_input_v3* q_ptr)
    : Wayland::Resource<text_input_v3>(client,
                                       version,
                                       id,
                                       &zwp_text_input_v3_interface,
                                       &s_interface,
                                       q_ptr)
    , q_ptr{q_ptr}
{
}

void text_input_v3::Private::send_enter(Surface* surface)
{
    assert(surface);
    entered_surface = surface;
    send<zwp_text_input_v3_send_enter>(surface->d_ptr->resource);
}

void text_input_v3::Private::send_leave(Surface* surface)
{
    assert(surface);
    current = {};
    pending = {};
    entered_surface = nullptr;
    send<zwp_text_input_v3_send_leave>(surface->d_ptr->resource);
}

void text_input_v3::Private::enable_callback([[maybe_unused]] wl_client* wlClient,
                                             wl_resource* wlResource)
{
    get_handle(wlResource)->d_ptr->pending.enabled = true;
}

void text_input_v3::Private::disable_callback([[maybe_unused]] wl_client* wlClient,
                                              wl_resource* wlResource)
{
    get_handle(wlResource)->d_ptr->pending.enabled = false;
}

void text_input_v3::Private::set_surrounding_text_callback([[maybe_unused]] wl_client* wlClient,
                                                           wl_resource* wlResource,
                                                           char const* text,
                                                           int32_t cursor,
                                                           int32_t anchor)
{
    auto priv = get_handle(wlResource)->d_ptr;

    priv->pending.surrounding_text.update = true;
    priv->pending.surrounding_text.data = text;
    priv->pending.surrounding_text.cursor_position = cursor;
    priv->pending.surrounding_text.selection_anchor = anchor;
}

void text_input_v3::Private::set_content_type_callback([[maybe_unused]] wl_client* wlClient,
                                                       wl_resource* wlResource,
                                                       uint32_t hint,
                                                       uint32_t purpose)
{
    auto priv = get_handle(wlResource)->d_ptr;

    priv->pending.content.hints = convert_content_hint(hint);
    priv->pending.content.purpose = convert_content_purpose(purpose);
}

void text_input_v3::Private::set_cursor_rectangle_callback([[maybe_unused]] wl_client* wlClient,
                                                           wl_resource* wlResource,
                                                           int32_t x,
                                                           int32_t y,
                                                           int32_t width,
                                                           int32_t height)
{
    auto priv = get_handle(wlResource)->d_ptr;
    priv->pending.cursor_rectangle = QRect(x, y, width, height);
}

void text_input_v3::Private::set_text_change_cause_callback([[maybe_unused]] wl_client* wlClient,
                                                            wl_resource* wlResource,
                                                            uint32_t cause)
{
    auto priv = get_handle(wlResource)->d_ptr;
    priv->pending.surrounding_text.change_cause = convert_change_cause(cause);
}

void text_input_v3::Private::set_commit_callback([[maybe_unused]] wl_client* wlClient,
                                                 wl_resource* wlResource)
{
    auto priv = get_handle(wlResource)->d_ptr;

    priv->serial++;

    if (auto pool = priv->seat->text_inputs(); pool.v3.text_input == priv->q_ptr) {
        pool.sync_to_input_method(priv->current, priv->pending);
    }
    priv->current = priv->pending;
    priv->pending.surrounding_text.update = false;

    Q_EMIT priv->handle->state_committed();
}

text_input_v3::text_input_v3(Client* client, uint32_t version, uint32_t id)
    : QObject(nullptr)
    , d_ptr(new Private(client, version, id, this))
{
}

void text_input_v3::set_preedit_string(std::string const& text,
                                       uint32_t cursor_begin,
                                       uint32_t cursor_end)
{
    d_ptr->send<zwp_text_input_v3_send_preedit_string>(text.c_str(), cursor_begin, cursor_end);
}

void text_input_v3::commit_string(std::string const& text)
{
    d_ptr->send<zwp_text_input_v3_send_commit_string>(text.c_str());
}
void text_input_v3::delete_surrounding_text(uint32_t before_length, uint32_t after_length)
{
    d_ptr->send<zwp_text_input_v3_send_delete_surrounding_text>(before_length, after_length);
}

void text_input_v3::done()
{
    d_ptr->send<zwp_text_input_v3_send_done>(d_ptr->serial);
}

text_input_v3_state const& text_input_v3::state() const
{
    return d_ptr->current;
}

Client* text_input_v3::client() const
{
    return d_ptr->client->handle;
}

Surface* text_input_v3::entered_surface() const
{
    return d_ptr->entered_surface;
}

}
