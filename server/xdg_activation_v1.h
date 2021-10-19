/*
    SPDX-FileCopyrightText: 2021 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#pragma once

#include <Wrapland/Server/wraplandserver_export.h>

#include <QObject>
#include <functional>
#include <memory>
#include <string>

namespace Wrapland::Server
{

class Client;
class Display;
class Seat;
class Surface;
class XdgActivationTokenV1;

class WRAPLANDSERVER_EXPORT XdgActivationV1 : public QObject
{
    Q_OBJECT
public:
    explicit XdgActivationV1(Display* display);
    ~XdgActivationV1() override;

Q_SIGNALS:
    void token_requested(Wrapland::Server::XdgActivationTokenV1* token);
    void activate(std::string token, Wrapland::Server::Surface* surface);

private:
    class Private;
    std::unique_ptr<Private> d_ptr;
};

class WRAPLANDSERVER_EXPORT XdgActivationTokenV1 : public QObject
{
    Q_OBJECT
public:
    uint32_t serial() const;
    Seat* seat() const;
    std::string app_id() const;
    Surface* surface() const;

    void done(std::string const& token);

Q_SIGNALS:
    void resourceDestroyed();

private:
    XdgActivationTokenV1(Client* client, uint32_t version, uint32_t id, XdgActivationV1* device);
    friend class XdgActivationV1;

    class Private;
    Private* d_ptr;
};

}
