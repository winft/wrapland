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
#include "output_changeset_v1.h"
#include "output_changeset_v1_p.h"

namespace Wrapland
{
namespace Server
{

OutputChangesetV1::Private::Private(OutputDeviceV1Interface *outputDevice,
                                    OutputChangesetV1 *parent)
    : q(parent)
    , o(outputDevice)
    , enabled(o->enabled())
    , modeId(o->modeId())
    , transform(o->transform())
    , geometry(o->geometry())
{
}

OutputChangesetV1::Private::~Private() = default;

OutputChangesetV1::OutputChangesetV1(OutputDeviceV1Interface *outputDevice, QObject *parent)
    : QObject(parent)
    , d(new Private(outputDevice, this))
{
}

OutputChangesetV1::~OutputChangesetV1() = default;

OutputChangesetV1::Private *OutputChangesetV1::d_func() const
{
    return reinterpret_cast<Private*>(d.get());
}

bool OutputChangesetV1::enabledChanged() const
{
    Q_D();
    return d->enabled != d->o->enabled();
}

OutputDeviceV1Interface::Enablement OutputChangesetV1::enabled() const
{
    Q_D();
    return d->enabled;
}

bool OutputChangesetV1::modeChanged() const
{
    Q_D();
    return d->modeId != d->o->modeId();
}

int OutputChangesetV1::mode() const
{
    Q_D();
    return d->modeId;
}

bool OutputChangesetV1::transformChanged() const
{
    Q_D();
    return d->transform != d->o->transform();
}

OutputDeviceV1Interface::Transform OutputChangesetV1::transform() const
{
    Q_D();
    return d->transform;
}
bool OutputChangesetV1::geometryChanged() const
{
    Q_D();
    return d->geometry != d->o->geometry();
}

QRectF OutputChangesetV1::geometry() const
{
    Q_D();
    return d->geometry;
}

}
}
