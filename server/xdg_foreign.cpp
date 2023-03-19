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

XdgForeign::Private::Private(Display* display, XdgForeign* q_ptr)
    : q_ptr{q_ptr}
{
    display->globals.xdg_foreign = q_ptr;

    exporter = std::make_unique<XdgExporterV2>(display);
    importer = std::make_unique<XdgImporterV2>(display);
    importer->setExporter(exporter.get());

    connect(importer.get(), &XdgImporterV2::parentChanged, q_ptr, &XdgForeign::parentChanged);
}

XdgForeign::Private::~Private()
{
    if (exporter && exporter->d_ptr->display()) {
        if (auto& ptr = exporter->d_ptr->display()->handle->globals.xdg_foreign; ptr == q_ptr) {
            ptr = nullptr;
        }
    }
}

XdgForeign::XdgForeign(Display* display)
    : d_ptr(new Private(display, this))
{
}

XdgForeign::~XdgForeign() = default;

Surface* XdgForeign::parentOf(Surface* surface)
{
    return d_ptr->importer->parentOf(surface);
}

}
