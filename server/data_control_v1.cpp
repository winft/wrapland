/*
    SPDX-FileCopyrightText: 2021 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#include "data_control_v1_p.h"

#include "client.h"
#include "data_device.h"
#include "data_source_p.h"
#include "display.h"
#include "primary_selection_p.h"
#include "selection_device_manager_p.h"
#include "selection_p.h"
#include "selection_pool.h"

#include "wayland/bind.h"
#include "wayland/global.h"

#include <wayland-server.h>

namespace Wrapland::Server
{

struct zwlr_data_control_manager_v1_interface const data_control_manager_v1::Private::s_interface
    = {
        cb<create_source>,
        cb<get_device>,
        resourceDestroyCallback,
};

data_control_manager_v1::Private::Private(data_control_manager_v1* q, Display* display)
    : device_manager<data_control_manager_v1_global>(q,
                                                     display,
                                                     &zwlr_data_control_manager_v1_interface,
                                                     &s_interface)
{
}

data_control_manager_v1::data_control_manager_v1(Display* display, QObject* parent)
    : QObject(parent)
    , d_ptr(new Private(this, display))
{
    d_ptr->create();
}

data_control_manager_v1::~data_control_manager_v1() = default;

void data_control_manager_v1::create_source(Client* client, uint32_t version, uint32_t id)
{
    [[maybe_unused]] auto src_res = new data_control_source_v1_res(client, version, id);
    // TODO(romangg): Catch oom.

    Q_EMIT source_created();
}

void data_control_manager_v1::get_device(Client* client, uint32_t version, uint32_t id, Seat* seat)
{
    auto device = new data_control_device_v1(client, version, id, seat);
    // TODO(romangg): Catch oom exception.

    if (auto src = seat->d_ptr->data_devices.focus.source) {
        device->send_selection(src);
    }

    QObject::connect(seat, &Seat::selectionChanged, device, [seat, device] {
        if (auto source = seat->d_ptr->data_devices.focus.source;
            !source || source != device->selection()) {
            device->send_selection(source);
        }
    });

    if (version >= ZWLR_DATA_CONTROL_DEVICE_V1_PRIMARY_SELECTION_SINCE_VERSION) {
        if (auto src = seat->d_ptr->primary_selection_devices.focus.source) {
            device->send_primary_selection(src);
        }
        QObject::connect(seat, &Seat::primarySelectionChanged, device, [seat, device] {
            if (auto source = seat->d_ptr->primary_selection_devices.focus.source;
                !source || source != device->primary_selection()) {
                device->send_primary_selection(source);
            }
        });
    }

    Q_EMIT device_created(device);
}

template<typename Source>
data_control_offer_v1_res* data_control_device_v1::impl::send_data_offer_impl(Source source)
{
    assert(source);

    auto offer = new data_control_offer_v1_res(client()->handle(), version(), source);

    if (!offer->impl->resource()) {
        delete offer;
        return nullptr;
    }

    send<zwlr_data_control_device_v1_send_data_offer>(offer->impl->resource());
    offer->send_offers();
    return offer;
}

data_control_offer_v1_res* data_control_device_v1::impl::send_data_offer(data_source* source)
{
    return send_data_offer_impl(source);
}

data_control_offer_v1_res*
data_control_device_v1::impl::send_data_offer(primary_selection_source* source)
{
    return send_data_offer_impl(source);
}

struct zwlr_data_control_device_v1_interface const data_control_device_v1::impl::s_interface = {
    set_selection_callback,
    destroyCallback,
    set_primary_selection_callback,
};

// Similar to set_selection in selection_p.h
void set_control_selection(data_control_device_v1* handle,
                           selection_source_holder& holder,
                           data_control_source_v1_res* source_res)
{
    auto source = source_res ? source_res->src() : nullptr;

    if (holder.source == source) {
        return;
    }

    QObject::disconnect(holder.destroyed_notifier);

    if (holder.source) {
        holder.source->cancel();
    }

    holder.source = source;

    if (source) {
        holder.destroyed_notifier = QObject::connect(
            source_res, &data_control_source_v1_res::resourceDestroyed, handle, [handle, &holder] {
                set_control_selection(handle, holder, nullptr);
            });
    } else {
        holder.destroyed_notifier = QMetaObject::Connection();
    }
    Q_EMIT handle->selection_changed();
}

data_control_device_v1::impl::impl(Client* client,
                                   uint32_t version,
                                   uint32_t id,
                                   Seat* seat,
                                   data_control_device_v1* q_ptr)
    : Wayland::Resource<data_control_device_v1>(client,
                                                version,
                                                id,
                                                &zwlr_data_control_device_v1_interface,
                                                &s_interface,
                                                q_ptr)
    , m_seat{seat}
{
}

template<typename Source, typename Devices>
void data_control_device_v1::impl::set_selection_impl(Devices& seat_devices,
                                                      selection_source_holder& selection_holder,
                                                      data_control_device_v1* handle,
                                                      wl_resource* wlSource)
{
    if (!wlSource) {
        set_control_selection(handle, selection_holder, nullptr);
        seat_devices.set_selection(nullptr);
        return;
    }

    auto source = Resource<data_control_source_v1_res>::handle(wlSource);
    if (!std::holds_alternative<std::monostate>(source->data_src)) {
        handle->d_ptr->postError(ZWLR_DATA_CONTROL_DEVICE_V1_ERROR_USED_SOURCE,
                                 "Source already used");
        return;
    }

    auto src = std::unique_ptr<Source>(new Source);
    src->d_ptr->mimeTypes = source->src()->early_mime_types;

    src->d_ptr->res = source;
    QObject::connect(source,
                     &data_control_source_v1_res::resourceDestroyed,
                     src.get(),
                     &Source::resourceDestroyed);

    set_control_selection(handle, selection_holder, source);

    // A connection from the set_selection call accesses data_src field. So move it over before.
    auto src_ptr = src.get();
    source->data_src = std::move(src);
    seat_devices.set_selection(src_ptr);
}

void data_control_device_v1::impl::set_selection_callback(wl_client* /*wlClient*/,
                                                          wl_resource* wlResource,
                                                          wl_resource* wlSource)
{
    auto handle = Resource::handle(wlResource);
    auto& seat_devices = handle->d_ptr->m_seat->d_ptr->data_devices;
    auto& selection_holder = handle->d_ptr->selection;

    set_selection_impl<data_source>(seat_devices, selection_holder, handle, wlSource);
}

void data_control_device_v1::impl::set_primary_selection_callback(wl_client* /*wlClient*/,
                                                                  wl_resource* wlResource,
                                                                  wl_resource* wlSource)
{
    auto handle = Resource::handle(wlResource);
    auto& seat_devices = handle->d_ptr->m_seat->d_ptr->primary_selection_devices;
    auto& selection_holder = handle->d_ptr->primary_selection;

    set_selection_impl<primary_selection_source>(seat_devices, selection_holder, handle, wlSource);
}

data_control_device_v1::data_control_device_v1(Client* client,
                                               uint32_t version,
                                               uint32_t id,
                                               Seat* seat)
    : d_ptr(new data_control_device_v1::impl(client, version, id, seat, this))
{
}

template<typename Source>
Source* data_control_device_v1::impl::get_selection(selection_source_holder const& holder)
{
    if (holder.source) {
        if (auto src = std::get_if<std::unique_ptr<Source>>(&holder.source->data_src)) {
            return src->get();
        }
    }

    return nullptr;
}

data_source* data_control_device_v1::selection() const
{
    return d_ptr->get_selection<data_source>(d_ptr->selection);
}

primary_selection_source* data_control_device_v1::primary_selection() const
{
    return d_ptr->get_selection<primary_selection_source>(d_ptr->primary_selection);
}

void data_control_device_v1::send_selection(data_source* source) const
{
    if (!source) {
        d_ptr->send<zwlr_data_control_device_v1_send_selection>(nullptr);
        return;
    }

    if (auto offer = d_ptr->send_data_offer(source)) {
        d_ptr->send<zwlr_data_control_device_v1_send_selection>(offer->impl->resource());
    }
}

void data_control_device_v1::send_primary_selection(primary_selection_source* source) const
{
    assert(d_ptr->version() >= ZWLR_DATA_CONTROL_DEVICE_V1_PRIMARY_SELECTION_SINCE_VERSION);

    if (!source) {
        d_ptr->send<zwlr_data_control_device_v1_send_primary_selection>(nullptr);
        return;
    }

    if (auto offer = d_ptr->send_data_offer(source)) {
        d_ptr->send<zwlr_data_control_device_v1_send_primary_selection>(offer->impl->resource());
    }
}

data_control_source_v1_res::res_impl::res_impl(Client* client,
                                               uint32_t version,
                                               uint32_t id,
                                               data_control_source_v1_res* q_ptr)
    : Wayland::Resource<data_control_source_v1_res>(client,
                                                    version,
                                                    id,
                                                    &zwlr_data_control_source_v1_interface,
                                                    &s_interface,
                                                    q_ptr)
    , q_ptr{q_ptr}
{
}

struct zwlr_data_control_source_v1_interface const data_control_source_v1_res::res_impl::s_interface
    = {
        offer_callback,
        destroyCallback,
};

template<class... Ts>
// NOLINTNEXTLINE(fuchsia-multiple-inheritance)
struct overload : Ts... {
    using Ts::operator()...;
};
template<class... Ts>
overload(Ts...) -> overload<Ts...>;

void data_control_source_v1_res::res_impl::offer_callback(wl_client* /*wlClient*/,
                                                          wl_resource* wlResource,
                                                          char const* mimeType)
{
    auto handle = Resource::handle(wlResource);

    std::visit(overload{[&](std::unique_ptr<data_source>& src) {
                            offer_mime_type(src->d_ptr.get(), mimeType);
                        },
                        [&](std::unique_ptr<primary_selection_source>& src) {
                            offer_mime_type(src->d_ptr.get(), mimeType);
                        },
                        [&](std::monostate /*unused*/) {
                            handle->src()->early_mime_types.emplace_back(mimeType);
                        }},
               handle->data_src);
}

data_control_source_v1_res::data_control_source_v1_res(Client* client,
                                                       uint32_t version,
                                                       uint32_t id)
    : impl{new data_control_source_v1_res::res_impl(client, version, id, this)}
{
}

data_control_source_v1_res::~data_control_source_v1_res() = default;

void data_control_source_v1_res::request_data(std::string const& mime_type, int32_t fd) const
{
    // TODO(unknown author): does this require a sanity check on the possible mime type?
    impl->send<zwlr_data_control_source_v1_send_send>(mime_type.c_str(), fd);
    close(fd);
}

void data_control_source_v1_res::cancel() const
{
    impl->send<zwlr_data_control_source_v1_send_cancelled>();
}

data_control_source_v1_res* data_control_source_v1_res::src()
{
    return this;
}

struct zwlr_data_control_offer_v1_interface const data_control_offer_v1_res_impl::s_interface = {
    receive_callback,
    destroyCallback,
};

data_control_offer_v1_res_impl::data_control_offer_v1_res_impl(Client* client,
                                                               uint32_t version,
                                                               data_control_offer_v1_res* q_ptr)
    : Wayland::Resource<data_control_offer_v1_res>(client,
                                                   version,
                                                   0,
                                                   &zwlr_data_control_offer_v1_interface,
                                                   &s_interface,
                                                   q_ptr)
{
}

void data_control_offer_v1_res_impl::receive_callback(wl_client* /*wlClient*/,
                                                      wl_resource* wlResource,
                                                      char const* mime_type,
                                                      int32_t fd)
{
    auto handle = Resource::handle(wlResource);

    std::visit(overload{[&](data_source* src) {
                            assert(src);
                            receive_mime_type_offer(src, mime_type, fd);
                        },
                        [&](primary_selection_source* src) {
                            assert(src);
                            receive_mime_type_offer(src, mime_type, fd);
                        },
                        [&](std::monostate /*unused*/) { close(fd); }},
               handle->impl->src);
}

data_control_offer_v1_res::data_control_offer_v1_res(Client* client,
                                                     uint32_t version,
                                                     data_source* source)
    : impl(new data_control_offer_v1_res_impl(client, version, this))
{
    assert(source);
    impl->src = source;

    QObject::connect(source, &data_source::mime_type_offered, this, [this](auto const& mime_type) {
        impl->send<zwlr_data_control_offer_v1_send_offer>(mime_type.c_str());
    });
    QObject::connect(source, &data_source::resourceDestroyed, this, [this] { impl->src = {}; });
}

data_control_offer_v1_res::data_control_offer_v1_res(Client* client,
                                                     uint32_t version,
                                                     primary_selection_source* source)
    : impl(new data_control_offer_v1_res_impl(client, version, this))
{
    assert(source);
    impl->src = source;

    QObject::connect(
        source, &primary_selection_source::mime_type_offered, this, [this](auto const& mime_type) {
            impl->send<zwlr_data_control_offer_v1_send_offer>(mime_type.c_str());
        });
    QObject::connect(
        source, &primary_selection_source::resourceDestroyed, this, [this] { impl->src = {}; });
}

void data_control_offer_v1_res::send_offers() const
{
    auto send_mime_types = [this](auto const& mime_types) {
        for (auto const& mt : mime_types) {
            impl->send<zwlr_data_control_offer_v1_send_offer>(mt.c_str());
        }
    };

    std::visit(overload{[&](data_source* src) {
                            assert(src);
                            send_mime_types(src->mime_types());
                        },
                        [&](primary_selection_source* src) {
                            assert(src);
                            send_mime_types(src->mime_types());
                        },
                        [](std::monostate /*unused*/) { assert(false); }},
               impl->src);
}

}
