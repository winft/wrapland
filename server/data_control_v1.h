/*
    SPDX-FileCopyrightText: 2021 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#pragma once

#include <Wrapland/Server/wraplandserver_export.h>

#include <QObject>
#include <memory>

namespace Wrapland::Server
{
class Client;
class data_control_device_v1;
class data_source;
class Display;
class primary_selection_source;
class Seat;

class WRAPLANDSERVER_EXPORT data_control_manager_v1 : public QObject
{
    Q_OBJECT
public:
    ~data_control_manager_v1() override;

    void create_source(Client* client, uint32_t version, uint32_t id);
    void get_device(Client* client, uint32_t version, uint32_t id, Seat* seat);

Q_SIGNALS:
    void source_created();
    void device_created(Wrapland::Server::data_control_device_v1* device);

private:
    friend class Display;
    explicit data_control_manager_v1(Display* display, QObject* parent = nullptr);

    class Private;
    std::unique_ptr<Private> d_ptr;
};

class WRAPLANDSERVER_EXPORT data_control_device_v1 : public QObject
{
    Q_OBJECT
public:
    data_control_device_v1(Client* client, uint32_t version, uint32_t id, Seat* seat);

    data_source* selection() const;
    primary_selection_source* primary_selection() const;

    void send_selection(data_source* source) const;
    void send_primary_selection(primary_selection_source* source) const;

Q_SIGNALS:
    void selection_changed();
    void resourceDestroyed();

private:
    class impl;
    impl* d_ptr;
};

}
