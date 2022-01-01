/*
    SPDX-FileCopyrightText: 2020 Adrien Faveraux <af@brain-networks.fr>
    SPDX-FileCopyrightText: 2021 Francesco Sorrentino <francesco.sorr@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#include "primary_selection_p.h"

#include "client.h"
#include "data_control_v1_p.h"
#include "display.h"
#include "seat_p.h"
#include "selection_p.h"

namespace Wrapland::Server
{

const struct zwp_primary_selection_device_manager_v1_interface
    primary_selection_device_manager::Private::s_interface
    = {
        cb<create_source>,
        cb<get_device>,
        resourceDestroyCallback,
};

primary_selection_device_manager::Private::Private(Display* display,
                                                   primary_selection_device_manager* q)
    : device_manager<primary_selection_device_manager_global>(
        q,
        display,
        &zwp_primary_selection_device_manager_v1_interface,
        &s_interface)
{
    create();
}

primary_selection_device_manager::Private::~Private() = default;

primary_selection_device_manager::primary_selection_device_manager(Display* display)
    : d_ptr(new Private(display, this))
{
}

primary_selection_device_manager::~primary_selection_device_manager() = default;

void primary_selection_device_manager::create_source(Client* client, uint32_t version, uint32_t id)
{
    auto src_res = new primary_selection_source_res(client, version, id);
    // TODO(romangg): Catch oom.

    Q_EMIT source_created(src_res->src());
}

void primary_selection_device_manager::get_device(Client* client,
                                                  uint32_t version,
                                                  uint32_t id,
                                                  Seat* seat)
{
    auto device = new primary_selection_device(client, version, id, seat);
    if (!device) {
        return;
    }

    seat->d_ptr->primary_selection_devices.register_device(device);
    Q_EMIT device_created(device);
}

const struct zwp_primary_selection_device_v1_interface
    primary_selection_device::Private::s_interface
    = {
        set_selection_callback,
        destroyCallback,
};

primary_selection_device::Private::Private(Client* client,
                                           uint32_t version,
                                           uint32_t id,
                                           Seat* seat,
                                           primary_selection_device* qptr)
    : Wayland::Resource<primary_selection_device>(client,
                                                  version,
                                                  id,
                                                  &zwp_primary_selection_device_v1_interface,
                                                  &s_interface,
                                                  qptr)
    , m_seat(seat)
{
}

primary_selection_device::Private::~Private() = default;

void primary_selection_device::Private::set_selection_callback(wl_client* /*wlClient*/,
                                                               wl_resource* wlResource,
                                                               wl_resource* wlSource,
                                                               uint32_t /*id*/)
{
    // TODO(unknown author): verify serial
    auto handle = Resource::get_handle(wlResource);
    set_selection(handle, handle->d_ptr, wlSource);
}

void primary_selection_device::send_selection(Wrapland::Server::primary_selection_source* source)
{
    if (!source) {
        send_clear_selection();
        return;
    }

    auto offer = d_ptr->sendDataOffer(source);
    if (!offer) {
        return;
    }

    d_ptr->send<zwp_primary_selection_device_v1_send_selection>(offer->d_ptr->resource);
}

void primary_selection_device::send_clear_selection()
{
    d_ptr->send<zwp_primary_selection_device_v1_send_selection>(nullptr);
}

primary_selection_offer*
primary_selection_device::Private::sendDataOffer(primary_selection_source* source)
{
    if (!source) {
        // A data offer can only exist together with a source.
        return nullptr;
    }

    auto offer = new primary_selection_offer(client->handle, version, source);

    if (!offer->d_ptr->resource) {
        delete offer;
        return nullptr;
    }

    send<zwp_primary_selection_device_v1_send_data_offer>(offer->d_ptr->resource);
    offer->send_offer();
    return offer;
}

primary_selection_device::primary_selection_device(Client* client,
                                                   uint32_t version,
                                                   uint32_t id,
                                                   Seat* seat)
    : d_ptr(new Private(client, version, id, seat, this))
{
}

primary_selection_device::~primary_selection_device() = default;

primary_selection_source* primary_selection_device::selection()
{
    return d_ptr->selection;
}

Client* primary_selection_device::client() const
{
    return d_ptr->client->handle;
}

Seat* primary_selection_device::seat() const
{
    return d_ptr->m_seat;
}

const struct zwp_primary_selection_offer_v1_interface primary_selection_offer::Private::s_interface
    = {
        receive_callback,
        destroyCallback,
};

primary_selection_offer::Private::Private(Client* client,
                                          uint32_t version,
                                          primary_selection_source* source,
                                          primary_selection_offer* qptr)
    : Wayland::Resource<primary_selection_offer>(client,
                                                 version,
                                                 0,
                                                 &zwp_primary_selection_offer_v1_interface,
                                                 &s_interface,
                                                 qptr)
    , source(source)
{
}

primary_selection_offer::Private::~Private() = default;

void primary_selection_offer::Private::receive_callback(wl_client* /*wlClient*/,
                                                        wl_resource* wlResource,
                                                        char const* mimeType,
                                                        int32_t fd)
{
    auto handle = Resource::get_handle(wlResource);
    receive_mime_type_offer(handle->d_ptr->source, mimeType, fd);
}

primary_selection_offer::primary_selection_offer(Client* client,
                                                 uint32_t version,
                                                 primary_selection_source* source)
    : d_ptr(new Private(client, version, source, this))
{
    assert(source);
    QObject::connect(source,
                     &primary_selection_source::mime_type_offered,
                     this,
                     [this](std::string const& mimeType) {
                         d_ptr->send<zwp_primary_selection_offer_v1_send_offer>(mimeType.c_str());
                     });
    QObject::connect(source, &primary_selection_source::resourceDestroyed, this, [this] {
        d_ptr->source = nullptr;
    });
}

primary_selection_offer::~primary_selection_offer() = default;

void primary_selection_offer::send_offer()
{
    for (auto const& mimeType : d_ptr->source->mime_types()) {
        d_ptr->send<zwp_primary_selection_offer_v1_send_offer>(mimeType.c_str());
    }
}

primary_selection_source::Private::Private(primary_selection_source* q_ptr)
    : q_ptr{q_ptr}
{
}

primary_selection_source::primary_selection_source()
    : d_ptr(new primary_selection_source::Private(this))
{
}

primary_selection_source_res_impl::primary_selection_source_res_impl(
    Client* client,
    uint32_t version,
    uint32_t id,
    primary_selection_source_res* q_ptr)
    : Wayland::Resource<primary_selection_source_res>(client,
                                                      version,
                                                      id,
                                                      &zwp_primary_selection_source_v1_interface,
                                                      &s_interface,
                                                      q_ptr)
    , q_ptr{q_ptr}
{
}

const struct zwp_primary_selection_source_v1_interface
    primary_selection_source_res_impl::s_interface
    = {
        offer_callback,
        destroyCallback,
};

void primary_selection_source_res_impl::offer_callback(wl_client* /*wlClient*/,
                                                       wl_resource* wlResource,
                                                       char const* mimeType)
{
    auto handle = Resource::get_handle(wlResource);
    offer_mime_type(handle->src_priv(), mimeType);
}

primary_selection_source_res::primary_selection_source_res(Client* client,
                                                           uint32_t version,
                                                           uint32_t id)
    : pub_src{new primary_selection_source}
    , impl{new primary_selection_source_res_impl(client, version, id, this)}
{
    QObject::connect(this,
                     &primary_selection_source_res::resourceDestroyed,
                     src(),
                     &primary_selection_source::resourceDestroyed);
    src_priv()->res = this;
}

void primary_selection_source_res::cancel() const
{
    impl->send<zwp_primary_selection_source_v1_send_cancelled>();
    impl->client->flush();
}

void primary_selection_source_res::request_data(std::string const& mimeType, qint32 fd) const
{
    impl->send<zwp_primary_selection_source_v1_send_send>(mimeType.c_str(), fd);
    close(fd);
}

primary_selection_source* primary_selection_source_res::src() const
{
    return src_priv()->q_ptr;
}

primary_selection_source::Private* primary_selection_source_res::src_priv() const
{
    return pub_src->d_ptr.get();
}

std::vector<std::string> primary_selection_source::mime_types() const
{
    return d_ptr->mimeTypes;
}

void primary_selection_source::cancel() const
{
    std::visit([](auto&& res) { res->cancel(); }, d_ptr->res);
}

void primary_selection_source::request_data(std::string const& mimeType, qint32 fd) const
{
    std::visit([&](auto&& res) { res->request_data(mimeType, fd); }, d_ptr->res);
}

template<class... Ts>
// NOLINTNEXTLINE(fuchsia-multiple-inheritance)
struct overload : Ts... {
    using Ts::operator()...;
};
template<class... Ts>
overload(Ts...) -> overload<Ts...>;

Client* primary_selection_source::client() const
{
    Client* cl{nullptr};

    std::visit(overload{[&](primary_selection_source_res* res) { cl = res->impl->client->handle; },
                        [&](data_control_source_v1_res* res) { cl = res->impl->client->handle; },
                        [&](primary_selection_source_ext* /*unused*/) {}},
               d_ptr->res);

    return cl;
}

primary_selection_source_ext::Private::Private(primary_selection_source_ext* q_ptr)
    : pub_src{new primary_selection_source}
    , q_ptr{q_ptr}
{
    pub_src->d_ptr->res = q_ptr;
}

primary_selection_source_ext::primary_selection_source_ext()
    : d_ptr{new Private(this)}
{
}

primary_selection_source_ext::~primary_selection_source_ext()
{
    Q_EMIT d_ptr->pub_src->resourceDestroyed();
}

void primary_selection_source_ext::offer(std::string const& mime_typ)
{
    offer_mime_type(d_ptr->pub_src->d_ptr.get(), mime_typ.c_str());
}

primary_selection_source* primary_selection_source_ext::src() const
{
    return d_ptr->pub_src.get();
}

}
