/*
    SPDX-FileCopyrightText: 2020 Adrien Faveraux <af@brain-networks.fr>
    SPDX-FileCopyrightText: 2021 Francesco Sorrentino <francesco.sorr@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#pragma once

#include <QObject>

#include <memory>

#include <wayland-server.h>

#include <Wrapland/Server/wraplandserver_export.h>

class QMimeType;

namespace Wrapland::Server
{

class Display;
class Client;
class Seat;
class PrimarySelectionSource;
class PrimarySelectionDevice;
class PrimarySelectionOffer;

class WRAPLANDSERVER_EXPORT PrimarySelectionDeviceManager : public QObject
{
    Q_OBJECT
public:
    using device_t = Wrapland::Server::PrimarySelectionDevice;
    using source_t = Wrapland::Server::PrimarySelectionSource;

    ~PrimarySelectionDeviceManager() override;

Q_SIGNALS:
    void deviceCreated(Wrapland::Server::PrimarySelectionDevice* device);
    void sourceCreated(Wrapland::Server::PrimarySelectionSource* source);

private:
    friend class Display;
    explicit PrimarySelectionDeviceManager(Display* display, QObject* parent = nullptr);

    template<typename Global>
    // NOLINTNEXTLINE(readability-redundant-declaration)
    friend void create_selection_source(wl_client* wlClient, wl_resource* wlResource, uint32_t id);
    template<typename Global>
    // NOLINTNEXTLINE(readability-redundant-declaration)
    friend void get_selection_device(wl_client* wlClient,
                                     wl_resource* wlResource,
                                     uint32_t id,
                                     wl_resource* wlSeat);

    class Private;
    std::unique_ptr<Private> d_ptr;
};

class WRAPLANDSERVER_EXPORT PrimarySelectionDevice : public QObject
{
    Q_OBJECT
public:
    using source_t = Wrapland::Server::PrimarySelectionSource;

    PrimarySelectionDevice(Client* client, uint32_t version, uint32_t id, Seat* seat);
    ~PrimarySelectionDevice() override;

    PrimarySelectionSource* selection();
    Seat* seat() const;
    Client* client() const;

    void sendSelection(PrimarySelectionDevice* device);
    void sendClearSelection();

Q_SIGNALS:
    void selectionChanged(PrimarySelectionSource* source);
    void selectionCleared();
    void resourceDestroyed();

private:
    template<typename Resource>
    // NOLINTNEXTLINE(readability-redundant-declaration)
    friend void set_selection(Resource* handle, wl_resource* wlSource);

    class Private;
    Private* d_ptr;
};

class WRAPLANDSERVER_EXPORT PrimarySelectionOffer : public QObject
{
    Q_OBJECT
public:
    ~PrimarySelectionOffer() override;

    void sendOffer();

Q_SIGNALS:
    void resourceDestroyed();

private:
    friend class PrimarySelectionDevice;
    explicit PrimarySelectionOffer(Client* client,
                                   uint32_t version,
                                   PrimarySelectionSource* source);

    template<typename Resource>
    // NOLINTNEXTLINE(readability-redundant-declaration)
    friend void receive_selection_offer(wl_client* wlClient,
                                        wl_resource* wlResource,
                                        char const* mimeType,
                                        int32_t fd);

    class Private;
    Private* d_ptr;
};

class WRAPLANDSERVER_EXPORT PrimarySelectionSource : public QObject
{
    Q_OBJECT
public:
    PrimarySelectionSource(Client* client, uint32_t version, uint32_t id);
    ~PrimarySelectionSource() override;

    void cancel();
    void requestData(std::string const& mimeType, qint32 fd);

    std::vector<std::string> mimeTypes();

    Client* client() const;

Q_SIGNALS:
    void mimeTypeOffered(std::string);
    void resourceDestroyed();

private:
    template<typename Resource>
    // NOLINTNEXTLINE(readability-redundant-declaration)
    friend void
    // NOLINTNEXTLINE(readability-redundant-declaration)
    add_offered_mime_type(wl_client* wlClient, wl_resource* wlResource, char const* mimeType);

    class Private;
    Private* d_ptr;
};

}
