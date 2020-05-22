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
#include "output_management_v1.h"

#include "display.h"
#include "output_configuration_v1_p.h"

#include "wayland/global.h"

#include "wayland-output-management-v1-server-protocol.h"

#include <QHash>

namespace Wrapland::Server
{

constexpr uint32_t OutputManagementV1Version = 1;
using OutputManagementV1Global = Wayland::Global<OutputManagementV1, OutputManagementV1Version>;
using OutputManagementV1Bind = Wayland::Resource<OutputManagementV1, OutputManagementV1Global>;

class OutputManagementV1::Private : public OutputManagementV1Global
{
public:
    Private(OutputManagementV1* q, Display* display);
    ~Private() override;

private:
    static void
    createConfigurationCallback(wl_client* wlClient, wl_resource* wlResource, uint32_t id);

    static const struct zkwinft_output_management_v1_interface s_interface;

    std::vector<OutputConfigurationV1*> m_configurations;
};

struct zkwinft_output_management_v1_interface const OutputManagementV1::Private::s_interface
    = {createConfigurationCallback};

OutputManagementV1::Private::Private(OutputManagementV1* q, Display* display)
    : OutputManagementV1Global(q, display, &zkwinft_output_management_v1_interface, &s_interface)
{
    create();
}

OutputManagementV1::Private::~Private()
{
    std::for_each(m_configurations.cbegin(),
                  m_configurations.cend(),
                  [](OutputConfigurationV1* config) { config->d_ptr->manager = nullptr; });
}

void OutputManagementV1::Private::createConfigurationCallback([[maybe_unused]] wl_client* wlClient,
                                                              wl_resource* wlResource,
                                                              uint32_t id)
{
    auto priv = handle(wlResource)->d_ptr.get();
    auto bind = priv->getBind(wlResource);

    auto config
        = new OutputConfigurationV1(bind->client()->handle(), bind->version(), id, priv->handle());
    priv->m_configurations.push_back(config);

    connect(config, &OutputConfigurationV1::resourceDestroyed, priv->handle(), [priv, config] {
        auto& configs = priv->m_configurations;
        configs.erase(std::remove(configs.begin(), configs.end(), config), configs.end());
    });
}

OutputManagementV1::OutputManagementV1(Display* display, QObject* parent)
    : QObject(parent)
    , d_ptr(new Private(this, display))
{
}

OutputManagementV1::~OutputManagementV1() = default;

}
