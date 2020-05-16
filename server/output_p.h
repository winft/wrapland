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

#include "wayland/global.h"

namespace Wrapland::Server
{
class D_isplay;

constexpr uint32_t OutputVersion = 3;
using OutputGlobal = Wayland::Global<Output, OutputVersion>;

class Output::Private : public OutputGlobal
{
public:
    Private(Output* q, D_isplay* display);

    void bindInit(Wayland::Resource<Output, OutputGlobal>* bind) override;

    void sendMode(Wayland::Resource<Output, OutputGlobal>* bind, const Mode& mode);
    void sendMode(const Mode& mode);
    void sendGeometry();
    void sendScale();
    void sendDone();

    void updateGeometry();
    void updateScale();

    D_isplay* displayHandle;

    QSize physicalSize;
    QPoint globalPosition;
    std::string manufacturer = "org.kde.kwin";
    std::string model = "none";

    int scale = 1;
    SubPixel subPixel = SubPixel::Unknown;
    Transform transform = Transform::Normal;

    std::vector<Mode> modes;

    struct {
        DpmsMode mode = DpmsMode::On;
        bool supported = false;
    } dpms;

    Output* q_ptr;

private:
    int32_t toTransform() const;
    int32_t toSubPixel() const;

    std::tuple<int32_t, int32_t, int32_t, int32_t, int32_t, const char*, const char*, int32_t>
    geometryArgs() const;

    int32_t getFlags(const Mode& mode);

    static const struct wl_output_interface s_interface;
};

}
