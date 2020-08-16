/****************************************************************************
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
#include "presentation_time.h"

#include "display.h"
#include "region.h"
#include "surface.h"
#include "surface_p.h"
#include "wl_output_p.h"

#include "wayland/global.h"
#include "wayland/resource.h"

#include <wayland-presentation-time-server-protocol.h>

namespace Wrapland::Server
{

constexpr uint32_t PresentationManagerVersion = 1;
using PresentationManagerGlobal = Wayland::Global<PresentationManager, PresentationManagerVersion>;
using PresentationManagerBind = Wayland::Bind<PresentationManagerGlobal>;

class PresentationManager::Private : public PresentationManagerGlobal
{
public:
    Private(PresentationManager* q, Display* display);

    void bindInit(PresentationManagerBind* bind) override;

    clockid_t clockId = 0;

private:
    static void feedbackCallback(wl_client* wlClient,
                                 wl_resource* wlResource,
                                 wl_resource* wlSurface,
                                 uint32_t id);

    static const struct wp_presentation_interface s_interface;
    static const uint32_t s_version;
};

const uint32_t PresentationManager::Private::s_version = 1;

PresentationManager::Private::Private(PresentationManager* q, Display* display)
    : PresentationManagerGlobal(q, display, &wp_presentation_interface, &s_interface)
{
}

const struct wp_presentation_interface PresentationManager::Private::s_interface = {
    resourceDestroyCallback,
    feedbackCallback,
};

void PresentationManager::Private::bindInit(PresentationManagerBind* bind)
{
    send<wp_presentation_send_clock_id>(bind, clockId);
}

void PresentationManager::Private::feedbackCallback([[maybe_unused]] wl_client* wlClient,
                                                    wl_resource* wlResource,
                                                    wl_resource* wlSurface,
                                                    uint32_t id)
{
    auto bind = handle(wlResource)->d_ptr->getBind(wlResource);
    auto surface = Wayland::Resource<Surface>::handle(wlSurface);

    auto feedback = new PresentationFeedback(bind->client()->handle(), bind->version(), id);
    // TODO(romangg): error handling (when resource not created)

    surface->d_ptr->addPresentationFeedback(feedback);
}

PresentationManager::PresentationManager(Display* display, QObject* parent)
    : QObject(parent)
    , d_ptr(new Private(this, display))
{
    d_ptr->create();
}

PresentationManager::~PresentationManager() = default;

clockid_t PresentationManager::clockId() const
{
    return d_ptr->clockId;
}

void PresentationManager::setClockId(clockid_t clockId)
{
    d_ptr->clockId = clockId;
    d_ptr->send<wp_presentation_send_clock_id>(clockId);
}

class PresentationFeedback::Private : public Wayland::Resource<PresentationFeedback>
{
public:
    Private(Client* client, uint32_t version, uint32_t id, PresentationFeedback* q);
};

PresentationFeedback::Private::Private(Client* client,
                                       uint32_t version,
                                       uint32_t id,
                                       PresentationFeedback* q)
    : Wayland::Resource<PresentationFeedback>(client,
                                              version,
                                              id,
                                              &wp_presentation_feedback_interface,
                                              nullptr,
                                              q)
{
}

PresentationFeedback::PresentationFeedback(Client* client, uint32_t version, uint32_t id)
    : QObject(nullptr)
    , d_ptr(new Private(client, version, id, this))
{
    connect(this, &PresentationFeedback::resourceDestroyed, this, [this] { d_ptr = nullptr; });
}

PresentationFeedback::~PresentationFeedback()
{
    if (d_ptr) {
        d_ptr->serverSideDestroy();
        delete d_ptr;
    }
}

void PresentationFeedback::sync(Output* output)
{
    auto outputBinds = output->wayland_output()->d_ptr->getBinds(d_ptr->client()->handle());

    for (auto bind : outputBinds) {
        d_ptr->send<wp_presentation_feedback_send_sync_output>(bind->resource());
    }
}

uint32_t toFlags(PresentationFeedback::Kinds kinds)
{
    using Kind = PresentationFeedback::Kind;
    uint32_t ret = 0;
    if (kinds.testFlag(Kind::Vsync)) {
        ret |= WP_PRESENTATION_FEEDBACK_KIND_VSYNC;
    }
    if (kinds.testFlag(Kind::HwClock)) {
        ret |= WP_PRESENTATION_FEEDBACK_KIND_HW_CLOCK;
    }
    if (kinds.testFlag(Kind::HwCompletion)) {
        ret |= WP_PRESENTATION_FEEDBACK_KIND_HW_COMPLETION;
    }
    if (kinds.testFlag(Kind::ZeroCopy)) {
        ret |= WP_PRESENTATION_FEEDBACK_KIND_ZERO_COPY;
    }
    return ret;
}

void PresentationFeedback::presented(uint32_t tvSecHi,
                                     uint32_t tvSecLo,
                                     uint32_t tvNsec,
                                     uint32_t refresh,
                                     uint32_t seqHi,
                                     uint32_t seqLo,
                                     Kinds kinds)
{
    d_ptr->send<wp_presentation_feedback_send_presented>(
        tvSecHi, tvSecLo, tvNsec, refresh, seqHi, seqLo, toFlags(kinds));
}

void PresentationFeedback::discarded()
{
    d_ptr->send<wp_presentation_feedback_send_discarded>();
}

}
