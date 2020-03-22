/****************************************************************************
 * Copyright 2015  Sebastian Kügler <sebas@kde.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) version 3, or any
 * later version accepted by the membership of KDE e.V. (or its
 * successor approved by the membership of KDE e.V.), which shall
 * act as a proxy defined in Section 6 of version 3 of the license.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library.  If not, see <http://www.gnu.org/licenses/>.
 ****************************************************************************/
#include "output_management_v1.h"

#include "event_queue.h"
#include "output_configuration_v1.h"
#include "wayland_pointer_p.h"

#include "wayland-output-management-v1-client-protocol.h"

namespace Wrapland
{
namespace Client
{

class Q_DECL_HIDDEN OutputManagementV1::Private
{
public:
    Private() = default;

    WaylandPointer<zkwinft_output_management_v1,
                    zkwinft_output_management_v1_destroy> outputManagement;
    EventQueue *queue = nullptr;
};

OutputManagementV1::OutputManagementV1(QObject *parent)
: QObject(parent)
, d(new Private)
{
}

OutputManagementV1::~OutputManagementV1()
{
    d->outputManagement.release();
}

void OutputManagementV1::setup(zkwinft_output_management_v1 *outputManagement)
{
    Q_ASSERT(outputManagement);
    Q_ASSERT(!d->outputManagement);
    d->outputManagement.setup(outputManagement);
}

void OutputManagementV1::release()
{
    d->outputManagement.release();
}

void OutputManagementV1::setEventQueue(EventQueue *queue)
{
    d->queue = queue;
}

EventQueue* OutputManagementV1::eventQueue()
{
    return d->queue;
}

OutputManagementV1::operator zkwinft_output_management_v1*() {
    return d->outputManagement;
}

OutputManagementV1::operator zkwinft_output_management_v1*() const {
    return d->outputManagement;
}

bool OutputManagementV1::isValid() const
{
    return d->outputManagement.isValid();
}

OutputConfigurationV1* OutputManagementV1::createConfiguration(QObject *parent)
{
    Q_UNUSED(parent);
    auto *config = new OutputConfigurationV1(this);
    auto w = zkwinft_output_management_v1_create_configuration(d->outputManagement);

    if (d->queue) {
        d->queue->addProxy(w);
    }

    config->setup(w);
    return config;
}


}
}
