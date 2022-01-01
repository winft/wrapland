/*
    SPDX-FileCopyrightText: 2021 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#pragma once

#include <Wrapland/Server/wraplandserver_export.h>

#include <QMargins>
#include <QObject>
#include <QSize>
#include <memory>
#include <vector>

namespace Wrapland::Server
{

class Client;
class Display;
class drm_lease_connector_v1;
class drm_lease_v1;
class Output;

class WRAPLANDSERVER_EXPORT drm_lease_device_v1 : public QObject
{
    Q_OBJECT
public:
    explicit drm_lease_device_v1(Display* display);
    ~drm_lease_device_v1() override;

    void update_fd(int fd);
    drm_lease_connector_v1*
    create_connector(std::string const& name, std::string const& description, int id);
    drm_lease_connector_v1* create_connector(Output* output);

Q_SIGNALS:
    void needs_new_client_fd();
    void leased(drm_lease_v1* lease);

private:
    friend class drm_lease_connector_v1;

    class Private;
    std::unique_ptr<Private> d_ptr;
};

class WRAPLANDSERVER_EXPORT drm_lease_connector_v1 : public QObject
{
    Q_OBJECT
public:
    ~drm_lease_connector_v1() override;

    uint32_t id() const;
    Output* output() const;

private:
    drm_lease_connector_v1(std::string const& name,
                           std::string const& description,
                           int id,
                           drm_lease_device_v1* device);
    drm_lease_connector_v1(Output* output, drm_lease_device_v1* device);
    friend class drm_lease_connector_v1_res;
    friend class drm_lease_device_v1;
    friend class drm_lease_request_v1;

    class Private;
    std::unique_ptr<Private> d_ptr;
};

class WRAPLANDSERVER_EXPORT drm_lease_v1 : public QObject
{
    Q_OBJECT
public:
    Client* lessee() const;
    std::vector<drm_lease_connector_v1*> connectors() const;

    int lessee_id() const;

    void grant(int fd);
    void finish();

Q_SIGNALS:
    void resourceDestroyed();

private:
    drm_lease_v1(Client* client,
                 uint32_t version,
                 uint32_t id,
                 std::vector<drm_lease_connector_v1*>&& connectors,
                 drm_lease_device_v1* device);
    friend class drm_lease_request_v1;

    class Private;
    Private* d_ptr;
};

}
