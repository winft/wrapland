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

#include <QObject>

#include <Wrapland/Server/wraplandserver_export.h>

#include <memory>

namespace Wrapland::Server
{
class Display;
class Client;
class Seat;
class Surface;

enum class text_input_v2_content_hint : uint32_t {
    none = 0,
    completion = 1 << 0,
    correction = 1 << 1,
    capitalization = 1 << 2,
    lowercase = 1 << 3,
    uppercase = 1 << 4,
    titlecase = 1 << 5,
    hidden_text = 1 << 6,
    sensitive_data = 1 << 7,
    latin = 1 << 8,
    multiline = 1 << 9,
};
Q_DECLARE_FLAGS(text_input_v2_content_hints, text_input_v2_content_hint)

enum class text_input_v2_content_purpose : uint32_t {
    normal,
    alpha,
    digits,
    number,
    phone,
    url,
    email,
    name,
    password,
    date,
    time,
    datetime,
    terminal,
};

struct text_input_v2_state {
    bool enabled{false};
    std::string preferred_language;
    QRect cursor_rectangle;

    struct {
        text_input_v2_content_hints hints{text_input_v2_content_hint::none};
        text_input_v2_content_purpose purpose{text_input_v2_content_purpose::normal};
    } content;

    struct {
        std::string data;
        int32_t cursor_position{0};
        int32_t selection_anchor{0};
    } surrounding_text;
};

class WRAPLANDSERVER_EXPORT text_input_manager_v2 : public QObject
{
    Q_OBJECT
public:
    explicit text_input_manager_v2(Display* display);
    ~text_input_manager_v2() override;

private:
    class Private;
    std::unique_ptr<Private> d_ptr;
};

class WRAPLANDSERVER_EXPORT text_input_v2 : public QObject
{
    Q_OBJECT
public:
    text_input_v2_state const& state() const;

    Client* client() const;
    QPointer<Surface> surface() const;

    void set_preedit_string(std::string const& text, std::string const& commit);
    void commit(std::string const& text);
    void set_preedit_cursor(int32_t index);
    void delete_surrounding_text(uint32_t beforeLength, uint32_t afterLength);
    void set_cursor_position(int32_t index, int32_t anchor);
    void set_text_direction(Qt::LayoutDirection direction);

    void keysym_pressed(uint32_t keysym, Qt::KeyboardModifiers modifiers = Qt::NoModifier);
    void keysym_released(uint32_t keysym, Qt::KeyboardModifiers modifiers = Qt::NoModifier);
    void set_input_panel_state(bool visible, QRect const& overlapped_surface_area);
    void set_language(std::string const& language_tag);

Q_SIGNALS:
    void enabled_changed();
    void show_input_panel_requested();
    void hide_input_panel_requested();
    void reset_requested();
    void preferred_language_changed();
    void cursor_rectangle_changed();
    void content_type_changed();
    void surrounding_text_changed();
    void resourceDestroyed();

private:
    explicit text_input_v2(Client* client, uint32_t version, uint32_t id);
    friend class text_input_manager_v2;
    friend class Seat;
    friend class text_input_pool;

    class Private;
    Private* d_ptr;
};

}

Q_DECLARE_METATYPE(Wrapland::Server::text_input_v2*)
Q_DECLARE_METATYPE(Wrapland::Server::text_input_v2_content_hint)
Q_DECLARE_METATYPE(Wrapland::Server::text_input_v2_content_hints)
Q_DECLARE_OPERATORS_FOR_FLAGS(Wrapland::Server::text_input_v2_content_hints)
Q_DECLARE_METATYPE(Wrapland::Server::text_input_v2_content_purpose)
