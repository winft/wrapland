/****************************************************************************
Copyright 2017  Marco Martin <notmart@gmail.com>

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
****************************************************************************/
#include "xdg_foreign.h"

#include "xdg_foreign_v2_p.h"

#include "display.h"
#include "surface_p.h"

#include "wayland-xdg-foreign-unstable-v2-server-protocol.h"

namespace Wrapland::Server
{

XdgForeign::Private::Private(D_isplay* display, XdgForeign* q)
{
    exporter = new XdgExporterV2(display, q);
    importer = new XdgImporterV2(display, q);
    importer->setExporter(exporter);

    connect(importer, &XdgImporterV2::parentChanged, q, &XdgForeign::parentChanged);
}

XdgForeign::XdgForeign(D_isplay* display, QObject* parent)
    : QObject(parent)
    , d_ptr(new Private(display, this))
{
}

XdgForeign::~XdgForeign()
{
    delete d_ptr->exporter;
    delete d_ptr->importer;
    delete d_ptr;
}

Surface* XdgForeign::parentOf(Surface* surface)
{
    return d_ptr->importer->parentOf(surface);
}

}
