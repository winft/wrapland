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
    explicit XdgExporterV2Interface(Display *display, QObject *parent = nullptr);
    ~XdgExporterV2Interface() override;

    XdgExportedV2Interface *exportedSurface(const QString &handle);

private:
    class Private;
    Private *d_func() const;
};

class Q_DECL_HIDDEN XdgImporterV2Interface : public Global
{
    Q_OBJECT
public:
    explicit XdgImporterV2Interface(Display *display, QObject *parent = nullptr);
    ~XdgImporterV2Interface() override;
    void setExporter(XdgExporterV2Interface *exporter);

    SurfaceInterface *parentOf(SurfaceInterface *surface);

Q_SIGNALS:
    void parentChanged(Wrapland::Server::SurfaceInterface *child,
                       Wrapland::Server::SurfaceInterface *parent);

private:
    class Private;
    Private *d_func() const;
};

class Q_DECL_HIDDEN XdgExportedV2Interface : public Resource
{
    Q_OBJECT
public:
    XdgExportedV2Interface(XdgExporterV2Interface *parent, SurfaceInterface *surface);
    ~XdgExportedV2Interface() override;

    SurfaceInterface *surface() const;

private:
    class Private;
    Private *d_func() const;
};

class Q_DECL_HIDDEN XdgImportedV2Interface : public Resource
{
    Q_OBJECT
public:
    XdgImportedV2Interface(XdgImporterV2Interface *parent, wl_resource *parentResource,
                           XdgExportedV2Interface *exported);
    ~XdgImportedV2Interface() override;

    XdgExportedV2Interface* source() const;
    SurfaceInterface *child() const;

Q_SIGNALS:
    void childChanged(SurfaceInterface *parent, SurfaceInterface *prevChild,
                      SurfaceInterface *nextChild);

private:
    class Private;
    Private *d_func() const;
};

}
}
