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

#include "wl_output_p.h"

namespace Wrapland::Server
{

const struct zxdg_output_manager_v1_interface XdgOutputManager::Private::s_interface = {
    resourceDestroyCallback,
    cb<getXdgOutputCallback>,
};

XdgOutputManager::Private::Private(Display* display, XdgOutputManager* qptr)
    : XdgOutputManagerGlobal(qptr, display, &zxdg_output_manager_v1_interface, &s_interface)
{
    create();
}

XdgOutputManager::XdgOutputManager(Display* display)
    : d_ptr(new Private(display, this))
{
}

XdgOutputManager::~XdgOutputManager() = default;

void XdgOutputManager::Private::getXdgOutputCallback(XdgOutputManagerBind* bind,
                                                     uint32_t id,
                                                     wl_resource* outputResource)
{
    auto priv = bind->global()->handle()->d_ptr.get();

    auto output_handle = WlOutputGlobal::handle(outputResource);
    if (!output_handle) {
        // Output client is requesting XdgOutput for an wl_output that doesn't exist.
        return;
    }

    auto output = output_handle->output();

    if (priv->outputs.find(output) == priv->outputs.end()) {
        // Server hasn't created an XdgOutput for this output yet, give the client nothing.
        return;
    }

    auto xdgOutputV1 = new XdgOutputV1(bind->client()->handle(), bind->version(), id);
    if (!xdgOutputV1->d_ptr->resource()) {
        bind->post_no_memory();
        delete xdgOutputV1;
        return;
    }

    auto xdgOutput = priv->outputs[output];
    xdgOutput->d_ptr->resourceConnected(xdgOutputV1);
    connect(xdgOutputV1, &XdgOutputV1::resourceDestroyed, xdgOutput, [xdgOutput, xdgOutputV1]() {
        xdgOutput->d_ptr->resourceDisconnected(xdgOutputV1);
    });
}

XdgOutput::Private::Private(Output* output, Display* display, XdgOutput* q)
    : output{output}
    , manager{display->xdgOutputManager()}
{
    assert(manager->d_ptr->outputs.find(output) == manager->d_ptr->outputs.end());
    manager->d_ptr->outputs[output] = q;
}

bool XdgOutput::Private::broadcast()
{
    auto const published = output->d_ptr->published;
    auto const pending = output->d_ptr->pending;

    bool changed = false;

    if (published.geometry.topLeft() != pending.geometry.topLeft()) {
        for (auto resource : resources) {
            resource->send_logical_position(pending.geometry.topLeft());
        }
        changed = true;
    }

    if (published.geometry.size() != pending.geometry.size()) {
        for (auto resource : resources) {
            resource->send_logical_size(pending.geometry.size());
        }
        changed = true;
    }

    if (!sent_once) {
        sent_once = true;
        changed = true;
        for (auto resource : resources) {
            resource->send_name(pending.info.name);
            if (resource->d_ptr->version() < 3) {
                resource->send_description(pending.info.description);
            }
        }
    }

    if (published.info.description != pending.info.description) {
        for (auto resource : resources) {
            if (resource->d_ptr->version() >= 3) {
                resource->send_description(pending.info.description);
            }
        }
        changed = true;
    }

    return changed;
}

void XdgOutput::Private::done()
{
    for (auto resource : resources) {
        resource->done();
    }
}

void XdgOutput::Private::resourceConnected(XdgOutputV1* resource)
{
    auto const& state = output->d_ptr->published;

    auto const geo = state.geometry;
    resource->send_logical_position(geo.topLeft());
    resource->send_logical_size(geo.size());

    resource->send_name(state.info.name);
    resource->send_description(state.info.description);

    if (resource->d_ptr->version() < 3) {
        resource->done();
    } else {
        output->d_ptr->done_wl(resource->d_ptr->client()->handle());
    }
    resources.push_back(resource);
}

void XdgOutput::Private::resourceDisconnected(XdgOutputV1* resource)
{
    resources.erase(std::remove(resources.begin(), resources.end(), resource), resources.end());
}

XdgOutput::XdgOutput(Output* output, Display* display)
    : QObject(nullptr)
    , d_ptr(new XdgOutput::Private(output, display, this))
{
    auto manager = display->xdgOutputManager();
    connect(this, &QObject::destroyed, manager, [manager, output]() {
        manager->d_ptr->outputs.erase(output);
    });
}

XdgOutput::~XdgOutput() = default;

XdgOutputV1::Private::Private(Client* client, uint32_t version, uint32_t id, XdgOutputV1* q)
    : Wayland::Resource<XdgOutputV1>(client,
                                     version,
                                     id,
                                     &zxdg_output_v1_interface,
                                     &s_interface,
                                     q)
{
}

const struct zxdg_output_v1_interface XdgOutputV1::Private::s_interface = {destroyCallback};

XdgOutputV1::XdgOutputV1(Client* client, uint32_t version, uint32_t id)
    : QObject(nullptr)
    , d_ptr(new XdgOutputV1::Private(client, version, id, this))
{
}

void XdgOutputV1::send_logical_position(QPointF const& pos) const
{
    d_ptr->send<zxdg_output_v1_send_logical_position>(pos.x(), pos.y());
}

void XdgOutputV1::send_logical_size(QSizeF const& size) const
{
    d_ptr->send<zxdg_output_v1_send_logical_size>(size.width(), size.height());
}

void XdgOutputV1::send_name(std::string const& name) const
{
    if (d_ptr->version() >= ZXDG_OUTPUT_V1_NAME_SINCE_VERSION) {
        d_ptr->send<zxdg_output_v1_send_name>(name.c_str());
    }
}

void XdgOutputV1::send_description(std::string const& description) const
{
    if (d_ptr->version() >= ZXDG_OUTPUT_V1_DESCRIPTION_SINCE_VERSION) {
        d_ptr->send<zxdg_output_v1_send_description>(description.c_str());
    }
}

void XdgOutputV1::done() const
{
    if (d_ptr->version() < 3) {
        d_ptr->send<zxdg_output_v1_send_done>();
    }
}

}
