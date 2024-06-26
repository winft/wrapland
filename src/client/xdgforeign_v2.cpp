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
#include "xdgforeign_v2.h"
#include "event_queue.h"
#include "wayland_pointer_p.h"
#include "xdgforeign_p.h"

#include <wayland-xdg-foreign-unstable-v2-client-protocol.h>

#include <QDebug>

namespace Wrapland
{
namespace Client
{

class Q_DECL_HIDDEN XdgExporterUnstableV2::Private : public XdgExporter::Private
{
public:
    Private();

    XdgExported* exportTopLevelV2(Surface* surface, QObject* parent) override;
    void setupV2(zxdg_exporter_v2* arg) override;
    zxdg_exporter_v2* exporterV2() override;

    void release() override;
    bool isValid() override;

    WaylandPointer<zxdg_exporter_v2, zxdg_exporter_v2_destroy> exporter;
};

XdgExporterUnstableV2::Private::Private()
    : XdgExporter::Private()
{
}

zxdg_exporter_v2* XdgExporterUnstableV2::Private::exporterV2()
{
    return exporter;
}

void XdgExporterUnstableV2::Private::release()
{
    exporter.release();
}

bool XdgExporterUnstableV2::Private::isValid()
{
    return exporter.isValid();
}

XdgExported* XdgExporterUnstableV2::Private::exportTopLevelV2(Surface* surface, QObject* parent)
{
    Q_ASSERT(isValid());
    auto p = new XdgExportedUnstableV2(parent);
    auto w = zxdg_exporter_v2_export_toplevel(exporter, *surface);
    if (queue) {
        queue->addProxy(w);
    }
    p->setup(w);
    return p;
}

XdgExporterUnstableV2::XdgExporterUnstableV2(QObject* parent)
    : XdgExporter(new Private, parent)
{
}

void XdgExporterUnstableV2::Private::setupV2(zxdg_exporter_v2* arg)
{
    Q_ASSERT(arg);
    Q_ASSERT(!exporter);
    exporter.setup(arg);
}

XdgExporterUnstableV2::~XdgExporterUnstableV2()
{
}

class Q_DECL_HIDDEN XdgImporterUnstableV2::Private : public XdgImporter::Private
{
public:
    Private();

    XdgImported* importTopLevelV2(QString const& handle, QObject* parent) override;
    void setupV2(zxdg_importer_v2* arg) override;
    zxdg_importer_v2* importerV2() override;

    void release() override;
    bool isValid() override;

    WaylandPointer<zxdg_importer_v2, zxdg_importer_v2_destroy> importer;
    EventQueue* queue = nullptr;
};

XdgImporterUnstableV2::Private::Private()
    : XdgImporter::Private()
{
}

zxdg_importer_v2* XdgImporterUnstableV2::Private::importerV2()
{
    return importer;
}

void XdgImporterUnstableV2::Private::release()
{
    importer.release();
}

bool XdgImporterUnstableV2::Private::isValid()
{
    return importer.isValid();
}

XdgImported* XdgImporterUnstableV2::Private::importTopLevelV2(QString const& handle,
                                                              QObject* parent)
{
    Q_ASSERT(isValid());
    auto* p = new XdgImportedUnstableV2(parent);
    auto* w = zxdg_importer_v2_import_toplevel(importer, handle.toUtf8());
    if (queue) {
        queue->addProxy(w);
    }
    p->setup(w);
    return p;
}

XdgImporterUnstableV2::XdgImporterUnstableV2(QObject* parent)
    : XdgImporter(new Private, parent)
{
}

void XdgImporterUnstableV2::Private::setupV2(zxdg_importer_v2* arg)
{
    Q_ASSERT(arg);
    Q_ASSERT(!importer);
    importer.setup(arg);
}

XdgImporterUnstableV2::~XdgImporterUnstableV2()
{
}

class Q_DECL_HIDDEN XdgExportedUnstableV2::Private : public XdgExported::Private
{
public:
    Private(XdgExportedUnstableV2* q);

    void setupV2(zxdg_exported_v2* arg) override;
    zxdg_exported_v2* exportedV2() override;

    void release() override;
    bool isValid() override;

    WaylandPointer<zxdg_exported_v2, zxdg_exported_v2_destroy> exported;

private:
    static void handleCallback(void* data, zxdg_exported_v2* zxdg_exported_v2, char const* handle);

    static zxdg_exported_v2_listener const s_listener;
};

zxdg_exported_v2* XdgExportedUnstableV2::Private::exportedV2()
{
    return exported;
}

void XdgExportedUnstableV2::Private::release()
{
    exported.release();
}

bool XdgExportedUnstableV2::Private::isValid()
{
    return exported.isValid();
}

zxdg_exported_v2_listener const XdgExportedUnstableV2::Private::s_listener = {
    handleCallback,
};

void XdgExportedUnstableV2::Private::handleCallback(void* data,
                                                    zxdg_exported_v2* zxdg_exported_v2,
                                                    char const* handle)
{
    auto p = reinterpret_cast<XdgExportedUnstableV2::Private*>(data);
    Q_ASSERT(p->exported == zxdg_exported_v2);

    p->handle = handle;
    Q_EMIT p->q->done();
}

XdgExportedUnstableV2::XdgExportedUnstableV2(QObject* parent)
    : XdgExported(new Private(this), parent)
{
}

void XdgExportedUnstableV2::Private::setupV2(zxdg_exported_v2* arg)
{
    Q_ASSERT(arg);
    Q_ASSERT(!exported);
    exported.setup(arg);
    zxdg_exported_v2_add_listener(exported, &s_listener, this);
}

XdgExportedUnstableV2::Private::Private(XdgExportedUnstableV2* q)
    : XdgExported::Private::Private(q)
{
}

XdgExportedUnstableV2::~XdgExportedUnstableV2()
{
}

class Q_DECL_HIDDEN XdgImportedUnstableV2::Private : public XdgImported::Private
{
public:
    Private(XdgImportedUnstableV2* q);

    void setupV2(zxdg_imported_v2* arg) override;
    zxdg_imported_v2* importedV2() override;

    void setParentOf(Surface* surface) override;
    void release() override;
    bool isValid() override;

    WaylandPointer<zxdg_imported_v2, zxdg_imported_v2_destroy> imported;

private:
    static void destroyedCallback(void* data, zxdg_imported_v2* zxdg_imported_v2);

    static zxdg_imported_v2_listener const s_listener;
};

XdgImportedUnstableV2::Private::Private(XdgImportedUnstableV2* q)
    : XdgImported::Private::Private(q)
{
}

zxdg_imported_v2* XdgImportedUnstableV2::Private::importedV2()
{
    return imported;
}

void XdgImportedUnstableV2::Private::release()
{
    imported.release();
}

bool XdgImportedUnstableV2::Private::isValid()
{
    return imported.isValid();
}

void XdgImportedUnstableV2::Private::setParentOf(Surface* surface)
{
    Q_ASSERT(isValid());
    zxdg_imported_v2_set_parent_of(imported, *surface);
}

zxdg_imported_v2_listener const XdgImportedUnstableV2::Private::s_listener = {
    destroyedCallback,
};

void XdgImportedUnstableV2::Private::destroyedCallback(void* data,
                                                       zxdg_imported_v2* zxdg_imported_v2)
{
    auto p = reinterpret_cast<XdgImportedUnstableV2::Private*>(data);
    Q_ASSERT(p->imported == zxdg_imported_v2);

    p->q->release();
    Q_EMIT p->q->importedDestroyed();
}

XdgImportedUnstableV2::XdgImportedUnstableV2(QObject* parent)
    : XdgImported(new Private(this), parent)
{
}

void XdgImportedUnstableV2::Private::setupV2(zxdg_imported_v2* arg)
{
    Q_ASSERT(arg);
    Q_ASSERT(!imported);
    imported.setup(arg);
    zxdg_imported_v2_add_listener(imported, &s_listener, this);
}

XdgImportedUnstableV2::~XdgImportedUnstableV2()
{
}

}
}
