/*
    SPDX-FileCopyrightText: 2020 Faveraux Adrien <ad1rie3@hotmail.fr>
    SPDX-FileCopyrightText: 2022 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#pragma once

#include <Wrapland/Server/wraplandserver_export.h>

#include <QObject>
#include <chrono>
#include <memory>

namespace Wrapland::Server
{

class Client;
class Display;
class IdleTimeout;
class Seat;

class WRAPLANDSERVER_EXPORT KdeIdle : public QObject
{
    Q_OBJECT
public:
    explicit KdeIdle(Display* display);
    ~KdeIdle() override;

Q_SIGNALS:
    void timeout_created(IdleTimeout*);

private:
    class Private;
    std::unique_ptr<Private> d_ptr;
};

class WRAPLANDSERVER_EXPORT IdleTimeout : public QObject
{
    Q_OBJECT
public:
    ~IdleTimeout() override;

    std::chrono::milliseconds duration() const;
    Seat* seat() const;

    void idle();
    void resume();

Q_SIGNALS:
    void resourceDestroyed();
    void simulate_user_activity();

private:
    friend class KdeIdle;
    IdleTimeout(Client* client,
                uint32_t version,
                uint32_t id,
                std::chrono::milliseconds duration,
                Seat* seat);

    class Private;
    Private* d_ptr;
};

}
