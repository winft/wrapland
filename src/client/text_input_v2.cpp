/*
    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2021 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#include "text_input_v2_p.h"

#include "event_queue.h"
#include "seat.h"
#include "surface.h"

namespace Wrapland::Client
{

TextInputManagerV2::TextInputManagerV2(QObject* parent)
    : QObject(parent)
    , d_ptr(new Private)
{
}

TextInputManagerV2::~TextInputManagerV2() = default;

void TextInputManagerV2::setup(zwp_text_input_manager_v2* manager)
{
    d_ptr->setupV2(manager);
}

void TextInputManagerV2::release()
{
    d_ptr->release();
}

void TextInputManagerV2::setEventQueue(EventQueue* queue)
{
    d_ptr->queue = queue;
}

EventQueue* TextInputManagerV2::eventQueue()
{
    return d_ptr->queue;
}

TextInputManagerV2::operator zwp_text_input_manager_v2*()
{
    return *(d_ptr.get());
}

TextInputManagerV2::operator zwp_text_input_manager_v2*() const
{
    return *(d_ptr.get());
}

bool TextInputManagerV2::isValid() const
{
    return d_ptr->isValid();
}

TextInputV2* TextInputManagerV2::createTextInput(Seat* seat, QObject* parent)
{
    return d_ptr->createTextInput(seat, parent);
}

void TextInputManagerV2::Private::release()
{
    manager_ptr.release();
}

bool TextInputManagerV2::Private::isValid()
{
    return manager_ptr.isValid();
}

void TextInputManagerV2::Private::setupV2(zwp_text_input_manager_v2* manager)
{
    Q_ASSERT(manager);
    Q_ASSERT(!manager_ptr);
    manager_ptr.setup(manager);
}

TextInputV2* TextInputManagerV2::Private::createTextInput(Seat* seat, QObject* parent)
{
    Q_ASSERT(isValid());
    auto t = new TextInputV2(seat, parent);
    auto w = zwp_text_input_manager_v2_get_text_input(manager_ptr, *seat);
    if (queue) {
        queue->addProxy(w);
    }
    t->setup(w);
    return t;
}

TextInputV2::Private::Private(TextInputV2* q, Seat* seat)
    : q{q}
    , seat(seat)
{
    currentCommit.deleteSurrounding.afterLength = 0;
    currentCommit.deleteSurrounding.beforeLength = 0;
    pendingCommit.deleteSurrounding.afterLength = 0;
    pendingCommit.deleteSurrounding.beforeLength = 0;
}

const zwp_text_input_v2_listener TextInputV2::Private::s_listener = {
    enterCallback,
    leaveCallback,
    inputPanelStateCallback,
    preeditStringCallback,
    preeditStylingCallback,
    preeditCursorCallback,
    commitStringCallback,
    cursorPositionCallback,
    deleteSurroundingTextCallback,
    modifiersMapCallback,
    keysymCallback,
    languageCallback,
    textDirectionCallback,
    configureSurroundingTextCallback,
    inputMethodChangedCallback,
};

void TextInputV2::Private::enterCallback(void* data,
                                         zwp_text_input_v2* zwp_text_input_v2,
                                         uint32_t serial,
                                         wl_surface* surface)
{
    auto t = reinterpret_cast<TextInputV2::Private*>(data);
    Q_ASSERT(t->text_input_ptr == zwp_text_input_v2);
    t->latestSerial = serial;
    t->enteredSurface = Surface::get(surface);
    emit t->q->entered();
}

void TextInputV2::Private::leaveCallback(void* data,
                                         zwp_text_input_v2* zwp_text_input_v2,
                                         uint32_t serial,
                                         wl_surface* surface)
{
    Q_UNUSED(surface)
    auto t = reinterpret_cast<TextInputV2::Private*>(data);
    Q_ASSERT(t->text_input_ptr == zwp_text_input_v2);
    t->enteredSurface = nullptr;
    t->latestSerial = serial;
    emit t->q->left();
}

void TextInputV2::Private::inputPanelStateCallback(void* data,
                                                   zwp_text_input_v2* zwp_text_input_v2,
                                                   uint32_t state,
                                                   int32_t x,
                                                   int32_t y,
                                                   int32_t width,
                                                   int32_t height)
{
    Q_UNUSED(x)
    Q_UNUSED(y)
    Q_UNUSED(width)
    Q_UNUSED(height)
    auto t = reinterpret_cast<TextInputV2::Private*>(data);
    Q_ASSERT(t->text_input_ptr == zwp_text_input_v2);
    // TODO: add rect
    if (t->inputPanelVisible != state) {
        t->inputPanelVisible = state;
        emit t->q->inputPanelStateChanged();
    }
}

void TextInputV2::Private::preeditStringCallback(void* data,
                                                 zwp_text_input_v2* zwp_text_input_v2,
                                                 char const* text,
                                                 char const* commit)
{
    auto t = reinterpret_cast<TextInputV2::Private*>(data);
    Q_ASSERT(t->text_input_ptr == zwp_text_input_v2);
    t->pendingPreEdit.commitText = QByteArray(commit);
    t->pendingPreEdit.text = QByteArray(text);
    if (!t->pendingPreEdit.cursorSet) {
        t->pendingPreEdit.cursor = t->pendingPreEdit.text.length();
    }
    t->currentPreEdit = t->pendingPreEdit;
    t->pendingPreEdit = TextInputV2::Private::PreEdit();
    emit t->q->composingTextChanged();
}

void TextInputV2::Private::preeditStylingCallback(void* data,
                                                  zwp_text_input_v2* zwp_text_input_v2,
                                                  uint32_t index,
                                                  uint32_t length,
                                                  uint32_t style)
{
    Q_UNUSED(index)
    Q_UNUSED(length)
    Q_UNUSED(style)
    // TODO: implement
    auto t = reinterpret_cast<TextInputV2::Private*>(data);
    Q_ASSERT(t->text_input_ptr == zwp_text_input_v2);
}

void TextInputV2::Private::preeditCursorCallback(void* data,
                                                 zwp_text_input_v2* zwp_text_input_v2,
                                                 int32_t index)
{
    auto t = reinterpret_cast<TextInputV2::Private*>(data);
    Q_ASSERT(t->text_input_ptr == zwp_text_input_v2);
    t->pendingPreEdit.cursor = index;
    t->pendingPreEdit.cursorSet = true;
}

void TextInputV2::Private::commitStringCallback(void* data,
                                                zwp_text_input_v2* zwp_text_input_v2,
                                                char const* text)
{
    auto t = reinterpret_cast<TextInputV2::Private*>(data);
    Q_ASSERT(t->text_input_ptr == zwp_text_input_v2);
    t->pendingCommit.text = QByteArray(text);
    t->currentCommit = t->pendingCommit;
    // TODO: what are the proper values it should be set to?
    t->pendingCommit = TextInputV2::Private::Commit();
    t->pendingCommit.deleteSurrounding.beforeLength = 0;
    t->pendingCommit.deleteSurrounding.afterLength = 0;
    emit t->q->committed();
}

void TextInputV2::Private::cursorPositionCallback(void* data,
                                                  zwp_text_input_v2* zwp_text_input_v2,
                                                  int32_t index,
                                                  int32_t anchor)
{
    auto t = reinterpret_cast<TextInputV2::Private*>(data);
    Q_ASSERT(t->text_input_ptr == zwp_text_input_v2);
    t->pendingCommit.cursor = index;
    t->pendingCommit.anchor = anchor;
}

void TextInputV2::Private::deleteSurroundingTextCallback(void* data,
                                                         zwp_text_input_v2* zwp_text_input_v2,
                                                         uint32_t before_length,
                                                         uint32_t after_length)
{
    auto t = reinterpret_cast<TextInputV2::Private*>(data);
    Q_ASSERT(t->text_input_ptr == zwp_text_input_v2);
    t->pendingCommit.deleteSurrounding.beforeLength = before_length;
    t->pendingCommit.deleteSurrounding.afterLength = after_length;
}

void TextInputV2::Private::modifiersMapCallback(void* data,
                                                zwp_text_input_v2* zwp_text_input_v2,
                                                wl_array* map)
{
    // TODO: implement
    Q_UNUSED(map)
    auto t = reinterpret_cast<TextInputV2::Private*>(data);
    Q_ASSERT(t->text_input_ptr == zwp_text_input_v2);
}

void TextInputV2::Private::keysymCallback(void* data,
                                          zwp_text_input_v2* zwp_text_input_v2,
                                          uint32_t time,
                                          uint32_t sym,
                                          uint32_t wlState,
                                          uint32_t modifiers)
{
    // TODO: add support for modifiers
    Q_UNUSED(modifiers)
    auto t = reinterpret_cast<TextInputV2::Private*>(data);
    Q_ASSERT(t->text_input_ptr == zwp_text_input_v2);
    TextInputV2::KeyState state;
    switch (wlState) {
    case WL_KEYBOARD_KEY_STATE_RELEASED:
        state = TextInputV2::KeyState::Released;
        break;
    case WL_KEYBOARD_KEY_STATE_PRESSED:
        state = TextInputV2::KeyState::Pressed;
        break;
    default:
        // invalid
        return;
    }
    emit t->q->keyEvent(sym, state, Qt::KeyboardModifiers(), time);
}

void TextInputV2::Private::languageCallback(void* data,
                                            zwp_text_input_v2* zwp_text_input_v2,
                                            char const* language)
{
    auto t = reinterpret_cast<TextInputV2::Private*>(data);
    Q_ASSERT(t->text_input_ptr == zwp_text_input_v2);
    if (qstrcmp(t->language, language) != 0) {
        t->language = QByteArray(language);
        emit t->q->languageChanged();
    }
}

void TextInputV2::Private::textDirectionCallback(void* data,
                                                 zwp_text_input_v2* zwp_text_input_v2,
                                                 uint32_t wlDirection)
{
    auto t = reinterpret_cast<TextInputV2::Private*>(data);
    Q_ASSERT(t->text_input_ptr == zwp_text_input_v2);
    Qt::LayoutDirection direction;
    switch (wlDirection) {
    case ZWP_TEXT_INPUT_V2_TEXT_DIRECTION_LTR:
        direction = Qt::LeftToRight;
        break;
    case ZWP_TEXT_INPUT_V2_TEXT_DIRECTION_RTL:
        direction = Qt::RightToLeft;
        break;
    case ZWP_TEXT_INPUT_V2_TEXT_DIRECTION_AUTO:
        direction = Qt::LayoutDirectionAuto;
        break;
    default:
        // invalid
        return;
    }
    if (direction != t->textDirection) {
        t->textDirection = direction;
        emit t->q->textDirectionChanged();
    }
}

void TextInputV2::Private::configureSurroundingTextCallback(void* data,
                                                            zwp_text_input_v2* zwp_text_input_v2,
                                                            int32_t before_cursor,
                                                            int32_t after_cursor)
{
    // TODO: implement
    Q_UNUSED(before_cursor)
    Q_UNUSED(after_cursor)
    auto t = reinterpret_cast<TextInputV2::Private*>(data);
    Q_ASSERT(t->text_input_ptr == zwp_text_input_v2);
}

void TextInputV2::Private::inputMethodChangedCallback(void* data,
                                                      zwp_text_input_v2* zwp_text_input_v2,
                                                      uint32_t serial,
                                                      uint32_t flags)
{
    Q_UNUSED(serial)
    Q_UNUSED(flags)
    // TODO: implement
    auto t = reinterpret_cast<TextInputV2::Private*>(data);
    Q_ASSERT(t->text_input_ptr == zwp_text_input_v2);
}

void TextInputV2::Private::setup(zwp_text_input_v2* text_input)
{
    Q_ASSERT(text_input);
    Q_ASSERT(!text_input_ptr);
    text_input_ptr.setup(text_input);
    zwp_text_input_v2_add_listener(text_input, &s_listener, this);
}

bool TextInputV2::Private::isValid() const
{
    return text_input_ptr.isValid();
}

void TextInputV2::Private::enable(Surface* surface)
{
    zwp_text_input_v2_enable(text_input_ptr, *surface);
}

void TextInputV2::Private::disable(Surface* surface)
{
    zwp_text_input_v2_disable(text_input_ptr, *surface);
}

void TextInputV2::Private::showInputPanel()
{
    zwp_text_input_v2_show_input_panel(text_input_ptr);
}

void TextInputV2::Private::hideInputPanel()
{
    zwp_text_input_v2_hide_input_panel(text_input_ptr);
}

void TextInputV2::Private::setCursorRectangle(QRect const& rect)
{
    zwp_text_input_v2_set_cursor_rectangle(
        text_input_ptr, rect.x(), rect.y(), rect.width(), rect.height());
}

void TextInputV2::Private::setPreferredLanguage(QString const& lang)
{
    zwp_text_input_v2_set_preferred_language(text_input_ptr, lang.toUtf8().constData());
}

void TextInputV2::Private::setSurroundingText(QString const& text, quint32 cursor, quint32 anchor)
{
    zwp_text_input_v2_set_surrounding_text(text_input_ptr,
                                           text.toUtf8().constData(),
                                           text.leftRef(cursor).toUtf8().length(),
                                           text.leftRef(anchor).toUtf8().length());
}

void TextInputV2::Private::reset()
{
    zwp_text_input_v2_update_state(
        text_input_ptr, latestSerial, ZWP_TEXT_INPUT_V2_UPDATE_STATE_RESET);
}

void TextInputV2::Private::setContentType(ContentHints hints, ContentPurpose purpose)
{
    uint32_t wlHints = 0;
    uint32_t wlPurpose = 0;
    if (hints.testFlag(ContentHint::AutoCompletion)) {
        wlHints |= ZWP_TEXT_INPUT_V2_CONTENT_HINT_AUTO_COMPLETION;
    }
    if (hints.testFlag(ContentHint::AutoCorrection)) {
        wlHints |= ZWP_TEXT_INPUT_V2_CONTENT_HINT_AUTO_CORRECTION;
    }
    if (hints.testFlag(ContentHint::AutoCapitalization)) {
        wlHints |= ZWP_TEXT_INPUT_V2_CONTENT_HINT_AUTO_CAPITALIZATION;
    }
    if (hints.testFlag(ContentHint::LowerCase)) {
        wlHints |= ZWP_TEXT_INPUT_V2_CONTENT_HINT_LOWERCASE;
    }
    if (hints.testFlag(ContentHint::UpperCase)) {
        wlHints |= ZWP_TEXT_INPUT_V2_CONTENT_HINT_UPPERCASE;
    }
    if (hints.testFlag(ContentHint::TitleCase)) {
        wlHints |= ZWP_TEXT_INPUT_V2_CONTENT_HINT_TITLECASE;
    }
    if (hints.testFlag(ContentHint::HiddenText)) {
        wlHints |= ZWP_TEXT_INPUT_V2_CONTENT_HINT_HIDDEN_TEXT;
    }
    if (hints.testFlag(ContentHint::SensitiveData)) {
        wlHints |= ZWP_TEXT_INPUT_V2_CONTENT_HINT_SENSITIVE_DATA;
    }
    if (hints.testFlag(ContentHint::Latin)) {
        wlHints |= ZWP_TEXT_INPUT_V2_CONTENT_HINT_LATIN;
    }
    if (hints.testFlag(ContentHint::MultiLine)) {
        wlHints |= ZWP_TEXT_INPUT_V2_CONTENT_HINT_MULTILINE;
    }
    switch (purpose) {
    case ContentPurpose::Normal:
        wlPurpose = ZWP_TEXT_INPUT_V2_CONTENT_PURPOSE_NORMAL;
        break;
    case ContentPurpose::Alpha:
        wlPurpose = ZWP_TEXT_INPUT_V2_CONTENT_PURPOSE_ALPHA;
        break;
    case ContentPurpose::Digits:
        wlPurpose = ZWP_TEXT_INPUT_V2_CONTENT_PURPOSE_DIGITS;
        break;
    case ContentPurpose::Number:
        wlPurpose = ZWP_TEXT_INPUT_V2_CONTENT_PURPOSE_NUMBER;
        break;
    case ContentPurpose::Phone:
        wlPurpose = ZWP_TEXT_INPUT_V2_CONTENT_PURPOSE_PHONE;
        break;
    case ContentPurpose::Url:
        wlPurpose = ZWP_TEXT_INPUT_V2_CONTENT_PURPOSE_URL;
        break;
    case ContentPurpose::Email:
        wlPurpose = ZWP_TEXT_INPUT_V2_CONTENT_PURPOSE_EMAIL;
        break;
    case ContentPurpose::Name:
        wlPurpose = ZWP_TEXT_INPUT_V2_CONTENT_PURPOSE_NAME;
        break;
    case ContentPurpose::Password:
        wlPurpose = ZWP_TEXT_INPUT_V2_CONTENT_PURPOSE_PASSWORD;
        break;
    case ContentPurpose::Date:
        wlPurpose = ZWP_TEXT_INPUT_V2_CONTENT_PURPOSE_DATE;
        break;
    case ContentPurpose::Time:
        wlPurpose = ZWP_TEXT_INPUT_V2_CONTENT_PURPOSE_TIME;
        break;
    case ContentPurpose::DateTime:
        wlPurpose = ZWP_TEXT_INPUT_V2_CONTENT_PURPOSE_DATETIME;
        break;
    case ContentPurpose::Terminal:
        wlPurpose = ZWP_TEXT_INPUT_V2_CONTENT_PURPOSE_TERMINAL;
        break;
    }
    zwp_text_input_v2_set_content_type(text_input_ptr, wlHints, wlPurpose);
}

TextInputV2::TextInputV2(Seat* seat, QObject* parent)
    : QObject(parent)
    , d_ptr(new Private(this, seat))
{
}

TextInputV2::~TextInputV2()
{
    release();
}

void TextInputV2::setEventQueue(EventQueue* queue)
{
    d_ptr->queue = queue;
}

EventQueue* TextInputV2::eventQueue() const
{
    return d_ptr->queue;
}

bool TextInputV2::isValid() const
{
    return d_ptr->isValid();
}

Surface* TextInputV2::enteredSurface() const
{
    return d_ptr->enteredSurface;
}

bool TextInputV2::isInputPanelVisible() const
{
    return d_ptr->inputPanelVisible;
}

void TextInputV2::enable(Surface* surface)
{
    d_ptr->enable(surface);
}

void TextInputV2::disable(Surface* surface)
{
    d_ptr->disable(surface);
}

void TextInputV2::showInputPanel()
{
    d_ptr->showInputPanel();
}

void TextInputV2::hideInputPanel()
{
    d_ptr->hideInputPanel();
}

void TextInputV2::reset()
{
    d_ptr->reset();
}

void TextInputV2::setSurroundingText(QString const& text, quint32 cursor, quint32 anchor)
{
    d_ptr->setSurroundingText(text, cursor, anchor);
}

void TextInputV2::setContentType(ContentHints hint, ContentPurpose purpose)
{
    d_ptr->setContentType(hint, purpose);
}

void TextInputV2::setCursorRectangle(QRect const& rect)
{
    d_ptr->setCursorRectangle(rect);
}

void TextInputV2::setPreferredLanguage(QString const& language)
{
    d_ptr->setPreferredLanguage(language);
}

Qt::LayoutDirection TextInputV2::textDirection() const
{
    return d_ptr->textDirection;
}

QByteArray TextInputV2::language() const
{
    return d_ptr->language;
}

qint32 TextInputV2::composingTextCursorPosition() const
{
    return d_ptr->currentPreEdit.cursor;
}

QByteArray TextInputV2::composingText() const
{
    return d_ptr->currentPreEdit.text;
}

QByteArray TextInputV2::composingFallbackText() const
{
    return d_ptr->currentPreEdit.commitText;
}

qint32 TextInputV2::anchorPosition() const
{
    return d_ptr->currentCommit.anchor;
}

qint32 TextInputV2::cursorPosition() const
{
    return d_ptr->currentCommit.cursor;
}

TextInputV2::DeleteSurroundingText TextInputV2::deleteSurroundingText() const
{
    return d_ptr->currentCommit.deleteSurrounding;
}

QByteArray TextInputV2::commitText() const
{
    return d_ptr->currentCommit.text;
}

void TextInputV2::setup(zwp_text_input_v2* text_input)
{
    d_ptr->setup(text_input);
}

void TextInputV2::release()
{
    d_ptr->text_input_ptr.release();
}

TextInputV2::operator zwp_text_input_v2*()
{
    return d_ptr->text_input_ptr;
}

TextInputV2::operator zwp_text_input_v2*() const
{
    return d_ptr->text_input_ptr;
}

}
