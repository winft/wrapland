/*
    SPDX-FileCopyrightText: 2020 Aleix Pol Gonzalez <aleixpol@kde.org>
    SPDX-FileCopyrightText: 2021 Dorota Czaplejewicz <gihuac.dcz@porcupinefactory.org>
    SPDX-FileCopyrightText: 2021 Roman Glig <subdiff@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#include "input_method_v2_p.h"

#include "display.h"
#include "seat_p.h"
#include "surface_p.h"
#include "text_input_v3_p.h"
#include "wayland/client.h"

#include <wayland-input-method-unstable-v2-server-protocol.h>

namespace Wrapland::Server
{

struct zwp_input_method_manager_v2_interface const input_method_manager_v2::Private::s_interface = {
    cb<get_input_method_callback>,
    resourceDestroyCallback,
};

void input_method_manager_v2::Private::get_input_method_callback(input_method_manager_v2_bind* bind,
                                                                 wl_resource* wlSeat,
                                                                 uint32_t id)
{
    auto seat = SeatGlobal::handle(wlSeat);
    auto im = new input_method_v2(bind->client()->handle(), bind->version(), id);
    if (seat->get_input_method_v2()) {
        // Seat already has an input method.
        im->d_ptr->send<zwp_input_method_v2_send_unavailable>();
        return;
    }
    im->d_ptr->seat = seat;

    seat->d_ptr->input_method = im;

    QObject::connect(im, &input_method_v2::resourceDestroyed, seat, [seat] {
        seat->d_ptr->input_method = nullptr;
        Q_EMIT seat->input_method_v2_changed();
    });
    Q_EMIT seat->input_method_v2_changed();
}

input_method_manager_v2::Private::Private(Display* display, input_method_manager_v2* q)
    : input_method_manager_v2_global(q,
                                     display,
                                     &zwp_input_method_manager_v2_interface,
                                     &s_interface)
{
    create();
}

input_method_manager_v2::input_method_manager_v2(Display* display)
    : d_ptr(new Private(display, this))
{
}

input_method_manager_v2::~input_method_manager_v2() = default;

struct zwp_input_method_v2_interface const input_method_v2::Private::s_interface = {
    commit_string_callback,
    preedit_string_callback,
    delete_surrounding_text_callback,
    commit_callback,
    get_input_popup_surface_callback,
    grab_keyboard_callback,
    destroyCallback,
};

void input_method_v2::Private::commit_string_callback([[maybe_unused]] wl_client* wlClient,
                                                      wl_resource* wlResource,
                                                      char const* text)
{
    auto priv = handle(wlResource)->d_ptr;
    priv->pending.commit_string.data = text;
    priv->pending.commit_string.update = true;
}

void input_method_v2::Private::preedit_string_callback([[maybe_unused]] wl_client* wlClient,
                                                       wl_resource* wlResource,
                                                       char const* text,
                                                       int32_t cursor_begin,
                                                       int32_t cursor_end)
{
    auto priv = handle(wlResource)->d_ptr;
    priv->pending.preedit_string.data = text;
    priv->pending.preedit_string.cursor_begin = cursor_begin;
    priv->pending.preedit_string.cursor_end = cursor_end;
    priv->pending.preedit_string.update = true;
}

void input_method_v2::Private::delete_surrounding_text_callback(
    [[maybe_unused]] wl_client* wlClient,
    wl_resource* wlResource,
    uint32_t beforeBytes,
    uint32_t afterBytes)
{
    auto priv = handle(wlResource)->d_ptr;
    priv->pending.delete_surrounding_text.before_length = beforeBytes;
    priv->pending.delete_surrounding_text.after_length = afterBytes;
    priv->pending.delete_surrounding_text.update = true;
}

void input_method_v2::Private::commit_callback([[maybe_unused]] wl_client* wlClient,
                                               wl_resource* wlResource,
                                               uint32_t serial)
{
    auto priv = handle(wlResource)->d_ptr;

    if (priv->serial != serial) {
        // Not on latest done event. Reset pending to current state and wait for next commit.
        priv->pending = priv->current;
        return;
    }

    priv->seat->text_inputs().sync_to_text_input(priv->current, priv->pending);
    priv->current = priv->pending;

    priv->pending.preedit_string.update = false;
    priv->pending.commit_string.update = false;
    priv->pending.delete_surrounding_text.update = false;

    Q_EMIT priv->handle()->state_committed();
}

void input_method_v2::Private::get_input_popup_surface_callback(
    [[maybe_unused]] wl_client* wlClient,
    wl_resource* wlResource,
    uint32_t id,
    wl_resource* wlSurface)
{
    auto priv = handle(wlResource)->d_ptr;
    auto surface = Surface::Private::handle(wlSurface);
    // TODO(romangg): should send error when surface already has a role.

    auto popup
        = new input_method_popup_surface_v2(priv->client()->handle(), priv->version(), id, surface);

    priv->popups.push_back(popup);
    QObject::connect(popup,
                     &input_method_popup_surface_v2::resourceDestroyed,
                     priv->q_ptr,
                     [priv, popup] { remove_one(priv->popups, popup); });

    if (auto ti = priv->seat->text_inputs().v3.text_input) {
        popup->set_text_input_rectangle(ti->state().cursor_rectangle);
    }

    Q_EMIT priv->q_ptr->popup_surface_created(popup);
}

void input_method_v2::Private::grab_keyboard_callback([[maybe_unused]] wl_client* wlClient,
                                                      wl_resource* wlResource,
                                                      uint32_t id)
{
    auto priv = handle(wlResource)->d_ptr;
    auto grab = new input_method_keyboard_grab_v2(
        priv->client()->handle(), priv->version(), id, priv->seat);
    Q_EMIT priv->q_ptr->keyboard_grabbed(grab);
}

input_method_v2::Private::Private(Client* client, uint32_t version, uint32_t id, input_method_v2* q)
    : Wayland::Resource<input_method_v2>(client,
                                         version,
                                         id,
                                         &zwp_input_method_v2_interface,
                                         &s_interface,
                                         q)
    , q_ptr{q}
{
}

input_method_v2::input_method_v2(Client* client, uint32_t version, uint32_t id)
    : QObject(nullptr)
    , d_ptr(new Private(client, version, id, this))
{
}

void input_method_v2::set_active(bool active)
{
    if (active) {
        d_ptr->current = {};
        d_ptr->pending = {};
        d_ptr->send<zwp_input_method_v2_send_activate>();
    } else {
        d_ptr->send<zwp_input_method_v2_send_deactivate>();
    }
}

void input_method_v2::set_surrounding_text(
    std::string const& text,
    uint32_t cursor,
    uint32_t anchor,
    Wrapland::Server::text_input_v3_change_cause change_cause)
{
    d_ptr->send<zwp_input_method_v2_send_surrounding_text>(text.c_str(), cursor, anchor);
    d_ptr->send<zwp_input_method_v2_send_text_change_cause>(convert_change_cause(change_cause));
}

void input_method_v2::set_content_type(text_input_v3_content_hints hints,
                                       text_input_v3_content_purpose purpose)
{
    d_ptr->send<zwp_input_method_v2_send_content_type>(convert_content_hints(hints),
                                                       convert_content_purpose(purpose));
}

void input_method_v2::done()
{
    d_ptr->serial++;
    d_ptr->send<zwp_input_method_v2_send_done>();
}

input_method_v2_state const& input_method_v2::state() const
{
    return d_ptr->current;
}

std::vector<input_method_popup_surface_v2*> const& input_method_v2::get_popups() const
{
    return d_ptr->popups;
}

struct zwp_input_method_keyboard_grab_v2_interface const
    input_method_keyboard_grab_v2::Private::s_interface{
        destroyCallback,
    };

input_method_keyboard_grab_v2::Private::Private(Client* client,
                                                uint32_t version,
                                                uint32_t id,
                                                Seat* seat,
                                                input_method_keyboard_grab_v2* q)
    : Wayland::Resource<input_method_keyboard_grab_v2>(client,
                                                       version,
                                                       id,
                                                       &zwp_input_method_keyboard_grab_v2_interface,
                                                       &s_interface,
                                                       q)
    , seat{seat}
{
}

input_method_keyboard_grab_v2::input_method_keyboard_grab_v2(Client* client,
                                                             uint32_t version,
                                                             uint32_t id,
                                                             Wrapland::Server::Seat* seat)
    : QObject(nullptr)
    , d_ptr(new Private(client, version, id, seat, this))
{
}

void input_method_keyboard_grab_v2::set_keymap(std::string const& content)
{
    auto tmpf = std::tmpfile();

    std::fputs(content.data(), tmpf);
    std::rewind(tmpf);

    d_ptr->send<zwp_input_method_keyboard_grab_v2_send_keymap>(
        WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1, fileno(tmpf), content.size());
    d_ptr->keymap = file_wrap(tmpf);
}

void input_method_keyboard_grab_v2::key(uint32_t time, uint32_t key, key_state state)
{
    auto serial = d_ptr->client()->display()->handle()->nextSerial();
    d_ptr->send<zwp_input_method_keyboard_grab_v2_send_key>(serial,
                                                            time,
                                                            key,
                                                            state == key_state::pressed
                                                                ? WL_KEYBOARD_KEY_STATE_PRESSED
                                                                : WL_KEYBOARD_KEY_STATE_RELEASED);
}

void input_method_keyboard_grab_v2::update_modifiers(uint32_t depressed,
                                                     uint32_t latched,
                                                     uint32_t locked,
                                                     uint32_t group)
{
    auto serial = d_ptr->client()->display()->handle()->nextSerial();
    d_ptr->send<zwp_input_method_keyboard_grab_v2_send_modifiers>(
        serial, depressed, latched, locked, group);
}

void input_method_keyboard_grab_v2::set_repeat_info(int32_t rate, int32_t delay)
{
    d_ptr->send<zwp_input_method_keyboard_grab_v2_send_repeat_info>(rate, delay);
}

struct zwp_input_popup_surface_v2_interface const
    input_method_popup_surface_v2::Private::s_interface{
        destroyCallback,
    };

input_method_popup_surface_v2::Private::Private(Client* client,
                                                uint32_t version,
                                                uint32_t id,
                                                Surface* surface,
                                                input_method_popup_surface_v2* q)
    : Wayland::Resource<input_method_popup_surface_v2>(client,
                                                       version,
                                                       id,
                                                       &zwp_input_popup_surface_v2_interface,
                                                       &s_interface,
                                                       q)
    , surface{surface}
{
}

input_method_popup_surface_v2::input_method_popup_surface_v2(Client* client,
                                                             uint32_t version,
                                                             uint32_t id,
                                                             Wrapland::Server::Surface* surface)
    : QObject(nullptr)
    , d_ptr(new Private(client, version, id, surface, this))
{
}

Surface* input_method_popup_surface_v2::surface() const
{
    return d_ptr->surface;
}

void input_method_popup_surface_v2::set_text_input_rectangle(QRect const& rect)
{
    d_ptr->send<zwp_input_popup_surface_v2_send_text_input_rectangle>(
        rect.x(), rect.y(), rect.width(), rect.height());
}

}
