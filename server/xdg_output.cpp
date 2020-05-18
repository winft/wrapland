/****************************************************************************
Copyright 2020  Adrien Faveraux <ad1rie3@hotmail.fr>

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
#include "xdg_output_p.h"

#include "output_p.h"

namespace Wrapland::Server
{

const struct zxdg_output_manager_v1_interface XdgOutputManager::Private::s_interface = {
    resourceDestroyCallback,
    getXdgOutputCallback,
};

XdgOutputManager::Private::Private(Display* display, XdgOutputManager* qptr)
    : Wayland::Global<XdgOutputManager>(qptr,
                                        display,
                                        &zxdg_output_manager_v1_interface,
                                        &s_interface)
{
    create();
}

XdgOutputManager::XdgOutputManager(Display* display, QObject* parent)
    : QObject(parent)
    , d_ptr(new Private(display, this))
{
}

XdgOutputManager::~XdgOutputManager() = default;

XdgOutput* XdgOutputManager::createXdgOutput(Output* output, QObject* parent)
{
    if (d_ptr->outputs.find(output) == d_ptr->outputs.end()) {
        auto xdgOutput = new XdgOutput(parent);
        d_ptr->outputs[output] = xdgOutput;

        // As XdgOutput lifespan is managed by user, delete our mapping when either.
        // It or the relevant Output gets deleted.
        connect(
            output, &QObject::destroyed, this, [this, output]() { d_ptr->outputs.erase(output); });
        connect(xdgOutput, &QObject::destroyed, this, [this, output]() {
            d_ptr->outputs.erase(output);
        });
    }
    return d_ptr->outputs[output];
}

void XdgOutputManager::Private::getXdgOutputCallback([[maybe_unused]] wl_client* wlClient,
                                                     wl_resource* wlResource,
                                                     uint32_t id,
                                                     wl_resource* outputResource)
{
    auto priv = handle(wlResource)->d_ptr.get();
    auto bind = priv->getBind(wlResource);

    // Output client is requesting XdgOutput for an Output that doesn't exist.
    if (!outputResource) {
        return;
    }

    auto output = OutputGlobal::handle(outputResource);

    if (priv->outputs.find(output) == priv->outputs.end()) {
        // Server hasn't created an XdgOutput for this output yet, give the client nothing.
        return;
    }

    auto xdgOutputV1
        = new XdgOutputV1(bind->client()->handle(), bind->version(), id, priv->handle());
    if (!xdgOutputV1->d_ptr->resource()) {
        wl_resource_post_no_memory(wlResource);
        delete xdgOutputV1;
        return;
    }

    auto xdgOutput = priv->outputs[output];
    xdgOutput->d_ptr->resourceConnected(xdgOutputV1);
    connect(xdgOutputV1, &XdgOutputV1::resourceDestroyed, xdgOutput, [xdgOutput, xdgOutputV1]() {
        xdgOutput->d_ptr->resourceDisconnected(xdgOutputV1);
    });
}

XdgOutput::XdgOutput(QObject* parent)
    : QObject(parent)
    , d_ptr(new XdgOutput::Private)
{
}

XdgOutput::~XdgOutput() = default;

void XdgOutput::setLogicalSize(const QSize& size)
{
    if (size == d_ptr->size) {
        return;
    }
    d_ptr->size = size;
    d_ptr->dirty = true;
    for (auto resource : d_ptr->resources) {
        resource->setLogicalSize(size);
    }
}

QSize XdgOutput::logicalSize() const
{
    return d_ptr->size;
}

void XdgOutput::setLogicalPosition(const QPoint& pos)
{
    if (pos == d_ptr->pos) {
        return;
    }
    d_ptr->pos = pos;
    d_ptr->dirty = true;
    for (auto resource : d_ptr->resources) {
        resource->setLogicalPosition(pos);
    }
}

QPoint XdgOutput::logicalPosition() const
{
    return d_ptr->pos;
}

void XdgOutput::done()
{
    d_ptr->doneOnce = true;
    if (!d_ptr->dirty) {
        return;
    }
    d_ptr->dirty = false;
    for (auto resource : d_ptr->resources) {
        resource->done();
    }
}

void XdgOutput::Private::resourceConnected(XdgOutputV1* resource)
{
    resource->setLogicalPosition(pos);
    resource->setLogicalSize(size);
    if (doneOnce) {
        resource->done();
    }
    resources.push_back(resource);
}

void XdgOutput::Private::resourceDisconnected(XdgOutputV1* resource)
{
    resources.erase(std::remove(resources.begin(), resources.end(), resource), resources.end());
}

const struct zxdg_output_v1_interface XdgOutputV1::Private::s_interface = {destroyCallback};
XdgOutputV1::Private::Private(Client* client, uint32_t version, uint32_t id, XdgOutputV1* q)
    : Wayland::Resource<XdgOutputV1>(client,
                                     version,
                                     id,
                                     &zxdg_output_v1_interface,
                                     &s_interface,
                                     q)
{
}

XdgOutputV1::XdgOutputV1(Client* client, uint32_t version, uint32_t id, XdgOutputManager* parent)
    : QObject(parent)
    , d_ptr(new XdgOutputV1::Private(client, version, id, this))
{
}

void XdgOutputV1::setLogicalSize(const QSize& size)
{
    d_ptr->send<zxdg_output_v1_send_logical_size>(size.width(), size.height());
}

void XdgOutputV1::setLogicalPosition(const QPoint& pos)
{
    d_ptr->send<zxdg_output_v1_send_logical_position>(pos.x(), pos.y());
}

void XdgOutputV1::done()
{
    d_ptr->send<zxdg_output_v1_send_done>();
}

}
