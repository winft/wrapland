/*
    SPDX-FileCopyrightText: 2024 Roman Glig <subdiff@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#pragma once

#include "security_context_v1.h"

#include "client.h"
#include "display.h"
#include "logging.h"
#include "wayland/global.h"

#include <QSocketNotifier>
#include <sys/poll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <unordered_map>
#include <wayland-security-context-staging-v1-server-protocol.h>

namespace Wrapland::Server
{

constexpr uint32_t security_context_manager_v1_version = 1;
using security_context_manager_v1_global
    = Wayland::Global<security_context_manager_v1, security_context_manager_v1_version>;

struct security_context_inviter {
    security_context_inviter(int listen_fd,
                             int close_fd,
                             std::string const& app_id,
                             Display& display,
                             std::function<void()> finish_callback)
        : listen_fd{listen_fd}
        , close_fd{close_fd}
        , finish{std::move(finish_callback)}
        , qobject{std::make_unique<QObject>()}
    {
        listeners.close = std::make_unique<QSocketNotifier>(close_fd, QSocketNotifier::Read);
        QObject::connect(listeners.close.get(), &QSocketNotifier::activated, qobject.get(), [this] {
            if (is_closed()) {
                is_finished = true;
                finish();
            }
        });

        if (is_closed()) {
            is_finished = true;
            finish();
            return;
        }

        listeners.listen = std::make_unique<QSocketNotifier>(listen_fd, QSocketNotifier::Read);
        QObject::connect(listeners.listen.get(),
                         &QSocketNotifier::activated,
                         qobject.get(),
                         [app_id, disp_ptr = &display](auto descr) {
                             auto fd = accept4(descr, nullptr, nullptr, SOCK_CLOEXEC);
                             if (fd < 0) {
                                 qCWarning(WRAPLAND_SERVER)
                                     << "Failed to accept client from security listen FD";
                                 return;
                             }
                             auto client = disp_ptr->createClient(fd);
                             client->set_security_context_app_id(app_id);
                             Q_EMIT disp_ptr->clientConnected(client);
                         });
    }

    security_context_inviter(security_context_inviter const&) = delete;
    security_context_inviter(security_context_inviter&&) = delete;

    bool is_closed() const
    {
        pollfd pfd{
            .fd = close_fd,
            .events = POLLIN,
            .revents = 0,
        };

        if (poll(&pfd, 1, 0) < 0) {
            return true;
        }

        return pfd.revents & (POLLHUP | POLLERR);
    }

    bool is_finished{false};
    int listen_fd;
    int close_fd;
    std::string app_id;

    std::function<void()> finish;
    std::unique_ptr<QObject> qobject;
    struct {
        std::unique_ptr<QSocketNotifier> close;
        std::unique_ptr<QSocketNotifier> listen;
    } listeners;
};

class security_context_manager_v1::Private : public security_context_manager_v1_global
{
public:
    Private(Display* display, security_context_manager_v1* q_ptr);
    ~Private() override;

    void add_inviter(int listen_fd, int close_fd, std::string const& app_id);

    std::unordered_map<uint32_t, std::unique_ptr<security_context_inviter>> inviters;
    uint32_t inviter_index{0};

private:
    static void create_listener_callback(security_context_manager_v1_global::bind_t* bind,
                                         uint32_t id,
                                         int listen_fd,
                                         int close_fd);

    static struct wp_security_context_manager_v1_interface const s_interface;
};

class security_context_v1 : public QObject
{
    Q_OBJECT
public:
    explicit security_context_v1(Client* client, uint32_t version, uint32_t id);

Q_SIGNALS:
    void resourceDestroyed();
    void committed(std::string const& app_id);

public:
    friend class security_context_manager_v1;
    class Private;
    Private* d_ptr;
};

class security_context_v1::Private : public Wayland::Resource<security_context_v1>
{
public:
    Private(Client* client, uint32_t version, uint32_t id, security_context_v1* q_ptr);

    std::string sandbox_engine;
    std::string app_id;
    std::string instance_id;

    struct {
        bool sandbox_engine{false};
        bool app_id{false};
        bool instance_id{false};
        bool commit{false};
    } received;

    security_context_v1* q_ptr;

    static void
    set_sandbox_engine_callback(wl_client* wlClient, wl_resource* wlResource, char const* engine);
    static void set_app_id_callback(wl_client* wlClient, wl_resource* wlResource, char const* id);
    static void
    set_instance_id_callback(wl_client* wlClient, wl_resource* wlResource, char const* id);
    static void commit_callback(wl_client* wlClient, wl_resource* wlResource);

    static struct wp_security_context_v1_interface const s_interface;
};

}
