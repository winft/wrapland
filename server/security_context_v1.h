/*
    SPDX-FileCopyrightText: 2024 Roman Glig <subdiff@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#pragma once

#include <Wrapland/Server/wraplandserver_export.h>

#include <QObject>
#include <memory>

namespace Wrapland::Server
{
class Display;

class WRAPLANDSERVER_EXPORT security_context_manager_v1 : public QObject
{
    Q_OBJECT
public:
    explicit security_context_manager_v1(Display* display);
    ~security_context_manager_v1() override;

private:
    class Private;
    std::unique_ptr<Private> d_ptr;
};

}
