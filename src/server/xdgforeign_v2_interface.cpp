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
#include "xdgforeign_interface.h"

#include "xdgforeign_v2_interface_p.h"

#include "display.h"
#include "global_p.h"
#include "resource_p.h"
#include "surface_interface_p.h"

#include "wayland-xdg-foreign-unstable-v2-server-protocol.h"

#include <QUuid>

namespace Wrapland
{
namespace Server
{

class Q_DECL_HIDDEN XdgExporterV2Interface::Private : public Global::Private
{
public:
    Private(XdgExporterV2Interface *q, Display *d, XdgForeignInterface *foreignInterface);

    XdgForeignInterface *foreignInterface;
    QHash<QString, XdgExportedV2Interface *> exportedSurfaces;

private:
    void bind(wl_client *client, uint32_t version, uint32_t id) override;

    static void unbind(wl_resource *resource);
    static Private *cast(wl_resource *resource) {
        return reinterpret_cast<Private*>(wl_resource_get_user_data(resource));
    }

    static void destroyCallback(wl_client *client, wl_resource *resource);
    static void exportCallback(wl_client *client, wl_resource *resource, uint32_t id,
                               wl_resource * surface);

    XdgExporterV2Interface *q;
    static const struct zxdg_exporter_v2_interface s_interface;
    static const quint32 s_version;
};

const quint32 XdgExporterV2Interface::Private::s_version = 1;

#ifndef DOXYGEN_SHOULD_SKIP_THIS
const struct zxdg_exporter_v2_interface XdgExporterV2Interface::Private::s_interface = {
    destroyCallback,
    exportCallback
};
#endif

XdgExporterV2Interface::XdgExporterV2Interface(Display *display, XdgForeignInterface *parent)
    : Global(new Private(this, display, parent), parent)
{
}

XdgExporterV2Interface::~XdgExporterV2Interface() = default;

XdgExporterV2Interface::Private *XdgExporterV2Interface::d_func() const
{
    return reinterpret_cast<Private*>(d.data());
}

XdgExportedV2Interface *XdgExporterV2Interface::exportedSurface(const QString &handle)
{
    Q_D();

    auto it = d->exportedSurfaces.constFind(handle);
    if (it != d->exportedSurfaces.constEnd()) {
        return it.value();
    }
    return nullptr;
}

void XdgExporterV2Interface::Private::destroyCallback(wl_client *client, wl_resource *resource)
{
    Q_UNUSED(client)
    Q_UNUSED(resource)
}

void XdgExporterV2Interface::Private::exportCallback(wl_client *client, wl_resource *resource,
                                                     uint32_t id, wl_resource * surface)
{
    auto *priv = cast(resource);
    QPointer <XdgExportedV2Interface> exported = new XdgExportedV2Interface(priv->q, surface);

    exported->create(priv->display->getConnection(client), wl_resource_get_version(resource), id);

    if (!exported->resource()) {
        wl_resource_post_no_memory(resource);
        delete exported;
        return;
    }

    const QString handle = QUuid::createUuid().toString();

    // Surface not exported anymore.
    connect(exported.data(), &XdgExportedV2Interface::unbound,
            priv->q, [priv, handle]() {
                priv->exportedSurfaces.remove(handle);
            });

    // If the surface dies before this, the export dies too.
    connect(SurfaceInterface::get(surface), &Resource::unbound,
            priv->q, [priv, exported, handle]() {
                if (exported) {
                    exported->deleteLater();
                }
                priv->exportedSurfaces.remove(handle);
            });

    priv->exportedSurfaces[handle] = exported;
    zxdg_exported_v2_send_handle(exported->resource(), handle.toUtf8().constData());
}

XdgExporterV2Interface::Private::Private(XdgExporterV2Interface *q, Display *d,
                                         XdgForeignInterface *foreignInterface)
    : Global::Private(d, &zxdg_exporter_v2_interface, s_version)
    , foreignInterface(foreignInterface)
    , q(q)
{
}

void XdgExporterV2Interface::Private::bind(wl_client *client, uint32_t version, uint32_t id)
{
    auto *con = display->getConnection(client);
    wl_resource *resource = con->createResource(&zxdg_exporter_v2_interface,
                                                qMin(version, s_version), id);
    if (!resource) {
        wl_client_post_no_memory(client);
        return;
    }
    wl_resource_set_implementation(resource, &s_interface, this, unbind);
    // TODO: should we track?
}

void XdgExporterV2Interface::Private::unbind(wl_resource *resource)
{
    Q_UNUSED(resource)
    // TODO: implement?
}


class Q_DECL_HIDDEN XdgImporterV2Interface::Private : public Global::Private
{
public:
    Private(XdgImporterV2Interface *q, Display *d, XdgForeignInterface *foreignInterface);

    XdgForeignInterface *foreignInterface;

    QHash<QString, XdgImportedV2Interface *> importedSurfaces;

    // Child -> parent hash
    QHash<SurfaceInterface *, XdgImportedV2Interface *> parents;
    // Parent -> child hash
    QHash<XdgImportedV2Interface *, SurfaceInterface *> children;

private:
    void bind(wl_client *client, uint32_t version, uint32_t id) override;

    static void unbind(wl_resource *resource);
    static Private *cast(wl_resource *resource) {
        return reinterpret_cast<Private*>(wl_resource_get_user_data(resource));
    }

    static void destroyCallback(wl_client *client, wl_resource *resource);
    static void importCallback(wl_client *client, wl_resource *resource, uint32_t id,
                               const char * handle);

    XdgImporterV2Interface *q;
    static const struct zxdg_importer_v2_interface s_interface;
    static const quint32 s_version;
};

const quint32 XdgImporterV2Interface::Private::s_version = 1;

#ifndef DOXYGEN_SHOULD_SKIP_THIS
const struct zxdg_importer_v2_interface XdgImporterV2Interface::Private::s_interface = {
    destroyCallback,
    importCallback
};
#endif

XdgImporterV2Interface::XdgImporterV2Interface(Display *display, XdgForeignInterface *parent)
    : Global(new Private(this, display, parent), parent)
{
}

XdgImporterV2Interface::~XdgImporterV2Interface() = default;

XdgImportedV2Interface *XdgImporterV2Interface::importedSurface(const QString &handle)
{
    Q_D();

    auto it = d->importedSurfaces.constFind(handle);
    if (it != d->importedSurfaces.constEnd()) {
        return it.value();
    }
    return nullptr;
}

SurfaceInterface *XdgImporterV2Interface::transientFor(SurfaceInterface *surface)
{
    Q_D();

    auto it = d->parents.constFind(surface);
    if (it == d->parents.constEnd()) {
        return nullptr;
    }
    return SurfaceInterface::get((*it)->parentResource());
}

XdgImporterV2Interface::Private *XdgImporterV2Interface::d_func() const
{
    return reinterpret_cast<Private*>(d.data());
}

void XdgImporterV2Interface::Private::destroyCallback(wl_client *client, wl_resource *resource)
{
    Q_UNUSED(client)
    Q_UNUSED(resource)
}

void XdgImporterV2Interface::Private::importCallback(wl_client *client, wl_resource *resource,
                                                     uint32_t id, const char *h)
{
    auto *priv = cast(resource);

    Q_ASSERT(priv->foreignInterface);
    const QString handle = QString::fromUtf8(h);

    XdgExportedV2Interface *exported = priv->foreignInterface->d->exporter->exportedSurface(handle);
    if (!exported) {
        zxdg_imported_v2_send_destroyed(resource);
        return;
    }

    wl_resource *surface = exported->parentResource();
    if (!surface) {
        zxdg_imported_v2_send_destroyed(resource);
        return;
    }

    QPointer<XdgImportedV2Interface> imported = new XdgImportedV2Interface(priv->q, surface);
    imported->create(priv->display->getConnection(client), wl_resource_get_version(resource), id);

    // Surface no longer exported.
    connect(exported, &XdgExportedV2Interface::unbound,
            priv->q, [priv, imported, handle]() {
                // Imported valid when the exported is deleted before the imported.
                if (imported) {
                    zxdg_imported_v2_send_destroyed(imported->resource());
                    imported->deleteLater();
                }
                priv->importedSurfaces.remove(handle);
                Q_EMIT priv->q->surfaceUnimported(handle);
            });

    connect(imported.data(), &XdgImportedV2Interface::childChanged,
            priv->q, [priv, imported](SurfaceInterface *child) {
                // Remove any previous association.
                auto it = priv->children.find(imported);
                if (it != priv->children.end()) {
                    priv->parents.remove(*it);
                    priv->children.erase(it);
                }

                priv->parents[child] = imported;
                priv->children[imported] = child;
                SurfaceInterface *parent = SurfaceInterface::get(imported->parentResource());
                Q_EMIT priv->q->transientChanged(child, parent);

                // Child surface destroyed.
                connect(child, &Resource::unbound,
                        priv->q, [priv, child]() {
                            auto it = priv->parents.find(child);
                            if (it != priv->parents.end()) {
                                Wrapland::Server::XdgImportedV2Interface* parent = *it;
                                priv->children.remove(*it);
                                priv->parents.erase(it);
                                Q_EMIT priv->q->transientChanged(nullptr,
                                                SurfaceInterface::get(parent->parentResource()));
                            }
                        });
            });

    // Surface no longer imported.
    connect(imported.data(), &XdgImportedV2Interface::unbound,
            priv->q, [priv, handle, imported]() {
                priv->importedSurfaces.remove(handle);
                Q_EMIT priv->q->surfaceUnimported(handle);

                auto it = priv->children.find(imported);
                if (it != priv->children.end()) {
                    Wrapland::Server::SurfaceInterface* child = *it;
                    priv->parents.remove(*it);
                    priv->children.erase(it);
                    Q_EMIT priv->q->transientChanged(child, nullptr);
                }
            });

    if (!imported->resource()) {
        wl_resource_post_no_memory(resource);
        delete imported;
        return;
    }

    priv->importedSurfaces[handle] = imported;
    Q_EMIT priv->q->surfaceImported(handle, imported);
}

XdgImporterV2Interface::Private::Private(XdgImporterV2Interface *q, Display *d,
                                         XdgForeignInterface *foreignInterface)
    : Global::Private(d, &zxdg_importer_v2_interface, s_version)
    , foreignInterface(foreignInterface)
    , q(q)
{
}

void XdgImporterV2Interface::Private::bind(wl_client *client, uint32_t version, uint32_t id)
{
    auto *con = display->getConnection(client);
    wl_resource *resource = con->createResource(&zxdg_importer_v2_interface,
                                                qMin(version, s_version), id);
    if (!resource) {
        wl_client_post_no_memory(client);
        return;
    }
    wl_resource_set_implementation(resource, &s_interface, this, unbind);
    // TODO: should we track?
}

void XdgImporterV2Interface::Private::unbind(wl_resource *resource)
{
    Q_UNUSED(resource)
    // TODO: implement?
}


class Q_DECL_HIDDEN XdgExportedV2Interface::Private : public Resource::Private
{
public:
    Private(XdgExportedV2Interface *q, XdgExporterV2Interface *c, wl_resource *parentResource);

private:

    XdgExportedV2Interface *q_func() {
        return reinterpret_cast<XdgExportedV2Interface *>(q);
    }

    static const struct zxdg_exported_v2_interface s_interface;
};

#ifndef DOXYGEN_SHOULD_SKIP_THIS
const struct zxdg_exported_v2_interface XdgExportedV2Interface::Private::s_interface = {
    resourceDestroyedCallback
};
#endif

XdgExportedV2Interface::XdgExportedV2Interface(XdgExporterV2Interface *parent,
                                               wl_resource *parentResource)
    : Resource(new Private(this, parent, parentResource))
{
}

XdgExportedV2Interface::~XdgExportedV2Interface() = default;

XdgExportedV2Interface::Private *XdgExportedV2Interface::d_func() const
{
    return reinterpret_cast<Private*>(d.data());
}

XdgExportedV2Interface::Private::Private(XdgExportedV2Interface *q, XdgExporterV2Interface *c,
                                         wl_resource *parentResource)
    : Resource::Private(q, c, parentResource, &zxdg_exported_v2_interface, &s_interface)
{
}


class Q_DECL_HIDDEN XdgImportedV2Interface::Private : public Resource::Private
{
public:
    Private(XdgImportedV2Interface *q, XdgImporterV2Interface *importer,
            wl_resource *parentResource);

    QPointer<SurfaceInterface> parentOf;

private:
    static void setParentOfCallback(wl_client *client, wl_resource *resource,
                                    wl_resource * surface);

    XdgImportedV2Interface *q_func() {
        return reinterpret_cast<XdgImportedV2Interface *>(q);
    }

    static const struct zxdg_imported_v2_interface s_interface;
};

#ifndef DOXYGEN_SHOULD_SKIP_THIS
const struct zxdg_imported_v2_interface XdgImportedV2Interface::Private::s_interface = {
    resourceDestroyedCallback,
    setParentOfCallback
};
#endif

XdgImportedV2Interface::XdgImportedV2Interface(XdgImporterV2Interface *parent,
                                               wl_resource *parentResource)
    : Resource(new Private(this, parent, parentResource))
{
}

XdgImportedV2Interface::~XdgImportedV2Interface() = default;

XdgImportedV2Interface::Private *XdgImportedV2Interface::d_func() const
{
    return reinterpret_cast<Private*>(d.data());
}

SurfaceInterface *XdgImportedV2Interface::child() const
{
    Q_D();
    return d->parentOf.data(); 
}

void XdgImportedV2Interface::Private::setParentOfCallback(wl_client *client, wl_resource *resource,
                                                          wl_resource * surface)
{
    Q_UNUSED(client)

    auto *priv = cast<Private>(resource);
    auto *surfaceInterface = SurfaceInterface::get(surface);

    if (!surfaceInterface) {
        return;
    }

    priv->parentOf = surfaceInterface;
    Q_EMIT priv->q_func()->childChanged(surfaceInterface);
}

XdgImportedV2Interface::Private::Private(XdgImportedV2Interface *q,
                                         XdgImporterV2Interface *importer,
                                         wl_resource *parentResource)
    : Resource::Private(q, importer, parentResource, &zxdg_imported_v2_interface, &s_interface)
{
}

}
}

