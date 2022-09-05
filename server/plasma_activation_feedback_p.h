/*
    SPDX-FileCopyrightText: 2022 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#pragma once

#include "plasma_activation_feedback.h"

#include "wayland/global.h"
#include "wayland/resource.h"

#include <QObject>
#include <unordered_map>

#include <wayland-plasma-window-management-server-protocol.h>

namespace Wrapland::Server
{

class Display;
class plasma_activation;

constexpr uint32_t plasma_activation_feedback_version = 1;
using plasma_activation_feedback_global
    = Wayland::Global<plasma_activation_feedback, plasma_activation_feedback_version>;
using plasma_activation_feedback_bind = Wayland::Bind<plasma_activation_feedback_global>;

class plasma_activation_feedback::Private : public plasma_activation_feedback_global
{
public:
    Private(Display* display, plasma_activation_feedback* q_ptr);

    plasma_activation* create_activation(plasma_activation_feedback_bind* bind,
                                         std::string const& id);

    std::unordered_map<std::string, std::vector<plasma_activation*>> activations;

private:
    void bindInit(plasma_activation_feedback_bind* bind) override;

    plasma_activation_feedback* q_ptr;

    static struct org_kde_plasma_activation_feedback_interface const s_interface;
};

class plasma_activation : public QObject
{
    Q_OBJECT
public:
    ~plasma_activation() override;

    void finished() const;

Q_SIGNALS:
    void resourceDestroyed();

private:
    friend class plasma_activation_feedback;
    plasma_activation(Client* client,
                      uint32_t version,
                      uint32_t id,
                      std::string const& app_id,
                      plasma_activation_feedback* manager);

    class Private;
    Private* d_ptr;
};

class plasma_activation::Private : public Wayland::Resource<plasma_activation>
{
public:
    Private(Client* client,
            uint32_t version,
            uint32_t id,
            std::string app_id,
            plasma_activation_feedback* manager,
            plasma_activation* q_ptr);

    std::string app_id;
    plasma_activation_feedback* manager;

private:
    static struct org_kde_plasma_activation_interface const s_interface;
};

}
