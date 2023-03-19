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

#include "output_p.h"
#include "wl_output.h"

#include "wayland/global.h"

namespace Wrapland::Server
{
class Display;

constexpr uint32_t WlOutputVersion = 3;
using WlOutputGlobal = Wayland::Global<WlOutput, WlOutputVersion>;
using WlOutputBind = Wayland::Bind<WlOutputGlobal>;

class WlOutput::Private : public WlOutputGlobal
{
public:
    Private(Server::output* output, Display* display, WlOutput* q_ptr);

    bool broadcast();
    void done();
    void done(WlOutputBind* bind);

    Server::output* output;

private:
    void bindInit(WlOutputBind* bind) override;

    static std::
        tuple<int32_t, int32_t, int32_t, int32_t, int32_t, char const*, char const*, int32_t>
        geometry_args(output_state const& state);

    void sendMode(WlOutputBind* bind, output_mode const& mode);
    void sendMode(output_mode const& mode);

    static const struct wl_output_interface s_interface;
};

}
