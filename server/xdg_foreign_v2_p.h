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

#include <QObject>

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
    Private(Display* display, XdgForeign* q);

    XdgExporterV2* exporter;
    XdgImporterV2* importer;
};

class Q_DECL_HIDDEN XdgExporterV2 : public QObject
{
    Q_OBJECT
public:
    explicit XdgExporterV2(Display* display, QObject* parent = nullptr);
    ~XdgExporterV2() override;

    XdgExportedV2* exportedSurface(const QString& handle);

private:
    class Private;
    Private* d_ptr;
};

class Q_DECL_HIDDEN XdgImporterV2 : public QObject
{
    Q_OBJECT
public:
    explicit XdgImporterV2(Display* display, QObject* parent = nullptr);
    ~XdgImporterV2() override;
    void setExporter(XdgExporterV2* exporter);

    Surface* parentOf(Surface* surface);

Q_SIGNALS:
    void parentChanged(Wrapland::Server::Surface* child, Wrapland::Server::Surface* parent);

private:
    class Private;
    Private* d_ptr;
};

class Q_DECL_HIDDEN XdgExportedV2 : public QObject
{
    Q_OBJECT
public:
    XdgExportedV2(Client* client,
                  uint32_t version,
                  uint32_t id,
                  Surface* surface,
                  const QString& protocolHandle);

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
