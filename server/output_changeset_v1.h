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

#include "output.h"
#include "output_device_v1.h"

#include <Wrapland/Server/wraplandserver_export.h>
#include <memory>

namespace Wrapland::Server
{

class WRAPLANDSERVER_EXPORT OutputChangesetV1 : public QObject
{
    Q_OBJECT
public:
    ~OutputChangesetV1() override;

    bool enabledChanged() const;
    bool transformChanged() const;
    bool modeChanged() const;
    bool geometryChanged() const;
    bool enabled() const;
    int mode() const;
    output_transform transform() const;
    QRectF geometry() const;

private:
    friend class OutputConfigurationV1;
    explicit OutputChangesetV1(OutputDeviceV1* outputDevice, QObject* parent = nullptr);

    class Private;
    std::unique_ptr<Private> d_ptr;
};

}
