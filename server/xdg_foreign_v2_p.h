/****************************************************************************
Copyright © 2017 Marco Martin <notmart@gmail.com>
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
****************************************************************************/
#pragma once

#include "wayland/global.h"

#include "wayland-xdg-foreign-unstable-v2-server-protocol.h"

#include <QObject>
#include <memory>

namespace Wrapland::Server
{
class Client;
class Display;
class Surface;
class XdgExportedV2;
class XdgImportedV2;

class Q_DECL_HIDDEN XdgForeign::Private
{
public:
    Private(Display* display, XdgForeign* q_ptr);
    ~Private();

    XdgForeign* q_ptr;
    std::unique_ptr<XdgExporterV2> exporter;
    std::unique_ptr<XdgImporterV2> importer;
};

class Q_DECL_HIDDEN XdgExporterV2 : public QObject
{
    Q_OBJECT
public:
    explicit XdgExporterV2(Display* display);
    ~XdgExporterV2() override;

    XdgExportedV2* exportedSurface(QString const& handle) const;

    class Private;
    Private* d_ptr;
};

constexpr uint32_t XdgExporterV2Version = 1;
using XdgExporterV2Global = Wayland::Global<XdgExporterV2, XdgExporterV2Version>;

class Q_DECL_HIDDEN XdgExporterV2::Private
    : public Wayland::Global<XdgExporterV2, XdgExporterV2Version>
{
public:
    Private(XdgExporterV2* q_ptr, Display* display);

    QHash<QString, XdgExportedV2*> exportedSurfaces;

private:
    static void
    exportToplevelCallback(XdgExporterV2Global::bind_t* bind, uint32_t id, wl_resource* wlSurface);

    static const struct zxdg_exporter_v2_interface s_interface;
};

class Q_DECL_HIDDEN XdgImporterV2 : public QObject
{
    Q_OBJECT
public:
    explicit XdgImporterV2(Display* display);
    ~XdgImporterV2() override;
    void setExporter(XdgExporterV2* exporter) const;

    Surface* parentOf(Surface* surface) const;

    class Private;
    Private* d_ptr;

Q_SIGNALS:
    void parentChanged(Wrapland::Server::Surface* parent, Wrapland::Server::Surface* child);
};

class Q_DECL_HIDDEN XdgExportedV2 : public QObject
{
    Q_OBJECT
public:
    XdgExportedV2(Client* client,
                  uint32_t version,
                  uint32_t id,
                  Surface* surface,
                  QString const& protocolHandle);

    Surface* surface() const;

Q_SIGNALS:
    void resourceDestroyed();

private:
    class Private;
    Private* d_ptr;
};

class Q_DECL_HIDDEN XdgImportedV2 : public QObject
{
    Q_OBJECT
public:
    XdgImportedV2(Client* client, uint32_t version, uint32_t id, XdgExportedV2* exported);
    ~XdgImportedV2() override;

    XdgExportedV2* source() const;
    Surface* child() const;

Q_SIGNALS:
    void childChanged(Surface* parent, Surface* prevChild, Surface* nextChild);
    void resourceDestroyed();

private:
    void onSourceDestroy();

    class Private;
    Private* d_ptr;
};

}
