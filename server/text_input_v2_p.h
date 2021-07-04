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
#pragma once
#include "text_input_v2.h"

#include <QPointer>
#include <QRect>
#include <QVector>
#include <vector>

#include <wayland-text-input-unstable-v2-server-protocol.h>

namespace Wrapland::Server
{

constexpr uint32_t TextInputManagerV2Version = 1;
using TextInputManagerV2Global = Wayland::Global<TextInputManagerV2, TextInputManagerV2Version>;
using TextInputManagerV2Bind = Wayland::Bind<TextInputManagerV2Global>;

class TextInputManagerV2::Private : public TextInputManagerV2Global
{
public:
    Private(Display* display, TextInputManagerV2* q);

private:
    static void
    getTextInputCallback(TextInputManagerV2Bind* bind, uint32_t id, wl_resource* wlSeat);

    static const struct zwp_text_input_manager_v2_interface s_interface;
};

class TextInputV2::Private : public Wayland::Resource<TextInputV2>
{
public:
    Private(Client* client, uint32_t version, uint32_t id, TextInputV2* q);

    void sendEnter(Surface* surface, quint32 serial);
    void sendLeave(quint32 serial, Surface* surface);
    void preEdit(const QByteArray& text, const QByteArray& commit);
    void commit(const QByteArray& text);
    void deleteSurroundingText(quint32 beforeLength, quint32 afterLength);
    void setTextDirection(Qt::LayoutDirection direction);
    void setPreEditCursor(qint32 index);
    void setCursorPosition(qint32 index, qint32 anchor);
    void keysymPressed(quint32 keysym, Qt::KeyboardModifiers modifiers);
    void keysymReleased(quint32 keysym, Qt::KeyboardModifiers modifiers);

    void sendInputPanelState();
    void sendLanguage();

    QByteArray preferredLanguage;
    QRect cursorRectangle;
    TextInputV2::ContentHints contentHints = TextInputV2::ContentHint::None;
    TextInputV2::ContentPurpose contentPurpose = TextInputV2::ContentPurpose::Normal;
    Seat* seat = nullptr;
    QPointer<Surface> surface;
    bool enabled = false;
    QByteArray surroundingText;
    qint32 surroundingTextCursorPosition = 0;
    qint32 surroundingTextSelectionAnchor = 0;
    bool inputPanelVisible = false;
    QRect overlappedSurfaceArea;
    QByteArray language;

private:
    static const struct zwp_text_input_v2_interface s_interface;

    static void enableCallback(wl_client* wlClient, wl_resource* wlResource, wl_resource* surface);
    static void disableCallback(wl_client* wlClient, wl_resource* wlResource, wl_resource* surface);
    static void updateStateCallback(wl_client* wlClient,
                                    wl_resource* wlResource,
                                    uint32_t serial,
                                    uint32_t reason);
    static void showInputPanelCallback(wl_client* wlClient, wl_resource* wlResource);
    static void hideInputPanelCallback(wl_client* wlClient, wl_resource* wlResource);
    static void setSurroundingTextCallback(wl_client* wlClient,
                                           wl_resource* wlResource,
                                           const char* text,
                                           int32_t cursor,
                                           int32_t anchor);
    static void setContentTypeCallback(wl_client* wlClient,
                                       wl_resource* wlResource,
                                       uint32_t hint,
                                       uint32_t purpose);
    static void setCursorRectangleCallback(wl_client* wlClient,
                                           wl_resource* wlResource,
                                           int32_t x,
                                           int32_t y,
                                           int32_t width,
                                           int32_t height);
    static void setPreferredLanguageCallback(wl_client* wlClient,
                                             wl_resource* wlResource,
                                             const char* language);

    void enable(Surface* s);
    void disable();
};

}
