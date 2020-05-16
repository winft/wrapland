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
#include "output.h"
#include "output_p.h"

#include "wayland/client.h"
#include "wayland/display.h"

namespace Wrapland::Server
{

constexpr uint32_t DpmsManagerVersion = 1;
using DpmsManagerGlobal = Wayland::Global<DpmsManager, DpmsManagerVersion>;

const struct org_kde_kwin_dpms_manager_interface DpmsManager::Private::s_interface = {
    getDpmsCallback,
};

DpmsManager::Private::Private(D_isplay* display, DpmsManager* q)
    : DpmsManagerGlobal(q, display, &org_kde_kwin_dpms_manager_interface, &s_interface)
{
    create();
}

void DpmsManager::Private::getDpmsCallback([[maybe_unused]] wl_client* wlClient,
                                           wl_resource* wlResource,
                                           uint32_t id,
                                           wl_resource* output)
{
    auto priv = handle(wlResource)->d_ptr.get();
    auto bind = priv->getBind(wlResource);

    auto dpms
        = new Dpms(bind->client()->handle(), bind->version(), id, OutputGlobal::handle(output));
    if (!dpms) {
        return;
    }

    dpms->sendSupported();
    dpms->sendMode();
    dpms->sendDone();
}

DpmsManager::DpmsManager(D_isplay* display, QObject* parent)
    : QObject(parent)
    , d_ptr(new Private(display, this))
{
}

DpmsManager::~DpmsManager() = default;

const struct org_kde_kwin_dpms_interface Dpms::Private::s_interface = {
    setCallback,
    destroyCallback,
};

Dpms::Private::Private(Client* client, uint32_t version, uint32_t id, Output* output, Dpms* q)
    : Wayland::Resource<Dpms>(client, version, id, &org_kde_kwin_dpms_interface, &s_interface, q)
    , output(output)
{
}

void Dpms::Private::setCallback(wl_client* client, wl_resource* wlResource, uint32_t mode)
{
    Q_UNUSED(client)
    Output::DpmsMode dpmsMode;
    switch (mode) {
    case ORG_KDE_KWIN_DPMS_MODE_ON:
        dpmsMode = Output::DpmsMode::On;
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
    Q_EMIT handle(wlResource)->d_ptr->output->dpmsModeRequested(dpmsMode);
}

Dpms::Dpms(Client* client, uint32_t version, uint32_t id, Output* output)
    : d_ptr(new Private(client, version, id, output, this))
{
    connect(output, &Output::dpmsSupportedChanged, this, [this] {
        sendSupported();
        sendDone();
    });
    connect(output, &Output::dpmsModeChanged, this, [this] {
        sendMode();
        sendDone();
    });
}

void Dpms::sendSupported()
{
    d_ptr->send<org_kde_kwin_dpms_send_supported>(d_ptr->output->isDpmsSupported());
}

void Dpms::sendMode()
{
    org_kde_kwin_dpms_mode mode;

    switch (d_ptr->output->dpmsMode()) {
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
