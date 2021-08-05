/*
    SPDX-FileCopyrightText: 2018 Roman Glig <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2020 Bhushan Shah <bshah@kde.org>
    SPDX-FileCopyrightText: 2021 dcz <gihuac.dcz@porcupinefactory.org>
    SPDX-FileCopyrightText: 2021 Roman Glig <subdiff@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#pragma once

#include <Wrapland/Server/wraplandserver_export.h>

#include <QObject>
#include <QRect>

#include <memory>

namespace Wrapland::Server
{
class Client;
class Display;
class Seat;
class Surface;

enum class text_input_v3_content_hint : uint32_t {
    none = 0,
    completion = 1 << 0,
    spellcheck = 1 << 1,
    auto_capitalization = 1 << 2,
    lowercase = 1 << 3,
    uppercase = 1 << 4,
    titlecase = 1 << 5,
    hidden_text = 1 << 6,
    sensitive_data = 1 << 7,
    latin = 1 << 8,
    multiline = 1 << 9,
};
Q_DECLARE_FLAGS(text_input_v3_content_hints, text_input_v3_content_hint)

enum class text_input_v3_content_purpose : uint32_t {
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

enum class text_input_v3_change_cause {
    input_method,
    other,
};

struct text_input_v3_state {
    bool enabled{false};
    QRect cursor_rectangle;
    struct {
        text_input_v3_content_hints hints{text_input_v3_content_hint::none};
        text_input_v3_content_purpose purpose{text_input_v3_content_purpose::normal};
    } content;

    struct {
        bool update{false};
        std::string data;
        int32_t cursor_position{0};
        int32_t selection_anchor{0};
        text_input_v3_change_cause change_cause{text_input_v3_change_cause::other};
    } surrounding_text;
};

class WRAPLANDSERVER_EXPORT text_input_manager_v3 : public QObject
{
    Q_OBJECT
public:
    explicit text_input_manager_v3(Display* display, QObject* parent = nullptr);
    ~text_input_manager_v3() override;

private:
    class Private;
    std::unique_ptr<Private> d_ptr;
};

class WRAPLANDSERVER_EXPORT text_input_v3 : public QObject
{
    Q_OBJECT
public:
    text_input_v3_state const& state() const;
    Surface* surface() const;
    Client* client() const;

    void delete_surrounding_text(uint32_t before_length, uint32_t after_length);
    void set_preedit_string(std::string const& text, uint32_t cursor_begin, uint32_t cursor_end);
    void commit_string(std::string const& text);

    void done();

Q_SIGNALS:
    void state_committed();
    void resourceDestroyed();

private:
    explicit text_input_v3(Client* client, uint32_t version, uint32_t id);
    friend class text_input_manager_v3;
    friend class Seat;

    class Private;
    Private* d_ptr;
};

}

Q_DECLARE_METATYPE(Wrapland::Server::text_input_v3*)
Q_DECLARE_METATYPE(Wrapland::Server::text_input_v3_content_hint)
Q_DECLARE_METATYPE(Wrapland::Server::text_input_v3_content_hints)
Q_DECLARE_OPERATORS_FOR_FLAGS(Wrapland::Server::text_input_v3_content_hints)
Q_DECLARE_METATYPE(Wrapland::Server::text_input_v3_content_purpose)
Q_DECLARE_METATYPE(Wrapland::Server::text_input_v3_change_cause)
