/*
    SPDX-FileCopyrightText: 2020 Aleix Pol Gonzalez <aleixpol@kde.org>
    SPDX-FileCopyrightText: 2021 Dorota Czaplejewicz <gihuac.dcz@porcupinefactory.org>
    SPDX-FileCopyrightText: 2021 Roman Glig <subdiff@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#pragma once

#include "keyboard_pool.h"
#include "text_input_v3.h"

#include <Wrapland/Server/wraplandserver_export.h>

#include <QObject>
#include <memory>
#include <vector>

namespace Wrapland::Server
{
class Client;
class Display;
class input_method_keyboard_grab_v2;
class input_method_popup_surface_v2;

class WRAPLANDSERVER_EXPORT input_method_manager_v2 : public QObject
{
    Q_OBJECT
public:
    explicit input_method_manager_v2(Display* d);
    ~input_method_manager_v2() override;

private:
    class Private;
    std::unique_ptr<Private> d_ptr;
};

struct input_method_v2_state {
    struct {
        bool update{false};
        std::string data;
        uint32_t cursor_begin{0};
        uint32_t cursor_end{0};
    } preedit_string;

    struct {
        bool update{false};
        std::string data;
    } commit_string;

    struct {
        bool update{false};
        uint32_t before_length{0};
        uint32_t after_length{0};
    } delete_surrounding_text;
};

class WRAPLANDSERVER_EXPORT input_method_v2 : public QObject
{
    Q_OBJECT
public:
    input_method_v2_state const& state() const;

    std::vector<input_method_popup_surface_v2*> const& get_popups() const;

    void set_active(bool active);
    void set_surrounding_text(std::string const& text,
                              uint32_t cursor,
                              uint32_t anchor,
                              text_input_v3_change_cause change_cause);
    void set_content_type(text_input_v3_content_hints hints, text_input_v3_content_purpose purpose);
    void done();

Q_SIGNALS:
    void state_committed();
    void keyboard_grabbed(Wrapland::Server::input_method_keyboard_grab_v2*);
    void popup_surface_created(Wrapland::Server::input_method_popup_surface_v2*);
    void resourceDestroyed();

private:
    explicit input_method_v2(Client* client, uint32_t version, uint32_t id);
    friend class input_method_manager_v2;

    class Private;
    Private* d_ptr;
};

class WRAPLANDSERVER_EXPORT input_method_keyboard_grab_v2 : public QObject
{
    Q_OBJECT
public:
    void set_keymap(std::string const& content);

    void key(uint32_t time, uint32_t key, key_state state);

    void update_modifiers(uint32_t depressed, uint32_t latched, uint32_t locked, uint32_t group);
    void set_repeat_info(int32_t rate, int32_t delay);

Q_SIGNALS:
    void resourceDestroyed();

private:
    explicit input_method_keyboard_grab_v2(Client* client,
                                           uint32_t version,
                                           uint32_t id,
                                           Seat* seat);
    friend class input_method_v2;

    class Private;
    Private* d_ptr;
};

class WRAPLANDSERVER_EXPORT input_method_popup_surface_v2 : public QObject
{
    Q_OBJECT
public:
    Surface* surface() const;
    void set_text_input_rectangle(QRect const& rect);

Q_SIGNALS:
    void resourceDestroyed();

private:
    explicit input_method_popup_surface_v2(Client* client,
                                           uint32_t version,
                                           uint32_t id,
                                           Surface* surface);
    friend class input_method_v2;

    class Private;
    Private* d_ptr;
};

}

Q_DECLARE_METATYPE(Wrapland::Server::input_method_v2*)
Q_DECLARE_METATYPE(Wrapland::Server::input_method_keyboard_grab_v2*)
Q_DECLARE_METATYPE(Wrapland::Server::input_method_popup_surface_v2*)
