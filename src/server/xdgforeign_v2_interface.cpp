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
    Private(XdgExporterV2Interface *q, Display *d);

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

XdgExporterV2Interface::XdgExporterV2Interface(Display *display, QObject *parent)
    : Global(new Private(this, display), parent)
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

    auto *surfaceInterface = SurfaceInterface::get(surface);
    Q_ASSERT(surfaceInterface);

    const QString handle = QUuid::createUuid().toString();

    auto *exported = new XdgExportedV2Interface(priv->q, surfaceInterface);
    exported->create(priv->display->getConnection(client), wl_resource_get_version(resource), id);

    if (!exported->resource()) {
        wl_resource_post_no_memory(resource);
        delete exported;
        return;
    }

    // Surface not exported anymore.
    connect(exported, &XdgExportedV2Interface::destroyed,
            priv->q, [priv, handle] { priv->exportedSurfaces.remove(handle); }
    );

    priv->exportedSurfaces[handle] = exported;
    zxdg_exported_v2_send_handle(exported->resource(), handle.toUtf8().constData());
}

XdgExporterV2Interface::Private::Private(XdgExporterV2Interface *q, Display *d)
    : Global::Private(d, &zxdg_exporter_v2_interface, s_version)
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
    Private(XdgImporterV2Interface *q, Display *d);

    void parentChange(XdgImportedV2Interface *imported, XdgExportedV2Interface *exported);

    XdgExporterV2Interface *exporter = nullptr;

    // Child -> parent hash
    QHash<SurfaceInterface*, SurfaceInterface*> parents;

private:
    void childChange(SurfaceInterface *parent,
                     SurfaceInterface *prevChild, SurfaceInterface *nextChild);

    void bind(wl_client *client, uint32_t version, uint32_t id) override;

    static void unbind(wl_resource *resource);
    static Private *cast(wl_resource *resource) {
        return reinterpret_cast<Private*>(wl_resource_get_user_data(resource));
    }

    static void destroyCallback(wl_client *client, wl_resource *resource);
    static void importCallback(wl_client *client, wl_resource *resource, uint32_t id,
                               const char *handle);

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

XdgImporterV2Interface::XdgImporterV2Interface(Display *display, QObject *parent)
    : Global(new Private(this, display), parent)
{
}

XdgImporterV2Interface::~XdgImporterV2Interface() = default;

void XdgImporterV2Interface::setExporter(XdgExporterV2Interface *exporter)
{
    Q_D();
    d->exporter = exporter;
}

SurfaceInterface *XdgImporterV2Interface::parentOf(SurfaceInterface *surface)
{
    Q_D();

    if (!d->parents.contains(surface)) {
        return nullptr;
    }
    return d->parents[surface];
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
                                                     uint32_t id, const char *handle)
{
    auto *priv = cast(resource);

    if (!priv->exporter) {
        zxdg_imported_v2_send_destroyed(resource);
        return;
    }

    auto *exported = priv->exporter->exportedSurface(QString::fromUtf8(handle));
    if (!exported) {
        zxdg_imported_v2_send_destroyed(resource);
        return;
    }

    wl_resource *surface = exported->parentResource();
    if (!surface) {
        zxdg_imported_v2_send_destroyed(resource);
        return;
    }

    auto *imported = new XdgImportedV2Interface(priv->q, surface, exported);
    imported->create(priv->display->getConnection(client), wl_resource_get_version(resource), id);

    if (!imported->resource()) {
        wl_resource_post_no_memory(resource);
        delete imported;
        return;
    }

    connect(imported, &XdgImportedV2Interface::childChanged,
            priv->q, [priv] (SurfaceInterface *parent, SurfaceInterface *prevChild,
                             SurfaceInterface *nextChild) {
                priv->childChange(parent, prevChild, nextChild);
            }
    );
}

XdgImporterV2Interface::Private::Private(XdgImporterV2Interface *q, Display *d)
    : Global::Private(d, &zxdg_importer_v2_interface, s_version)
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

void XdgImporterV2Interface::Private::childChange(SurfaceInterface *parent,
                                                  SurfaceInterface *prevChild,
                                                  SurfaceInterface *nextChild)
{
    if (prevChild) {
        int removed = parents.remove(prevChild);
        Q_ASSERT(removed);
    }
    if (nextChild && parent) {
        parents[nextChild] = parent;
    }

    Q_EMIT q->parentChanged(parent, nextChild);
}


class Q_DECL_HIDDEN XdgExportedV2Interface::Private : public Resource::Private
{
public:
    Private(XdgExportedV2Interface *q, XdgExporterV2Interface *c, SurfaceInterface *surface);

    SurfaceInterface *exportedSurface;

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
                                               SurfaceInterface *surface)
    : Resource(new Private(this, parent, surface))
{
    // If the surface dies, the export dies too.
    connect(surface, &SurfaceInterface::aboutToBeUnbound,
            this, [this]() { delete this; } );
}

XdgExportedV2Interface::~XdgExportedV2Interface() = default;

SurfaceInterface* XdgExportedV2Interface::surface() const
{
    Q_D();
    return d->exportedSurface;
}

XdgExportedV2Interface::Private *XdgExportedV2Interface::d_func() const
{
    return reinterpret_cast<Private*>(d.data());
}

XdgExportedV2Interface::Private::Private(XdgExportedV2Interface *q, XdgExporterV2Interface *c,
                                         SurfaceInterface *surface)
    : Resource::Private(q, c, surface->resource(), &zxdg_exported_v2_interface, &s_interface)
    , exportedSurface(surface)
{
}


class Q_DECL_HIDDEN XdgImportedV2Interface::Private : public Resource::Private
{
public:
    Private(XdgImportedV2Interface *q, XdgImporterV2Interface *importer,
            wl_resource *parentResource, XdgExportedV2Interface *exported);
    ~Private();

    void setChild(SurfaceInterface *surface);

    XdgExportedV2Interface *source = nullptr;
    SurfaceInterface *child = nullptr;

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
                                               wl_resource *parentResource,
                                               XdgExportedV2Interface *exported)
    : Resource(new Private(this, parent, parentResource, exported))
{
    connect(exported, &QObject::destroyed, this, [this] {
            // Export no longer available.
            Q_D();
            d->source = nullptr;
            d->setChild(child());
            zxdg_imported_v2_send_destroyed(resource());
            delete this;
        }
    );
}

XdgImportedV2Interface::~XdgImportedV2Interface()
{
    Q_D();

    if (source()) {
        d->setChild(nullptr);
    }

}

XdgImportedV2Interface::Private *XdgImportedV2Interface::d_func() const
{
    return reinterpret_cast<Private*>(d.data());
}

SurfaceInterface *XdgImportedV2Interface::child() const
{
    Q_D();
    return d->child;
}

XdgExportedV2Interface* XdgImportedV2Interface::source() const
{
    Q_D();
    return d->source;
}

void XdgImportedV2Interface::Private::setParentOfCallback(wl_client *client, wl_resource *resource,
                                                          wl_resource * surface)
{
    Q_UNUSED(client)

    auto *priv = cast<Private>(resource);
    auto *surfaceInterface = SurfaceInterface::get(surface);

    // Guaranteed by libwayland (?).
    Q_ASSERT(surfaceInterface);

    if (priv->child != surfaceInterface) {
        // Only set on change. We do the check here so the setChild function can be reused
        // for setting the same child when the exporting source goes away.
        priv->setChild(surfaceInterface);
    }
}

void XdgImportedV2Interface::Private::setChild(SurfaceInterface *surface)
{
    auto *prevChild = child;
    if (prevChild) {
        disconnect(prevChild, &SurfaceInterface::aboutToBeUnbound, q_func(), nullptr);
    }

    child = surface;

    connect(surface, &SurfaceInterface::aboutToBeUnbound, q_func(), [this] {
        // Child surface is destroyed, this means relation is cancelled.
        Q_EMIT q_func()->childChanged(source->surface(), this->child, nullptr);
        this->child = nullptr;
    });

    Q_EMIT q_func()->childChanged(source ? source->surface() : nullptr, prevChild, surface);
}

XdgImportedV2Interface::Private::Private(XdgImportedV2Interface *q,
                                         XdgImporterV2Interface *importer,
                                         wl_resource *parentResource,
                                         XdgExportedV2Interface *exported)
    : Resource::Private(q, importer, parentResource, &zxdg_imported_v2_interface, &s_interface)
    , source(exported)
{
}

XdgImportedV2Interface::Private::~Private() = default;

}
}

