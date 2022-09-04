/*
    SPDX-FileCopyrightText: 2022 Roman Gilg <subdiff@gmail.com>

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

class WRAPLANDSERVER_EXPORT plasma_activation_feedback : public QObject
{
    Q_OBJECT
public:
    explicit plasma_activation_feedback(Display* display);
    ~plasma_activation_feedback() override;

    void app_id(std::string const& id);
    void finished(std::string const& id) const;

private:
    friend class plasma_activation;
    class Private;
    std::unique_ptr<Private> d_ptr;
};

}
