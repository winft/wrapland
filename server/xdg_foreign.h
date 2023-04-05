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

#include <Wrapland/Server/wraplandserver_export.h>

#include <QObject>
#include <memory>

namespace Wrapland::Server
{
class Display;
class Surface;
class XdgExporterV2;
class XdgImporterV2;

class WRAPLANDSERVER_EXPORT XdgForeign : public QObject
{
    Q_OBJECT
public:
    explicit XdgForeign(Display* display);
    ~XdgForeign() override;

    /**
     * This returns the xdg-foreign parent surface of @param surface, i.e. this returns a valid
     * surface pointer if:
     * - the client did import a foreign surface via the xdg-foreign protocol and
     * - set the foreign surface as the parent of @param surface.
     *
     * @param surface that a parent is searched for
     * @returns the parent if found, nullptr otherwise
     */
    Surface* parentOf(Surface* surface);

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
    void parentChanged(Wrapland::Server::Surface* parent, Wrapland::Server::Surface* child);

private:
    friend class Display;
    class Private;
    std::unique_ptr<Private> d_ptr;
};

}
