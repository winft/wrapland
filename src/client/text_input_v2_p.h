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

class Q_DECL_HIDDEN TextInputManagerV2::Private
{
public:
    ~Private() = default;

    void release();
    bool isValid();
    void setupV2(zwp_text_input_manager_v2* manager);
    TextInputV2* createTextInput(Seat* seat, QObject* parent = nullptr);

    operator zwp_text_input_manager_v2*()
    {
        return manager_ptr;
    }
    operator zwp_text_input_manager_v2*() const
    {
        return manager_ptr;
    }

    WaylandPointer<zwp_text_input_manager_v2, zwp_text_input_manager_v2_destroy> manager_ptr;

    EventQueue* queue = nullptr;
};

class Q_DECL_HIDDEN TextInputV2::Private
{
public:
    Private(TextInputV2* q, Seat* seat);
    virtual ~Private() = default;

    void setup(zwp_text_input_v2* ti);

    bool isValid() const;
    void enable(Surface* surface);
    void disable(Surface* surface);
    void showInputPanel();
    void hideInputPanel();
    void setCursorRectangle(QRect const& rect);
    void setPreferredLanguage(QString const& lang);
    void setSurroundingText(QString const& text, quint32 cursor, quint32 anchor);
    void reset();
    void setContentType(ContentHints hint, ContentPurpose purpose);

    WaylandPointer<zwp_text_input_v2, zwp_text_input_v2_destroy> text_input_ptr;

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
                                      char const* text,
                                      char const* commit);
    static void preeditStylingCallback(void* data,
                                       zwp_text_input_v2* zwp_text_input_v2,
                                       uint32_t index,
                                       uint32_t length,
                                       uint32_t style);
    static void
    preeditCursorCallback(void* data, zwp_text_input_v2* zwp_text_input_v2, int32_t index);
    static void
    commitStringCallback(void* data, zwp_text_input_v2* zwp_text_input_v2, char const* text);
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
    languageCallback(void* data, zwp_text_input_v2* zwp_text_input_v2, char const* language);
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

    TextInputV2* q;

    static zwp_text_input_v2_listener const s_listener;
};

}
