/********************************************************************
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
*********************************************************************/
#include "dpms_p.h"

#include "display.h"
#include "wl_output_p.h"

#include "wayland/client.h"
#include "wayland/display.h"

namespace Wrapland::Server
{

const struct org_kde_kwin_dpms_manager_interface DpmsManager::Private::s_interface = {
    cb<getDpmsCallback>,
};

DpmsManager::Private::Private(Display* display, DpmsManager* q)
    : DpmsManagerGlobal(q, display, &org_kde_kwin_dpms_manager_interface, &s_interface)
{
    create();
}

void DpmsManager::Private::getDpmsCallback(DpmsManagerBind* bind, uint32_t id, wl_resource* output)
{
    auto dpms
        = new Dpms(bind->client()->handle(), bind->version(), id, WlOutputGlobal::handle(output));
    if (!dpms) {
        return;
    }

    dpms->sendSupported();
    dpms->sendMode();
    dpms->sendDone();
}

DpmsManager::DpmsManager(Display* display)
    : d_ptr(new Private(display, this))
{
}

DpmsManager::~DpmsManager() = default;

const struct org_kde_kwin_dpms_interface Dpms::Private::s_interface = {
    setCallback,
    destroyCallback,
};

Dpms::Private::Private(Client* client, uint32_t version, uint32_t id, WlOutput* output, Dpms* q)
    : Wayland::Resource<Dpms>(client, version, id, &org_kde_kwin_dpms_interface, &s_interface, q)
    , output(output)
{
}

void Dpms::Private::setCallback(wl_client* /*client*/, wl_resource* wlResource, uint32_t mode)
{
    auto dpmsMode{Output::DpmsMode::On};

    switch (mode) {
    case ORG_KDE_KWIN_DPMS_MODE_ON:
        break;
    case ORG_KDE_KWIN_DPMS_MODE_STANDBY:
        dpmsMode = Output::DpmsMode::Standby;
        break;
    case ORG_KDE_KWIN_DPMS_MODE_SUSPEND:
        dpmsMode = Output::DpmsMode::Suspend;
        break;
    case ORG_KDE_KWIN_DPMS_MODE_OFF:
        dpmsMode = Output::DpmsMode::Off;
        break;
    default:
        return;
    }

    Q_EMIT handle(wlResource)->d_ptr->output->output()->dpms_mode_requested(dpmsMode);
}

Dpms::Dpms(Client* client, uint32_t version, uint32_t id, WlOutput* output)
    : d_ptr(new Private(client, version, id, output, this))
{
    auto master_output = output->output();
    connect(master_output, &Output::dpms_supported_changed, this, [this] {
        sendSupported();
        sendDone();
    });
    connect(master_output, &Output::dpms_mode_changed, this, [this] {
        sendMode();
        sendDone();
    });
}

void Dpms::sendSupported()
{
    d_ptr->send<org_kde_kwin_dpms_send_supported>(d_ptr->output->output()->dpms_supported());
}

void Dpms::sendMode()
{
    org_kde_kwin_dpms_mode mode = ORG_KDE_KWIN_DPMS_MODE_ON;

    switch (d_ptr->output->output()->dpms_mode()) {
    case Output::DpmsMode::On:
        mode = ORG_KDE_KWIN_DPMS_MODE_ON;
        break;
    case Output::DpmsMode::Standby:
        mode = ORG_KDE_KWIN_DPMS_MODE_STANDBY;
        break;
    case Output::DpmsMode::Suspend:
        mode = ORG_KDE_KWIN_DPMS_MODE_SUSPEND;
        break;
    case Output::DpmsMode::Off:
        mode = ORG_KDE_KWIN_DPMS_MODE_OFF;
        break;
    default:
        Q_UNREACHABLE();
    }

    d_ptr->send<org_kde_kwin_dpms_send_mode>(mode);
}

void Dpms::sendDone()
{
    d_ptr->send<org_kde_kwin_dpms_send_done>();
    d_ptr->flush();
}

}
