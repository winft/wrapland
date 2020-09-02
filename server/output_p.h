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

#include <QRectF>

namespace Wrapland::Server
{
class OutputDeviceV1;
class WlOutput;
class XdgOutput;

class Display;

struct OutputState {
    struct Info {
        std::string name = "Unknown";
        std::string description;
        std::string make;
        std::string model;
        std::string serial_number;
        QSize physical_size;
    } info;

    bool enabled{false};

    Output::Mode mode;
    Output::Subpixel subpixel = Output::Subpixel::Unknown;

    Output::Transform transform = Output::Transform::Normal;
    QRectF geometry;
    int client_scale = 1;
};

class Output::Private
{
public:
    Private(Display* display, Output* q);

    void update_client_scale();
    void done();

    static int32_t get_mode_flags(Output::Mode const& mode, OutputState const& state);
    static int32_t to_transform(Output::Transform transform);

    Display* display_handle;

    std::vector<Mode> modes;

    struct {
        DpmsMode mode = DpmsMode::Off;
        bool supported = false;
    } dpms;

    OutputState pending;
    OutputState published;

    std::unique_ptr<OutputDeviceV1> device;
    std::unique_ptr<WlOutput> wayland_output;
    std::unique_ptr<XdgOutput> xdg_output;

    Output* q_ptr;

private:
    int32_t toTransform() const;
    int32_t toSubpixel() const;
};

}
