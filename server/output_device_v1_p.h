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
#pragma once

#include "output.h"
#include "output_device_v1.h"

#include "wayland/global.h"
#include "wayland/resource.h"

namespace Wrapland::Server
{
class Display;
class Output;

constexpr uint32_t OutputDeviceV1Version = 1;
using OutputDeviceV1Global = Wayland::Global<OutputDeviceV1, OutputDeviceV1Version>;
using OutputDeviceV1Bind = Wayland::Bind<OutputDeviceV1Global>;

class OutputDeviceV1::Private : public OutputDeviceV1Global
{
public:
    Private(Output* output, Display* display, OutputDeviceV1* q_ptr);

    bool broadcast();
    void done();

    Display* displayHandle;
    Output* output;

private:
    void bindInit(OutputDeviceV1Bind* bind) override;

    void sendMode(OutputDeviceV1Bind* bind, Output::Mode const& mode);
    void sendMode(Output::Mode const& mode);
};

}
