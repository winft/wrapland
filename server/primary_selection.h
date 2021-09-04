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
class PrimarySelectionSource;
class PrimarySelectionDevice;

class WRAPLANDSERVER_EXPORT PrimarySelectionDeviceManager : public QObject
{
    Q_OBJECT
public:
    using device_t = Wrapland::Server::PrimarySelectionDevice;
    using source_t = Wrapland::Server::PrimarySelectionSource;

    ~PrimarySelectionDeviceManager() override;

    void create_source(Client* client, uint32_t version, uint32_t id);
    void get_device(Client* client, uint32_t version, uint32_t id, Seat* seat);

Q_SIGNALS:
    void source_created(Wrapland::Server::PrimarySelectionSource* source);
    void device_created(Wrapland::Server::PrimarySelectionDevice* device);

private:
    friend class Display;
    explicit PrimarySelectionDeviceManager(Display* display, QObject* parent = nullptr);

    class Private;
    std::unique_ptr<Private> d_ptr;
};

class WRAPLANDSERVER_EXPORT PrimarySelectionDevice : public QObject
{
    Q_OBJECT
public:
    using source_t = Wrapland::Server::PrimarySelectionSource;

    ~PrimarySelectionDevice() override;

    PrimarySelectionSource* selection();
    Seat* seat() const;
    Client* client() const;

    void send_selection(PrimarySelectionSource* source);
    void send_clear_selection();

Q_SIGNALS:
    void selection_changed(PrimarySelectionSource* source);
    void selection_cleared();
    void resourceDestroyed();

private:
    PrimarySelectionDevice(Client* client, uint32_t version, uint32_t id, Seat* seat);
    friend class PrimarySelectionDeviceManager;

    class Private;
    Private* d_ptr;
};

class WRAPLANDSERVER_EXPORT PrimarySelectionOffer : public QObject
{
    Q_OBJECT
public:
    ~PrimarySelectionOffer() override;

    void send_offer();

Q_SIGNALS:
    void resourceDestroyed();

private:
    friend class PrimarySelectionDevice;
    explicit PrimarySelectionOffer(Client* client,
                                   uint32_t version,
                                   PrimarySelectionSource* source);

    class Private;
    Private* d_ptr;
};

class WRAPLANDSERVER_EXPORT PrimarySelectionSource : public QObject
{
    Q_OBJECT
public:
    ~PrimarySelectionSource() override;

    void cancel();
    void request_data(std::string const& mimeType, qint32 fd);

    std::vector<std::string> mime_types();

    Client* client() const;

Q_SIGNALS:
    void mime_type_offered(std::string);
    void resourceDestroyed();

private:
    PrimarySelectionSource(Client* client, uint32_t version, uint32_t id);
    friend class PrimarySelectionDeviceManager;
    friend class PrimarySelectionDevice;

    class Private;
    Private* d_ptr;
};

}
