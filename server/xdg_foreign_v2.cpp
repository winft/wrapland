/****************************************************************************
Copyright 2017  Marco Martin <notmart@gmail.com>
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
#include "xdg_foreign.h"

#include "xdg_foreign_v2_p.h"

#include "display.h"
#include "surface_p.h"

#include "wayland/global.h"

#include "wayland-xdg-foreign-unstable-v2-server-protocol.h"

#include <QUuid>

namespace Wrapland
{
namespace Server
{

class Q_DECL_HIDDEN XdgExporterV2::Private : public Wayland::Global<XdgExporterV2>
{
public:
    Private(XdgExporterV2* q, D_isplay* display);
    ~Private() override = default;

    QHash<QString, XdgExportedV2*> exportedSurfaces;

private:
    static void exportCallback(wl_client* wlClient,
                               wl_resource* wlResource,
                               uint32_t id,
                               wl_resource* wlSurface);

    static const struct zxdg_exporter_v2_interface s_interface;
    static const quint32 s_version;
};

const quint32 XdgExporterV2::Private::s_version = 1;

#ifndef DOXYGEN_SHOULD_SKIP_THIS
const struct zxdg_exporter_v2_interface XdgExporterV2::Private::s_interface = {
    resourceDestroyCallback,
    exportCallback,
};
#endif

XdgExporterV2::XdgExporterV2(D_isplay* display, QObject* parent)
    : QObject(parent)
    , d_ptr(new Private(this, display))
{
}

XdgExporterV2::~XdgExporterV2()
{
    delete d_ptr;
}

XdgExportedV2* XdgExporterV2::exportedSurface(const QString& handle)
{
    auto it = d_ptr->exportedSurfaces.constFind(handle);
    if (it != d_ptr->exportedSurfaces.constEnd()) {
        return it.value();
    }
    return nullptr;
}
void XdgExporterV2::Private::exportCallback(wl_client* wlClient,
                                            wl_resource* wlResource,
                                            uint32_t id,
                                            wl_resource* wlSurface)
{
    auto handle = fromResource(wlResource);
    auto client = handle->d_ptr->display()->handle()->getClient(wlClient);
    auto priv = handle->d_ptr;
    auto surface = Wayland::Resource<Surface>::fromResource(wlSurface)->handle();

    const QString protocolHandle = QUuid::createUuid().toString();

    auto exported = new XdgExportedV2(client, wl_resource_get_version(wlResource), id, surface);
    // TODO: error handling

    // Surface not exported anymore.
    connect(exported, &XdgExportedV2::destroyed, handle, [priv, protocolHandle] {
        priv->exportedSurfaces.remove(protocolHandle);
    });

    priv->exportedSurfaces[protocolHandle] = exported;
    priv->send<zxdg_exported_v2_send_handle>(protocolHandle.toUtf8().constData());
}

XdgExporterV2::Private::Private(XdgExporterV2* q, D_isplay* display)
    : Wayland::Global<XdgExporterV2>(q, display, &zxdg_exporter_v2_interface, &s_interface)
{
}

class Q_DECL_HIDDEN XdgImporterV2::Private : public Wayland::Global<XdgImporterV2>
{
public:
    Private(XdgImporterV2* q, D_isplay* display);

    void parentChange(XdgImportedV2* imported, XdgExportedV2* exported);

    XdgExporterV2* exporter = nullptr;

    // Child -> parent hash
    QHash<Surface*, Surface*> parents;

private:
    void childChange(Surface* parent, Surface* prevChild, Surface* nextChild);

    static void
    importCallback(wl_client* wlClient, wl_resource* wlResource, uint32_t id, const char* handle);

    static const struct zxdg_importer_v2_interface s_interface;
    static const quint32 s_version;
};

const quint32 XdgImporterV2::Private::s_version = 1;

const struct zxdg_importer_v2_interface XdgImporterV2::Private::s_interface
    = {resourceDestroyCallback, importCallback};

XdgImporterV2::XdgImporterV2(D_isplay* display, QObject* parent)
    : QObject(parent)
    , d_ptr(new Private(this, display))
{
}

XdgImporterV2::~XdgImporterV2()
{
    delete d_ptr;
}

void XdgImporterV2::setExporter(XdgExporterV2* exporter)
{
    d_ptr->exporter = exporter;
}

Surface* XdgImporterV2::parentOf(Surface* surface)
{
    if (!d_ptr->parents.contains(surface)) {
        return nullptr;
    }
    return d_ptr->parents[surface];
}

void XdgImporterV2::Private::importCallback(wl_client* wlClient,
                                            wl_resource* wlResource,
                                            uint32_t id,
                                            const char* handle)
{
    auto importerHandle = fromResource(wlResource);
    auto client = importerHandle->d_ptr->display()->handle()->getClient(wlClient);

    if (!importerHandle->d_ptr->exporter) {
        importerHandle->d_ptr->send<zxdg_imported_v2_send_destroyed>();
        return;
    }

    auto* exported = importerHandle->d_ptr->exporter->exportedSurface(QString::fromUtf8(handle));
    if (!exported) {
        importerHandle->d_ptr->send<zxdg_imported_v2_send_destroyed>();
        return;
    }

    if (!exported->surface()) {
        importerHandle->d_ptr->send<zxdg_imported_v2_send_destroyed>();
        return;
    }

    auto imported = new XdgImportedV2(client, importerHandle->d_ptr->version(), id, exported);
    // TODO: error handling

    connect(imported,
            &XdgImportedV2::childChanged,
            importerHandle,
            [importerHandle](Surface* parent, Surface* prevChild, Surface* nextChild) {
                importerHandle->d_ptr->childChange(parent, prevChild, nextChild);
            });
}

XdgImporterV2::Private::Private(XdgImporterV2* q, D_isplay* display)
    : Wayland::Global<XdgImporterV2>(q, display, &zxdg_importer_v2_interface, &s_interface)
{
}

void XdgImporterV2::Private::childChange(Surface* parent, Surface* prevChild, Surface* nextChild)
{
    if (prevChild) {
        int removed = parents.remove(prevChild);
        Q_ASSERT(removed);
    }
    if (nextChild && parent) {
        parents[nextChild] = parent;
    }

    Q_EMIT handle()->parentChanged(parent, nextChild);
}

class Q_DECL_HIDDEN XdgExportedV2::Private : public Wayland::Resource<XdgExportedV2>
{
public:
    Private(Client* client, uint32_t version, uint32_t id, Surface* surface, XdgExportedV2* q);

    Surface* exportedSurface;

private:
    static const struct zxdg_exported_v2_interface s_interface;
};

const struct zxdg_exported_v2_interface XdgExportedV2::Private::s_interface = {destroyCallback};

XdgExportedV2::XdgExportedV2(Client* client, uint32_t version, uint32_t id, Surface* surface)
    : QObject(nullptr)
    , d_ptr(new Private(client, version, id, surface, this))
{
    // If the surface dies, the export dies too.
    connect(surface, &Surface::resourceDestroyed, this, [this]() { delete this; });
}

XdgExportedV2::~XdgExportedV2() = default;

Surface* XdgExportedV2::surface() const
{
    return d_ptr->exportedSurface;
}

XdgExportedV2::Private::Private(Client* client,
                                uint32_t version,
                                uint32_t id,
                                Surface* surface,
                                XdgExportedV2* q)
    : Wayland::Resource<XdgExportedV2>(client,
                                       version,
                                       id,
                                       &zxdg_exported_v2_interface,
                                       &s_interface,
                                       q)
    , exportedSurface(surface)
{
}

class Q_DECL_HIDDEN XdgImportedV2::Private : public Wayland::Resource<XdgImportedV2>
{
public:
    Private(Client* client,
            uint32_t version,
            uint32_t id,
            XdgExportedV2* exported,
            XdgImportedV2* q);
    ~Private() override = default;

    void setChild(Surface* surface);

    XdgExportedV2* source = nullptr;
    Surface* child = nullptr;

private:
    static void
    setParentOfCallback(wl_client* wlClient, wl_resource* wlResource, wl_resource* wlSurface);

    static const struct zxdg_imported_v2_interface s_interface;
};

const struct zxdg_imported_v2_interface XdgImportedV2::Private::s_interface = {
    destroyCallback,
    setParentOfCallback,
};

XdgImportedV2::XdgImportedV2(Client* client, uint32_t version, uint32_t id, XdgExportedV2* exported)
    : QObject(nullptr)
    , d_ptr(new Private(client, version, id, exported, this))
{
    connect(exported, &XdgExportedV2::resourceDestroyed, this, [this] {
        // Export no longer available.
        d_ptr->source = nullptr;
        d_ptr->setChild(child());
        d_ptr->send<zxdg_imported_v2_send_destroyed>();
        delete this;
    });
}

XdgImportedV2::~XdgImportedV2()
{
    if (source()) {
        d_ptr->setChild(nullptr);
    }
}

Surface* XdgImportedV2::child() const
{
    return d_ptr->child;
}

XdgExportedV2* XdgImportedV2::source() const
{
    return d_ptr->source;
}

void XdgImportedV2::Private::setParentOfCallback([[maybe_unused]] wl_client* wlClient,
                                                 wl_resource* wlResource,
                                                 wl_resource* wlSurface)
{
    auto priv = static_cast<Private*>(fromResource(wlResource));
    auto surface = Wayland::Resource<Surface>::fromResource(wlSurface)->handle();

    // Guaranteed by libwayland (?).
    Q_ASSERT(surface);

    if (priv->child != surface) {
        // Only set on change. We do the check here so the setChild function can be reused
        // for setting the same child when the exporting source goes away.
        priv->setChild(surface);
    }
}

void XdgImportedV2::Private::setChild(Surface* surface)
{
    auto* prevChild = child;
    if (prevChild) {
        disconnect(prevChild, &Surface::resourceDestroyed, handle(), nullptr);
    }

    child = surface;

    connect(surface, &Surface::resourceDestroyed, handle(), [this] {
        // Child surface is destroyed, this means relation is cancelled.
        Q_EMIT handle()->childChanged(source->surface(), this->child, nullptr);
        this->child = nullptr;
    });

    Q_EMIT handle()->childChanged(source ? source->surface() : nullptr, prevChild, surface);
}

XdgImportedV2::Private::Private(Client* client,
                                uint32_t version,
                                uint32_t id,
                                XdgExportedV2* exported,
                                XdgImportedV2* q)
    : Wayland::Resource<XdgImportedV2>(client,
                                       version,
                                       id,
                                       &zxdg_imported_v2_interface,
                                       &s_interface,
                                       q)
    , source(exported)
{
}

}
}
