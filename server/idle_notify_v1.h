/*
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
class idle_notification_v1;
class Seat;

class WRAPLANDSERVER_EXPORT idle_notifier_v1 : public QObject
{
    Q_OBJECT
public:
    explicit idle_notifier_v1(Display* display);
    ~idle_notifier_v1() override;

Q_SIGNALS:
    void notification_created(idle_notification_v1*);

private:
    class Private;
    std::unique_ptr<Private> d_ptr;
};

class WRAPLANDSERVER_EXPORT idle_notification_v1 : public QObject
{
    Q_OBJECT
public:
    ~idle_notification_v1() override;

    std::chrono::milliseconds duration() const;
    Seat* seat() const;

    void idle();
    void resume();

Q_SIGNALS:
    void resourceDestroyed();

private:
    friend class idle_notifier_v1;
    idle_notification_v1(Client* client,
                         uint32_t version,
                         uint32_t id,
                         std::chrono::milliseconds duration,
                         Seat* seat);

    class Private;
    Private* d_ptr;
};

}
