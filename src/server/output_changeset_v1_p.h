/********************************************************************
Copyright © 2015  Sebastian Kügler <sebas@kde.org>
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

#include "output_changeset_v1.h"

#include <QRectF>

namespace Wrapland
{
namespace Server
{

class OutputChangesetV1::Private
{
public:
    Private(OutputDeviceV1Interface *outputDevice, OutputChangesetV1 *parent);
    ~Private();

    OutputChangesetV1 *q;
    OutputDeviceV1Interface *o;

    OutputDeviceV1Interface::Enablement enabled;
    int modeId;
    OutputDeviceV1Interface::Transform transform;
    QRectF geometry;
};
}
}
