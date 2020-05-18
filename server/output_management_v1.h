/********************************************************************
Copyright © 2020 Roman Gilg <subdiff@gmail.com>

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) version 3, or any
later version accepted by the membership of KDE e.V. (or its
successor approved by the membership of KDE e.V.), which shall
act as a proxy defined in Section 6 of version 3 of the license.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#pragma once

#include <QObject>

// For Qt metatype declaration.
#include "output_configuration_v1.h"

#include <memory>

#include <Wrapland/Server/wraplandserver_export.h>

namespace Wrapland::Server
{
class Display;
class OutputConfigurationV1;

class WRAPLANDSERVER_EXPORT OutputManagementV1 : public QObject
{
    Q_OBJECT
public:
    ~OutputManagementV1() override;

Q_SIGNALS:
    void configurationChangeRequested(OutputConfigurationV1* configuration);

private:
    explicit OutputManagementV1(Display* display, QObject* parent = nullptr);
    friend class Display;

    class Private;
    std::unique_ptr<Private> d_ptr;
};

}
