/*
    SPDX-FileCopyrightText: 2021 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#pragma once

#include "keyboard_pool.h"

#include <Wrapland/Server/wraplandserver_export.h>

#include <QObject>
#include <memory>

namespace Wrapland::Server
{

class Client;
class Display;
class Seat;
class virtual_keyboard_v1;

class WRAPLANDSERVER_EXPORT virtual_keyboard_manager_v1 : public QObject
{
    Q_OBJECT
public:
    explicit virtual_keyboard_manager_v1(Display* display);
    ~virtual_keyboard_manager_v1() override;

Q_SIGNALS:
    void keyboard_created(Wrapland::Server::virtual_keyboard_v1* keyboard,
                          Wrapland::Server::Seat* seat);

private:
    class Private;
    std::unique_ptr<Private> d_ptr;
};

class WRAPLANDSERVER_EXPORT virtual_keyboard_v1 : public QObject
{
    Q_OBJECT

Q_SIGNALS:
    void keymap(quint32 format, qint32 fd, quint32 size);
    void key(quint32 time, quint32 key, Wrapland::Server::key_state state);
    void modifiers(quint32 depressed, quint32 latched, quint32 locked, quint32 group);
    void resourceDestroyed();

private:
    virtual_keyboard_v1(Client* client, uint32_t version, uint32_t id);
    friend class virtual_keyboard_manager_v1;

    class Private;
    Private* d_ptr;
};

}

Q_DECLARE_METATYPE(Wrapland::Server::virtual_keyboard_manager_v1*)
Q_DECLARE_METATYPE(Wrapland::Server::virtual_keyboard_v1*)
