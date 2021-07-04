/*
    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2021 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#pragma once

#include "text_input_v2.h"

#include "wayland_pointer_p.h"

#include <QObject>

#include <wayland-text-input-v2-client-protocol.h>

namespace Wrapland::Client
{

class TextInputManagerUnstableV2 : public TextInputManager
{
    Q_OBJECT
public:
    /**
     * Creates a new TextInputManagerUnstableV2.
     * Note: after constructing the TextInputManagerUnstableV2 it is not yet valid and one needs
     * to call setup. In order to get a ready to use TextInputManagerUnstableV2 prefer using
     * Registry::createTextInputManagerUnstableV2.
     **/
    explicit TextInputManagerUnstableV2(QObject* parent = nullptr);
    virtual ~TextInputManagerUnstableV2();

private:
    class Private;
    std::unique_ptr<Private> d;
};

class Q_DECL_HIDDEN TextInputManager::Private
{
public:
    Private() = default;
    virtual ~Private() = default;

    virtual void release() = 0;
    virtual bool isValid() = 0;
    virtual void setupV2(zwp_text_input_manager_v2* textinputmanagerunstablev2)
    {
        Q_UNUSED(textinputmanagerunstablev2)
    }
    virtual TextInput* createTextInput(Seat* seat, QObject* parent = nullptr) = 0;
    virtual operator zwp_text_input_manager_v2*()
    {
        return nullptr;
    }
    virtual operator zwp_text_input_manager_v2*() const
    {
        return nullptr;
    }

    EventQueue* queue = nullptr;
};

class TextInputManagerUnstableV2::Private : public TextInputManager::Private
{
public:
    Private() = default;

    void release() override;
    bool isValid() override;
    void setupV2(zwp_text_input_manager_v2* ti) override;
    TextInput* createTextInput(Seat* seat, QObject* parent = nullptr) override;
    operator zwp_text_input_manager_v2*() override
    {
        return textinputmanagerunstablev2;
    }
    operator zwp_text_input_manager_v2*() const override
    {
        return textinputmanagerunstablev2;
    }

    WaylandPointer<zwp_text_input_manager_v2, zwp_text_input_manager_v2_destroy>
        textinputmanagerunstablev2;
};

class TextInputUnstableV2 : public TextInput
{
    Q_OBJECT
public:
    explicit TextInputUnstableV2(Seat* seat, QObject* parent = nullptr);
    virtual ~TextInputUnstableV2();

    /**
     * Setup this TextInputUnstableV2 to manage the @p textinputunstablev2.
     * When using TextInputManagerUnstableV2::createTextInputUnstableV2 there is no need to call
     * this method.
     **/
    void setup(zwp_text_input_v2* textinputunstablev2);
    /**
     * Releases the zwp_text_input_v2 interface.
     * After the interface has been released the TextInputUnstableV2 instance is no
     * longer valid and can be setup with another zwp_text_input_v2 interface.
     **/
    void release();

    operator zwp_text_input_v2*();
    operator zwp_text_input_v2*() const;

private:
    class Private;
    Private* d_func() const;
};

class Q_DECL_HIDDEN TextInput::Private
{
public:
    Private(Seat* seat);
    virtual ~Private() = default;

    virtual bool isValid() const = 0;
    virtual void enable(Surface* surface) = 0;
    virtual void disable(Surface* surface) = 0;
    virtual void showInputPanel() = 0;
    virtual void hideInputPanel() = 0;
    virtual void setCursorRectangle(const QRect& rect) = 0;
    virtual void setPreferredLanguage(const QString& lang) = 0;
    virtual void setSurroundingText(const QString& text, quint32 cursor, quint32 anchor) = 0;
    virtual void reset() = 0;
    virtual void setContentType(ContentHints hint, ContentPurpose purpose) = 0;

    EventQueue* queue = nullptr;
    Seat* seat;
    Surface* enteredSurface = nullptr;
    quint32 latestSerial = 0;
    bool inputPanelVisible = false;
    Qt::LayoutDirection textDirection = Qt::LayoutDirectionAuto;
    QByteArray language;

    struct PreEdit {
        QByteArray text;
        QByteArray commitText;
        qint32 cursor = 0;
        bool cursorSet = false;
    };
    PreEdit currentPreEdit;
    PreEdit pendingPreEdit;

    struct Commit {
        QByteArray text;
        qint32 cursor = 0;
        qint32 anchor = 0;
        DeleteSurroundingText deleteSurrounding;
    };
    Commit currentCommit;
    Commit pendingCommit;
};

class TextInputUnstableV2::Private : public TextInput::Private
{
public:
    Private(TextInputUnstableV2* q, Seat* seat);

    void setup(zwp_text_input_v2* ti);

    bool isValid() const override;
    void enable(Surface* surface) override;
    void disable(Surface* surface) override;
    void showInputPanel() override;
    void hideInputPanel() override;
    void setCursorRectangle(const QRect& rect) override;
    void setPreferredLanguage(const QString& lang) override;
    void setSurroundingText(const QString& text, quint32 cursor, quint32 anchor) override;
    void reset() override;
    void setContentType(ContentHints hint, ContentPurpose purpose) override;

    WaylandPointer<zwp_text_input_v2, zwp_text_input_v2_destroy> textinputunstablev2;

private:
    static void enterCallback(void* data,
                              zwp_text_input_v2* zwp_text_input_v2,
                              uint32_t serial,
                              wl_surface* surface);
    static void leaveCallback(void* data,
                              zwp_text_input_v2* zwp_text_input_v2,
                              uint32_t serial,
                              wl_surface* surface);
    static void inputPanelStateCallback(void* data,
                                        zwp_text_input_v2* zwp_text_input_v2,
                                        uint32_t state,
                                        int32_t x,
                                        int32_t y,
                                        int32_t width,
                                        int32_t height);
    static void preeditStringCallback(void* data,
                                      zwp_text_input_v2* zwp_text_input_v2,
                                      const char* text,
                                      const char* commit);
    static void preeditStylingCallback(void* data,
                                       zwp_text_input_v2* zwp_text_input_v2,
                                       uint32_t index,
                                       uint32_t length,
                                       uint32_t style);
    static void
    preeditCursorCallback(void* data, zwp_text_input_v2* zwp_text_input_v2, int32_t index);
    static void
    commitStringCallback(void* data, zwp_text_input_v2* zwp_text_input_v2, const char* text);
    static void cursorPositionCallback(void* data,
                                       zwp_text_input_v2* zwp_text_input_v2,
                                       int32_t index,
                                       int32_t anchor);
    static void deleteSurroundingTextCallback(void* data,
                                              zwp_text_input_v2* zwp_text_input_v2,
                                              uint32_t before_length,
                                              uint32_t after_length);
    static void
    modifiersMapCallback(void* data, zwp_text_input_v2* zwp_text_input_v2, wl_array* map);
    static void keysymCallback(void* data,
                               zwp_text_input_v2* zwp_text_input_v2,
                               uint32_t time,
                               uint32_t sym,
                               uint32_t state,
                               uint32_t modifiers);
    static void
    languageCallback(void* data, zwp_text_input_v2* zwp_text_input_v2, const char* language);
    static void
    textDirectionCallback(void* data, zwp_text_input_v2* zwp_text_input_v2, uint32_t direction);
    static void configureSurroundingTextCallback(void* data,
                                                 zwp_text_input_v2* zwp_text_input_v2,
                                                 int32_t before_cursor,
                                                 int32_t after_cursor);
    static void inputMethodChangedCallback(void* data,
                                           zwp_text_input_v2* zwp_text_input_v2,
                                           uint32_t serial,
                                           uint32_t flags);

    TextInputUnstableV2* q;

    static const zwp_text_input_v2_listener s_listener;
};

}
