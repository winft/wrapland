/*
    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2021 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#include "textinput_p.h"

namespace Wrapland::Client
{

TextInputManager::TextInputManager(Private* p, QObject* parent)
    : QObject(parent)
    , d(p)
{
}

TextInputManager::~TextInputManager() = default;

void TextInputManager::setup(zwp_text_input_manager_v2* textinputmanagerunstablev2)
{
    d->setupV2(textinputmanagerunstablev2);
}

void TextInputManager::release()
{
    d->release();
}

void TextInputManager::setEventQueue(EventQueue* queue)
{
    d->queue = queue;
}

EventQueue* TextInputManager::eventQueue()
{
    return d->queue;
}

TextInputManager::operator zwp_text_input_manager_v2*()
{
    return *(d.get());
}

TextInputManager::operator zwp_text_input_manager_v2*() const
{
    return *(d.get());
}

bool TextInputManager::isValid() const
{
    return d->isValid();
}

TextInput* TextInputManager::createTextInput(Seat* seat, QObject* parent)
{
    return d->createTextInput(seat, parent);
}

TextInput::Private::Private(Seat* seat)
    : seat(seat)
{
    currentCommit.deleteSurrounding.afterLength = 0;
    currentCommit.deleteSurrounding.beforeLength = 0;
    pendingCommit.deleteSurrounding.afterLength = 0;
    pendingCommit.deleteSurrounding.beforeLength = 0;
}

TextInput::TextInput(Private* p, QObject* parent)
    : QObject(parent)
    , d(p)
{
}

TextInput::~TextInput() = default;

void TextInput::setEventQueue(EventQueue* queue)
{
    d->queue = queue;
}

EventQueue* TextInput::eventQueue() const
{
    return d->queue;
}

bool TextInput::isValid() const
{
    return d->isValid();
}

Surface* TextInput::enteredSurface() const
{
    return d->enteredSurface;
}

bool TextInput::isInputPanelVisible() const
{
    return d->inputPanelVisible;
}

void TextInput::enable(Surface* surface)
{
    d->enable(surface);
}

void TextInput::disable(Surface* surface)
{
    d->disable(surface);
}

void TextInput::showInputPanel()
{
    d->showInputPanel();
}

void TextInput::hideInputPanel()
{
    d->hideInputPanel();
}

void TextInput::reset()
{
    d->reset();
}

void TextInput::setSurroundingText(const QString& text, quint32 cursor, quint32 anchor)
{
    d->setSurroundingText(text, cursor, anchor);
}

void TextInput::setContentType(ContentHints hint, ContentPurpose purpose)
{
    d->setContentType(hint, purpose);
}

void TextInput::setCursorRectangle(const QRect& rect)
{
    d->setCursorRectangle(rect);
}

void TextInput::setPreferredLanguage(const QString& language)
{
    d->setPreferredLanguage(language);
}

Qt::LayoutDirection TextInput::textDirection() const
{
    return d->textDirection;
}

QByteArray TextInput::language() const
{
    return d->language;
}

qint32 TextInput::composingTextCursorPosition() const
{
    return d->currentPreEdit.cursor;
}

QByteArray TextInput::composingText() const
{
    return d->currentPreEdit.text;
}

QByteArray TextInput::composingFallbackText() const
{
    return d->currentPreEdit.commitText;
}

qint32 TextInput::anchorPosition() const
{
    return d->currentCommit.anchor;
}

qint32 TextInput::cursorPosition() const
{
    return d->currentCommit.cursor;
}

TextInput::DeleteSurroundingText TextInput::deleteSurroundingText() const
{
    return d->currentCommit.deleteSurrounding;
}

QByteArray TextInput::commitText() const
{
    return d->currentCommit.text;
}

}
