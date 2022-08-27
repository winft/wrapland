/********************************************************************
Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>

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
#include "region.h"
#include "wayland_pointer_p.h"
// Qt
#include <QRegion>
#include <QVector>
// Wayland
#include <wayland-client-protocol.h>

namespace Wrapland
{
namespace Client
{

class Q_DECL_HIDDEN Region::Private
{
public:
    Private(QRegion const& region);
    void installRegion(QRect const& rect);
    void installRegion(QRegion const& region);
    void uninstallRegion(QRect const& rect);
    void uninstallRegion(QRegion const& region);

    WaylandPointer<wl_region, wl_region_destroy> region;
    QRegion qtRegion;
};

Region::Private::Private(QRegion const& region)
    : qtRegion(region)
{
}

void Region::Private::installRegion(QRect const& rect)
{
    if (!region.isValid()) {
        return;
    }
    wl_region_add(region, rect.x(), rect.y(), rect.width(), rect.height());
}

void Region::Private::installRegion(QRegion const& region)
{
    for (QRect const& rect : region) {
        installRegion(rect);
    }
}

void Region::Private::uninstallRegion(QRect const& rect)
{
    if (!region.isValid()) {
        return;
    }
    wl_region_subtract(region, rect.x(), rect.y(), rect.width(), rect.height());
}

void Region::Private::uninstallRegion(QRegion const& region)
{
    for (QRect const& rect : region) {
        uninstallRegion(rect);
    }
}

Region::Region(QRegion const& region, QObject* parent)
    : QObject(parent)
    , d(new Private(region))
{
}

Region::~Region()
{
    release();
}

void Region::release()
{
    d->region.release();
}

void Region::setup(wl_region* region)
{
    Q_ASSERT(region);
    d->region.setup(region);
    d->installRegion(d->qtRegion);
}

bool Region::isValid() const
{
    return d->region.isValid();
}

void Region::add(QRect const& rect)
{
    d->qtRegion = d->qtRegion.united(rect);
    d->installRegion(rect);
}

void Region::add(QRegion const& region)
{
    d->qtRegion = d->qtRegion.united(region);
    d->installRegion(region);
}

void Region::subtract(QRect const& rect)
{
    d->qtRegion = d->qtRegion.subtracted(rect);
    d->uninstallRegion(rect);
}

void Region::subtract(QRegion const& region)
{
    d->qtRegion = d->qtRegion.subtracted(region);
    d->uninstallRegion(region);
}

QRegion Region::region() const
{
    return d->qtRegion;
}

Region::operator wl_region*() const
{
    return d->region;
}

Region::operator wl_region*()
{
    return d->region;
}

}
}
