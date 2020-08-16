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
#include "output_changeset_v1_p.h"

namespace Wrapland::Server
{

OutputChangesetV1::Private::Private(OutputDeviceV1* outputDevice, OutputChangesetV1* parent)
    : device{outputDevice}
    , enabled{device->output()->enabled()}
    , modeId{device->output()->mode_id()}
    , transform{device->output()->transform()}
    , geometry{device->output()->geometry()}
    , q{parent}
{
}

OutputChangesetV1::OutputChangesetV1(OutputDeviceV1* outputDevice, QObject* parent)
    : QObject(parent)
    , d_ptr(new Private(outputDevice, this))
{
}

OutputChangesetV1::~OutputChangesetV1() = default;

bool OutputChangesetV1::enabledChanged() const
{
    return d_ptr->enabled != d_ptr->device->output()->enabled();
}

bool OutputChangesetV1::enabled() const
{
    return d_ptr->enabled;
}

bool OutputChangesetV1::modeChanged() const
{
    return d_ptr->modeId != d_ptr->device->output()->mode_id();
}

int OutputChangesetV1::mode() const
{
    return d_ptr->modeId;
}

bool OutputChangesetV1::transformChanged() const
{
    return d_ptr->transform != d_ptr->device->output()->transform();
}

Output::Transform OutputChangesetV1::transform() const
{
    return d_ptr->transform;
}
bool OutputChangesetV1::geometryChanged() const
{
    return d_ptr->geometry != d_ptr->device->output()->geometry();
}

QRectF OutputChangesetV1::geometry() const
{
    return d_ptr->geometry;
}

}
