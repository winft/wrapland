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
#ifndef WRAPLAND_SERVER_XDGFOREIGNV2_INTERFACE_P_H
#define WRAPLAND_SERVER_XDGFOREIGNV2_INTERFACE_P_H

#include "global.h"
#include "resource.h"

namespace Wrapland
{
namespace Server
{

class Display;
class SurfaceInterface;
class XdgExportedV2Interface;
class XdgImportedV2Interface;

class Q_DECL_HIDDEN XdgForeignInterface::Private
{
public:
    Private(Display *display, XdgForeignInterface *q);

    XdgForeignInterface *q;
    XdgExporterV2Interface *exporter;
    XdgImporterV2Interface *importer;
};

class Q_DECL_HIDDEN XdgExporterV2Interface : public Global
{
    Q_OBJECT
public:
    virtual ~XdgExporterV2Interface();

    XdgExportedV2Interface *exportedSurface(const QString &handle);

Q_SIGNALS:
    void surfaceExported(const QString &handle, XdgExportedV2Interface *exported);
    void surfaceUnexported(const QString &handle);

private:
    explicit XdgExporterV2Interface(Display *display, XdgForeignInterface *parent = nullptr);
    friend class Display;
    friend class XdgForeignInterface;
    class Private;
    Private *d_func() const;
};

class Q_DECL_HIDDEN XdgImporterV2Interface : public Global
{
    Q_OBJECT
public:
    virtual ~XdgImporterV2Interface();

    XdgImportedV2Interface *importedSurface(const QString &handle);
    SurfaceInterface *transientFor(SurfaceInterface *surface);

Q_SIGNALS:
    void surfaceImported(const QString &handle, XdgImportedV2Interface *imported);
    void surfaceUnimported(const QString &handle);
    void transientChanged(Wrapland::Server::SurfaceInterface *child, Wrapland::Server::SurfaceInterface *parent);

private:
    explicit XdgImporterV2Interface(Display *display, XdgForeignInterface *parent = nullptr);
    friend class Display;
    friend class XdgForeignInterface;
    class Private;
    Private *d_func() const;
};

class Q_DECL_HIDDEN XdgExportedV2Interface : public Resource
{
    Q_OBJECT
public:
    virtual ~XdgExportedV2Interface();

private:
    explicit XdgExportedV2Interface(XdgExporterV2Interface *parent, wl_resource *parentResource);
    friend class XdgExporterV2Interface;

    class Private;
    Private *d_func() const;
};

class Q_DECL_HIDDEN XdgImportedV2Interface : public Resource
{
    Q_OBJECT
public:
    virtual ~XdgImportedV2Interface();

    SurfaceInterface *child() const;

Q_SIGNALS:
    void childChanged(Wrapland::Server::SurfaceInterface *child);

private:
    explicit XdgImportedV2Interface(XdgImporterV2Interface *parent, wl_resource *parentResource);
    friend class XdgImporterV2Interface;

    class Private;
    Private *d_func() const;
};

}
}

#endif
