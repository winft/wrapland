/*
    SPDX-FileCopyrightText: 2020 Adrien Faveraux <af@brain-networks.fr>
    SPDX-FileCopyrightText: 2021 Francesco Sorrentino <francesco.sorr@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#pragma once

#include <Wrapland/Server/wraplandserver_export.h>

#include <QObject>
#include <memory>

namespace Wrapland::Server
{

class Client;
class Display;
class Seat;
class primary_selection_source;
class primary_selection_device;

class WRAPLANDSERVER_EXPORT primary_selection_device_manager : public QObject
{
    Q_OBJECT
public:
    using device_t = Wrapland::Server::primary_selection_device;
    using source_t = Wrapland::Server::primary_selection_source;

    ~primary_selection_device_manager() override;

    void create_source(Client* client, uint32_t version, uint32_t id);
    void get_device(Client* client, uint32_t version, uint32_t id, Seat* seat);

Q_SIGNALS:
    void source_created(Wrapland::Server::primary_selection_source* source);
    void device_created(Wrapland::Server::primary_selection_device* device);

private:
    friend class Display;
    explicit primary_selection_device_manager(Display* display, QObject* parent = nullptr);

    class Private;
    std::unique_ptr<Private> d_ptr;
};

class WRAPLANDSERVER_EXPORT primary_selection_device : public QObject
{
    Q_OBJECT
public:
    using source_t = Wrapland::Server::primary_selection_source;

    ~primary_selection_device() override;

    primary_selection_source* selection();
    Seat* seat() const;
    Client* client() const;

    void send_selection(primary_selection_source* source);
    void send_clear_selection();

Q_SIGNALS:
    void selection_changed(primary_selection_source* source);
    void selection_cleared();
    void resourceDestroyed();

private:
    primary_selection_device(Client* client, uint32_t version, uint32_t id, Seat* seat);
    friend class primary_selection_device_manager;

    class Private;
    Private* d_ptr;
};

class WRAPLANDSERVER_EXPORT primary_selection_offer : public QObject
{
    Q_OBJECT
public:
    ~primary_selection_offer() override;

    void send_offer();

Q_SIGNALS:
    void resourceDestroyed();

private:
    friend class primary_selection_device;
    explicit primary_selection_offer(Client* client,
                                     uint32_t version,
                                     primary_selection_source* source);

    class Private;
    Private* d_ptr;
};

class WRAPLANDSERVER_EXPORT primary_selection_source : public QObject
{
    Q_OBJECT
public:
    std::vector<std::string> mime_types() const;

    void cancel() const;
    void request_data(std::string const& mimeType, qint32 fd) const;

    Client* client() const;

Q_SIGNALS:
    void mime_type_offered(std::string);
    void resourceDestroyed();

private:
    friend class primary_selection_source_res;
    primary_selection_source();

    class Private;
    std::unique_ptr<Private> d_ptr;
};

}
