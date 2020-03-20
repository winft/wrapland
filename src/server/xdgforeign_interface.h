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
#pragma once

#include "global.h"
#include "resource.h"

#include <Wrapland/Server/wraplandserver_export.h>

namespace Wrapland
{
namespace Server
{

class Display;
class SurfaceInterface;
class XdgExporterV2Interface;
class XdgImporterV2Interface;

/**
 * This class encapsulates the server side logic of the XdgForeign protocol.
 * a process can export a surface to be identifiable by a server-wide unique
 * string handle, and another process can in turn import that surface, and set it
 * as transient parent for one of its own surfaces.
 * This parent relationship is traced by the parentChanged signal and the
 * parentFor method.
 *
 * @since 5.40
 */
class WRAPLANDSERVER_EXPORT XdgForeignInterface : public QObject
{
    Q_OBJECT
public:
    XdgForeignInterface(Display *display, QObject *parent = nullptr);
    ~XdgForeignInterface() override;

    /**
     * Creates the native zxdg_exporter_v2 and zxdg_importer_v2 interfaces
     * and announces them to the client.
     */
    void create();

    /**
     * @returns true if theimporter and exporter are valid and functional
     */
    bool isValid();

    /**
     * This returns the xdg-foreign parent surface of @param surface, i.e. this returns a valid
     * surface pointer if:
     * - the client did import a foreign surface via the xdg-foreign protocol and
     * - set the foreign surface as the parent of @param surface.
     *
     * @param surface that a parent is searched for
     * @returns the parent if found, nullptr otherwise
     */
    SurfaceInterface* parentOf(SurfaceInterface *surface);

Q_SIGNALS:
    /**
     * An inheritance relation between surfaces changed.
     * @param parent is the surface exported by one client and imported into another, which will act
     *        as parent.
     * @param child is the surface that the importer client did set as child of the surface that it
     *        imported.
     * If one of the two paramenters is nullptr, it means that a previously relation is not valid
     * anymore and either one of the surfaces has been unmapped, or the parent surface is not
     * exported anymore.
     */
    void parentChanged(Wrapland::Server::SurfaceInterface *parent,
                       Wrapland::Server::SurfaceInterface *child);

private:
    friend class Display;
    class Private;
    Private *d;
};

}
}
