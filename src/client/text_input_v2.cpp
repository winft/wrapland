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

TextInputManager::TextInputManager(Private* p, QObject* parent)
    : QObject(parent)
    , d_ptr(p)
{
}

TextInputManager::~TextInputManager() = default;

void TextInputManager::setup(zwp_text_input_manager_v2* textinputmanagerunstablev2)
{
    d_ptr->setupV2(textinputmanagerunstablev2);
}

void TextInputManager::release()
{
    d_ptr->release();
}

void TextInputManager::setEventQueue(EventQueue* queue)
{
    d_ptr->queue = queue;
}

EventQueue* TextInputManager::eventQueue()
{
    return d_ptr->queue;
}

TextInputManager::operator zwp_text_input_manager_v2*()
{
    return *(d_ptr.get());
}

TextInputManager::operator zwp_text_input_manager_v2*() const
{
    return *(d_ptr.get());
}

bool TextInputManager::isValid() const
{
    return d_ptr->isValid();
}

TextInput* TextInputManager::createTextInput(Seat* seat, QObject* parent)
{
    return d_ptr->createTextInput(seat, parent);
}

void TextInputManagerUnstableV2::Private::release()
{
    textinputmanagerunstablev2.release();
}

bool TextInputManagerUnstableV2::Private::isValid()
{
    return textinputmanagerunstablev2.isValid();
}

TextInputManagerUnstableV2::TextInputManagerUnstableV2(QObject* parent)
    : TextInputManager(new Private, parent)
{
}

TextInputManagerUnstableV2::~TextInputManagerUnstableV2() = default;

void TextInputManagerUnstableV2::Private::setupV2(zwp_text_input_manager_v2* ti)
{
    Q_ASSERT(ti);
    Q_ASSERT(!textinputmanagerunstablev2);
    textinputmanagerunstablev2.setup(ti);
}

TextInput* TextInputManagerUnstableV2::Private::createTextInput(Seat* seat, QObject* parent)
{
    Q_ASSERT(isValid());
    TextInputUnstableV2* t = new TextInputUnstableV2(seat, parent);
    auto w = zwp_text_input_manager_v2_get_text_input(textinputmanagerunstablev2, *seat);
    if (queue) {
        queue->addProxy(w);
    }
    t->setup(w);
    return t;
}

TextInput::Private::Private(Seat* seat)
    : seat(seat)
{
    currentCommit.deleteSurrounding.afterLength = 0;
    currentCommit.deleteSurrounding.beforeLength = 0;
    pendingCommit.deleteSurrounding.afterLength = 0;
    pendingCommit.deleteSurrounding.beforeLength = 0;
}

const zwp_text_input_v2_listener TextInputUnstableV2::Private::s_listener = {
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

void TextInputUnstableV2::Private::enterCallback(void* data,
                                                 zwp_text_input_v2* zwp_text_input_v2,
                                                 uint32_t serial,
                                                 wl_surface* surface)
{
    auto t = reinterpret_cast<TextInputUnstableV2::Private*>(data);
    Q_ASSERT(t->textinputunstablev2 == zwp_text_input_v2);
    t->latestSerial = serial;
    t->enteredSurface = Surface::get(surface);
    emit t->q->entered();
}

void TextInputUnstableV2::Private::leaveCallback(void* data,
                                                 zwp_text_input_v2* zwp_text_input_v2,
                                                 uint32_t serial,
                                                 wl_surface* surface)
{
    Q_UNUSED(surface)
    auto t = reinterpret_cast<TextInputUnstableV2::Private*>(data);
    Q_ASSERT(t->textinputunstablev2 == zwp_text_input_v2);
    t->enteredSurface = nullptr;
    t->latestSerial = serial;
    emit t->q->left();
}

void TextInputUnstableV2::Private::inputPanelStateCallback(void* data,
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
    auto t = reinterpret_cast<TextInputUnstableV2::Private*>(data);
    Q_ASSERT(t->textinputunstablev2 == zwp_text_input_v2);
    // TODO: add rect
    if (t->inputPanelVisible != state) {
        t->inputPanelVisible = state;
        emit t->q->inputPanelStateChanged();
    }
}

void TextInputUnstableV2::Private::preeditStringCallback(void* data,
                                                         zwp_text_input_v2* zwp_text_input_v2,
                                                         const char* text,
                                                         const char* commit)
{
    auto t = reinterpret_cast<TextInputUnstableV2::Private*>(data);
    Q_ASSERT(t->textinputunstablev2 == zwp_text_input_v2);
    t->pendingPreEdit.commitText = QByteArray(commit);
    t->pendingPreEdit.text = QByteArray(text);
    if (!t->pendingPreEdit.cursorSet) {
        t->pendingPreEdit.cursor = t->pendingPreEdit.text.length();
    }
    t->currentPreEdit = t->pendingPreEdit;
    t->pendingPreEdit = TextInput::Private::PreEdit();
    emit t->q->composingTextChanged();
}

void TextInputUnstableV2::Private::preeditStylingCallback(void* data,
                                                          zwp_text_input_v2* zwp_text_input_v2,
                                                          uint32_t index,
                                                          uint32_t length,
                                                          uint32_t style)
{
    Q_UNUSED(index)
    Q_UNUSED(length)
    Q_UNUSED(style)
    // TODO: implement
    auto t = reinterpret_cast<TextInputUnstableV2::Private*>(data);
    Q_ASSERT(t->textinputunstablev2 == zwp_text_input_v2);
}

void TextInputUnstableV2::Private::preeditCursorCallback(void* data,
                                                         zwp_text_input_v2* zwp_text_input_v2,
                                                         int32_t index)
{
    auto t = reinterpret_cast<TextInputUnstableV2::Private*>(data);
    Q_ASSERT(t->textinputunstablev2 == zwp_text_input_v2);
    t->pendingPreEdit.cursor = index;
    t->pendingPreEdit.cursorSet = true;
}

void TextInputUnstableV2::Private::commitStringCallback(void* data,
                                                        zwp_text_input_v2* zwp_text_input_v2,
                                                        const char* text)
{
    auto t = reinterpret_cast<TextInputUnstableV2::Private*>(data);
    Q_ASSERT(t->textinputunstablev2 == zwp_text_input_v2);
    t->pendingCommit.text = QByteArray(text);
    t->currentCommit = t->pendingCommit;
    // TODO: what are the proper values it should be set to?
    t->pendingCommit = TextInput::Private::Commit();
    t->pendingCommit.deleteSurrounding.beforeLength = 0;
    t->pendingCommit.deleteSurrounding.afterLength = 0;
    emit t->q->committed();
}

void TextInputUnstableV2::Private::cursorPositionCallback(void* data,
                                                          zwp_text_input_v2* zwp_text_input_v2,
                                                          int32_t index,
                                                          int32_t anchor)
{
    auto t = reinterpret_cast<TextInputUnstableV2::Private*>(data);
    Q_ASSERT(t->textinputunstablev2 == zwp_text_input_v2);
    t->pendingCommit.cursor = index;
    t->pendingCommit.anchor = anchor;
}

void TextInputUnstableV2::Private::deleteSurroundingTextCallback(
    void* data,
    zwp_text_input_v2* zwp_text_input_v2,
    uint32_t before_length,
    uint32_t after_length)
{
    auto t = reinterpret_cast<TextInputUnstableV2::Private*>(data);
    Q_ASSERT(t->textinputunstablev2 == zwp_text_input_v2);
    t->pendingCommit.deleteSurrounding.beforeLength = before_length;
    t->pendingCommit.deleteSurrounding.afterLength = after_length;
}

void TextInputUnstableV2::Private::modifiersMapCallback(void* data,
                                                        zwp_text_input_v2* zwp_text_input_v2,
                                                        wl_array* map)
{
    // TODO: implement
    Q_UNUSED(map)
    auto t = reinterpret_cast<TextInputUnstableV2::Private*>(data);
    Q_ASSERT(t->textinputunstablev2 == zwp_text_input_v2);
}

void TextInputUnstableV2::Private::keysymCallback(void* data,
                                                  zwp_text_input_v2* zwp_text_input_v2,
                                                  uint32_t time,
                                                  uint32_t sym,
                                                  uint32_t wlState,
                                                  uint32_t modifiers)
{
    // TODO: add support for modifiers
    Q_UNUSED(modifiers)
    auto t = reinterpret_cast<TextInputUnstableV2::Private*>(data);
    Q_ASSERT(t->textinputunstablev2 == zwp_text_input_v2);
    TextInput::KeyState state;
    switch (wlState) {
    case WL_KEYBOARD_KEY_STATE_RELEASED:
        state = TextInput::KeyState::Released;
        break;
    case WL_KEYBOARD_KEY_STATE_PRESSED:
        state = TextInput::KeyState::Pressed;
        break;
    default:
        // invalid
        return;
    }
    emit t->q->keyEvent(sym, state, Qt::KeyboardModifiers(), time);
}

void TextInputUnstableV2::Private::languageCallback(void* data,
                                                    zwp_text_input_v2* zwp_text_input_v2,
                                                    const char* language)
{
    auto t = reinterpret_cast<TextInputUnstableV2::Private*>(data);
    Q_ASSERT(t->textinputunstablev2 == zwp_text_input_v2);
    if (qstrcmp(t->language, language) != 0) {
        t->language = QByteArray(language);
        emit t->q->languageChanged();
    }
}

void TextInputUnstableV2::Private::textDirectionCallback(void* data,
                                                         zwp_text_input_v2* zwp_text_input_v2,
                                                         uint32_t wlDirection)
{
    auto t = reinterpret_cast<TextInputUnstableV2::Private*>(data);
    Q_ASSERT(t->textinputunstablev2 == zwp_text_input_v2);
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

void TextInputUnstableV2::Private::configureSurroundingTextCallback(
    void* data,
    zwp_text_input_v2* zwp_text_input_v2,
    int32_t before_cursor,
    int32_t after_cursor)
{
    // TODO: implement
    Q_UNUSED(before_cursor)
    Q_UNUSED(after_cursor)
    auto t = reinterpret_cast<TextInputUnstableV2::Private*>(data);
    Q_ASSERT(t->textinputunstablev2 == zwp_text_input_v2);
}

void TextInputUnstableV2::Private::inputMethodChangedCallback(void* data,
                                                              zwp_text_input_v2* zwp_text_input_v2,
                                                              uint32_t serial,
                                                              uint32_t flags)
{
    Q_UNUSED(serial)
    Q_UNUSED(flags)
    // TODO: implement
    auto t = reinterpret_cast<TextInputUnstableV2::Private*>(data);
    Q_ASSERT(t->textinputunstablev2 == zwp_text_input_v2);
}

TextInputUnstableV2::Private::Private(TextInputUnstableV2* q, Seat* seat)
    : TextInput::Private(seat)
    , q(q)
{
}

void TextInputUnstableV2::Private::setup(zwp_text_input_v2* ti)
{
    Q_ASSERT(ti);
    Q_ASSERT(!textinputunstablev2);
    textinputunstablev2.setup(ti);
    zwp_text_input_v2_add_listener(ti, &s_listener, this);
}

bool TextInputUnstableV2::Private::isValid() const
{
    return textinputunstablev2.isValid();
}

void TextInputUnstableV2::Private::enable(Surface* surface)
{
    zwp_text_input_v2_enable(textinputunstablev2, *surface);
}

void TextInputUnstableV2::Private::disable(Surface* surface)
{
    zwp_text_input_v2_disable(textinputunstablev2, *surface);
}

void TextInputUnstableV2::Private::showInputPanel()
{
    zwp_text_input_v2_show_input_panel(textinputunstablev2);
}

void TextInputUnstableV2::Private::hideInputPanel()
{
    zwp_text_input_v2_hide_input_panel(textinputunstablev2);
}

void TextInputUnstableV2::Private::setCursorRectangle(const QRect& rect)
{
    zwp_text_input_v2_set_cursor_rectangle(
        textinputunstablev2, rect.x(), rect.y(), rect.width(), rect.height());
}

void TextInputUnstableV2::Private::setPreferredLanguage(const QString& lang)
{
    zwp_text_input_v2_set_preferred_language(textinputunstablev2, lang.toUtf8().constData());
}

void TextInputUnstableV2::Private::setSurroundingText(const QString& text,
                                                      quint32 cursor,
                                                      quint32 anchor)
{
    zwp_text_input_v2_set_surrounding_text(textinputunstablev2,
                                           text.toUtf8().constData(),
                                           text.leftRef(cursor).toUtf8().length(),
                                           text.leftRef(anchor).toUtf8().length());
}

void TextInputUnstableV2::Private::reset()
{
    zwp_text_input_v2_update_state(
        textinputunstablev2, latestSerial, ZWP_TEXT_INPUT_V2_UPDATE_STATE_RESET);
}

void TextInputUnstableV2::Private::setContentType(ContentHints hints, ContentPurpose purpose)
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
    zwp_text_input_v2_set_content_type(textinputunstablev2, wlHints, wlPurpose);
}

TextInput::TextInput(Private* p, QObject* parent)
    : QObject(parent)
    , d_ptr(p)
{
}

TextInput::~TextInput() = default;

void TextInput::setEventQueue(EventQueue* queue)
{
    d_ptr->queue = queue;
}

EventQueue* TextInput::eventQueue() const
{
    return d_ptr->queue;
}

bool TextInput::isValid() const
{
    return d_ptr->isValid();
}

Surface* TextInput::enteredSurface() const
{
    return d_ptr->enteredSurface;
}

bool TextInput::isInputPanelVisible() const
{
    return d_ptr->inputPanelVisible;
}

void TextInput::enable(Surface* surface)
{
    d_ptr->enable(surface);
}

void TextInput::disable(Surface* surface)
{
    d_ptr->disable(surface);
}

void TextInput::showInputPanel()
{
    d_ptr->showInputPanel();
}

void TextInput::hideInputPanel()
{
    d_ptr->hideInputPanel();
}

void TextInput::reset()
{
    d_ptr->reset();
}

void TextInput::setSurroundingText(const QString& text, quint32 cursor, quint32 anchor)
{
    d_ptr->setSurroundingText(text, cursor, anchor);
}

void TextInput::setContentType(ContentHints hint, ContentPurpose purpose)
{
    d_ptr->setContentType(hint, purpose);
}

void TextInput::setCursorRectangle(const QRect& rect)
{
    d_ptr->setCursorRectangle(rect);
}

void TextInput::setPreferredLanguage(const QString& language)
{
    d_ptr->setPreferredLanguage(language);
}

Qt::LayoutDirection TextInput::textDirection() const
{
    return d_ptr->textDirection;
}

QByteArray TextInput::language() const
{
    return d_ptr->language;
}

qint32 TextInput::composingTextCursorPosition() const
{
    return d_ptr->currentPreEdit.cursor;
}

QByteArray TextInput::composingText() const
{
    return d_ptr->currentPreEdit.text;
}

QByteArray TextInput::composingFallbackText() const
{
    return d_ptr->currentPreEdit.commitText;
}

qint32 TextInput::anchorPosition() const
{
    return d_ptr->currentCommit.anchor;
}

qint32 TextInput::cursorPosition() const
{
    return d_ptr->currentCommit.cursor;
}

TextInput::DeleteSurroundingText TextInput::deleteSurroundingText() const
{
    return d_ptr->currentCommit.deleteSurrounding;
}

QByteArray TextInput::commitText() const
{
    return d_ptr->currentCommit.text;
}

TextInputUnstableV2::TextInputUnstableV2(Seat* seat, QObject* parent)
    : TextInput(new Private(this, seat), parent)
{
}

TextInputUnstableV2::~TextInputUnstableV2()
{
    release();
}

TextInputUnstableV2::Private* TextInputUnstableV2::d_func() const
{
    return reinterpret_cast<Private*>(d_ptr.get());
}

void TextInputUnstableV2::setup(zwp_text_input_v2* textinputunstablev2)
{
    Q_D();
    d->setup(textinputunstablev2);
}

void TextInputUnstableV2::release()
{
    Q_D();
    d->textinputunstablev2.release();
}

TextInputUnstableV2::operator zwp_text_input_v2*()
{
    Q_D();
    return d->textinputunstablev2;
}

TextInputUnstableV2::operator zwp_text_input_v2*() const
{
    Q_D();
    return d->textinputunstablev2;
}

}
