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
    cb<getTextInputCallback>,
};

text_input_manager_v2::Private::Private(Display* display, text_input_manager_v2* q)
    : text_input_manager_v2_global(q, display, &zwp_text_input_manager_v2_interface, &s_interface)
{
    create();
}

void text_input_manager_v2::Private::getTextInputCallback(text_input_manager_v2_bind* bind,
                                                          uint32_t id,
                                                          wl_resource* wlSeat)
{
    auto seat = SeatGlobal::handle(wlSeat);

    auto textInput = new text_input_v2(bind->client()->handle(), bind->version(), id);

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
    enableCallback,
    disableCallback,
    showInputPanelCallback,
    hideInputPanelCallback,
    setSurroundingTextCallback,
    setContentTypeCallback,
    setCursorRectangleCallback,
    setPreferredLanguageCallback,
    updateStateCallback,
};

text_input_v2::Private::Private(Client* client, uint32_t version, uint32_t id, text_input_v2* q)
    : Wayland::Resource<text_input_v2>(client,
                                       version,
                                       id,
                                       &zwp_text_input_v2_interface,
                                       &s_interface,
                                       q)
{
}

void text_input_v2::Private::enable(Surface* s)
{
    surface = QPointer<Surface>(s);
    enabled = true;
    Q_EMIT handle()->enabledChanged();
}

void text_input_v2::Private::disable()
{
    surface.clear();
    enabled = false;
    Q_EMIT handle()->enabledChanged();
}

void text_input_v2::Private::sendEnter(Surface* surface, quint32 serial)
{
    if (!surface) {
        return;
    }
    send<zwp_text_input_v2_send_enter>(serial, surface->d_ptr->resource());
}

void text_input_v2::Private::sendLeave(quint32 serial, Surface* surface)
{
    if (!surface) {
        return;
    }
    send<zwp_text_input_v2_send_leave>(serial, surface->d_ptr->resource());
}

void text_input_v2::Private::preEdit(const QByteArray& text, const QByteArray& commit)
{
    send<zwp_text_input_v2_send_preedit_string>(text.constData(), commit.constData());
}

void text_input_v2::Private::commit(const QByteArray& text)
{
    send<zwp_text_input_v2_send_commit_string>(text.constData());
}

void text_input_v2::Private::keysymPressed(quint32 keysym,
                                           [[maybe_unused]] Qt::KeyboardModifiers modifiers)
{
    send<zwp_text_input_v2_send_keysym>(
        seat ? seat->timestamp() : 0, keysym, WL_KEYBOARD_KEY_STATE_PRESSED, 0);
}

void text_input_v2::Private::keysymReleased(quint32 keysym,
                                            [[maybe_unused]] Qt::KeyboardModifiers modifiers)
{
    send<zwp_text_input_v2_send_keysym>(
        seat ? seat->timestamp() : 0, keysym, WL_KEYBOARD_KEY_STATE_RELEASED, 0);
}

void text_input_v2::Private::deleteSurroundingText(quint32 beforeLength, quint32 afterLength)
{
    send<zwp_text_input_v2_send_delete_surrounding_text>(beforeLength, afterLength);
}

void text_input_v2::Private::setCursorPosition(qint32 index, qint32 anchor)
{
    send<zwp_text_input_v2_send_cursor_position>(index, anchor);
}

void text_input_v2::Private::setTextDirection(Qt::LayoutDirection direction)
{
    zwp_text_input_v2_text_direction wlDirection = ZWP_TEXT_INPUT_V2_TEXT_DIRECTION_AUTO;
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
    send<zwp_text_input_v2_send_text_direction>(wlDirection);
}

void text_input_v2::Private::setPreEditCursor(qint32 index)
{
    send<zwp_text_input_v2_send_preedit_cursor>(index);
}

void text_input_v2::Private::sendInputPanelState()
{
    send<zwp_text_input_v2_send_input_panel_state>(
        inputPanelVisible ? ZWP_TEXT_INPUT_V2_INPUT_PANEL_VISIBILITY_VISIBLE
                          : ZWP_TEXT_INPUT_V2_INPUT_PANEL_VISIBILITY_HIDDEN,
        overlappedSurfaceArea.x(),
        overlappedSurfaceArea.y(),
        overlappedSurfaceArea.width(),
        overlappedSurfaceArea.height());
}

void text_input_v2::Private::sendLanguage()
{
    send<zwp_text_input_v2_send_language>(language.constData());
}

void text_input_v2::Private::enableCallback([[maybe_unused]] wl_client* wlClient,
                                            wl_resource* wlResource,
                                            wl_resource* surface)
{
    auto priv = handle(wlResource)->d_ptr;
    priv->enable(Wayland::Resource<Surface>::handle(surface));
}

void text_input_v2::Private::disableCallback([[maybe_unused]] wl_client* wlClient,
                                             wl_resource* wlResource,
                                             [[maybe_unused]] wl_resource* surface)
{
    auto priv = handle(wlResource)->d_ptr;
    priv->disable();
}

void text_input_v2::Private::updateStateCallback([[maybe_unused]] wl_client* wlClient,
                                                 wl_resource* wlResource,
                                                 [[maybe_unused]] uint32_t serial,
                                                 uint32_t reason)
{
    auto priv = handle(wlResource)->d_ptr;

    // TODO(unknown author): use other reason values reason
    if (reason == ZWP_TEXT_INPUT_V2_UPDATE_STATE_RESET) {
        Q_EMIT priv->handle()->requestReset();
    }
}

void text_input_v2::Private::showInputPanelCallback([[maybe_unused]] wl_client* wlClient,
                                                    wl_resource* wlResource)
{
    auto priv = handle(wlResource)->d_ptr;
    Q_EMIT priv->handle()->requestShowInputPanel();
}

void text_input_v2::Private::hideInputPanelCallback([[maybe_unused]] wl_client* wlClient,
                                                    wl_resource* wlResource)
{
    auto priv = handle(wlResource)->d_ptr;
    Q_EMIT priv->handle()->requestHideInputPanel();
}

void text_input_v2::Private::setSurroundingTextCallback([[maybe_unused]] wl_client* wlClient,
                                                        wl_resource* wlResource,
                                                        const char* text,
                                                        int32_t cursor,
                                                        int32_t anchor)
{
    auto priv = handle(wlResource)->d_ptr;

    priv->surroundingText = QByteArray(text);
    priv->surroundingTextCursorPosition = cursor;
    priv->surroundingTextSelectionAnchor = anchor;

    Q_EMIT priv->handle()->surroundingTextChanged();
}

text_input_v2_content_hints convertContentHint(uint32_t hint)
{
    const auto hints = zwp_text_input_v2_content_hint(hint);
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

text_input_v2_content_purpose convertContentPurpose(uint32_t purpose)
{
    const auto wlPurpose = zwp_text_input_v2_content_purpose(purpose);

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

void text_input_v2::Private::setContentTypeCallback([[maybe_unused]] wl_client* wlClient,
                                                    wl_resource* wlResource,
                                                    uint32_t hint,
                                                    uint32_t purpose)
{
    auto priv = handle(wlResource)->d_ptr;
    const auto contentHints = convertContentHint(hint);
    const auto contentPurpose = convertContentPurpose(purpose);

    if (contentHints != priv->contentHints || contentPurpose != priv->contentPurpose) {
        priv->contentHints = contentHints;
        priv->contentPurpose = contentPurpose;
        Q_EMIT priv->handle()->contentTypeChanged();
    }
}

void text_input_v2::Private::setCursorRectangleCallback([[maybe_unused]] wl_client* wlClient,
                                                        wl_resource* wlResource,
                                                        int32_t x,
                                                        int32_t y,
                                                        int32_t width,
                                                        int32_t height)
{
    auto priv = handle(wlResource)->d_ptr;
    const QRect rect = QRect(x, y, width, height);

    if (priv->cursorRectangle != rect) {
        priv->cursorRectangle = rect;
        Q_EMIT priv->handle()->cursorRectangleChanged(priv->cursorRectangle);
    }
}

void text_input_v2::Private::setPreferredLanguageCallback([[maybe_unused]] wl_client* wlClient,
                                                          wl_resource* wlResource,
                                                          const char* language)
{
    auto priv = handle(wlResource)->d_ptr;
    const QByteArray preferredLanguage = QByteArray(language);

    if (priv->preferredLanguage != preferredLanguage) {
        priv->preferredLanguage = preferredLanguage;
        Q_EMIT priv->handle()->preferredLanguageChanged(priv->preferredLanguage);
    }
}

text_input_v2::text_input_v2(Client* client, uint32_t version, uint32_t id)
    : QObject(nullptr)
    , d_ptr(new Private(client, version, id, this))
{
}

QByteArray text_input_v2::preferredLanguage() const
{
    return d_ptr->preferredLanguage;
}

text_input_v2_content_hints text_input_v2::contentHints() const
{
    return d_ptr->contentHints;
}

text_input_v2_content_purpose text_input_v2::contentPurpose() const
{

    return d_ptr->contentPurpose;
}

QByteArray text_input_v2::surroundingText() const
{
    return d_ptr->surroundingText;
}

qint32 text_input_v2::surroundingTextCursorPosition() const
{
    return d_ptr->surroundingTextCursorPosition;
}

qint32 text_input_v2::surroundingTextSelectionAnchor() const
{
    return d_ptr->surroundingTextSelectionAnchor;
}

void text_input_v2::preEdit(const QByteArray& text, const QByteArray& commit)
{
    d_ptr->preEdit(text, commit);
}

void text_input_v2::commit(const QByteArray& text)
{
    d_ptr->commit(text);
}

void text_input_v2::keysymPressed(quint32 keysym, Qt::KeyboardModifiers modifiers)
{
    d_ptr->keysymPressed(keysym, modifiers);
}

void text_input_v2::keysymReleased(quint32 keysym, Qt::KeyboardModifiers modifiers)
{
    d_ptr->keysymReleased(keysym, modifiers);
}

void text_input_v2::deleteSurroundingText(quint32 beforeLength, quint32 afterLength)
{
    d_ptr->deleteSurroundingText(beforeLength, afterLength);
}

void text_input_v2::setCursorPosition(qint32 index, qint32 anchor)
{
    d_ptr->setCursorPosition(index, anchor);
}

void text_input_v2::setTextDirection(Qt::LayoutDirection direction)
{
    d_ptr->setTextDirection(direction);
}

void text_input_v2::setPreEditCursor(qint32 index)
{
    d_ptr->setPreEditCursor(index);
}

void text_input_v2::setInputPanelState(bool visible, const QRect& overlappedSurfaceArea)
{
    if (d_ptr->inputPanelVisible == visible
        && d_ptr->overlappedSurfaceArea == overlappedSurfaceArea) {
        // not changed
        return;
    }
    d_ptr->inputPanelVisible = visible;
    d_ptr->overlappedSurfaceArea = overlappedSurfaceArea;
    d_ptr->sendInputPanelState();
}

void text_input_v2::setLanguage(const QByteArray& languageTag)
{
    if (d_ptr->language == languageTag) {
        // not changed
        return;
    }
    d_ptr->language = languageTag;
    d_ptr->sendLanguage();
}

QPointer<Surface> text_input_v2::surface() const
{
    return d_ptr->surface;
}

QRect text_input_v2::cursorRectangle() const
{
    return d_ptr->cursorRectangle;
}

bool text_input_v2::isEnabled() const
{
    return d_ptr->enabled;
}

Client* text_input_v2::client() const
{
    return d_ptr->client()->handle();
}

}
