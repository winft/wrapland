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

#include "output_device_v1.h"

#include "wayland/global.h"

#include <QRectF>
#include <QSize>
#include <QString>
#include <memory>

namespace Wrapland::Server
{

constexpr uint32_t OutputDeviceV1Version = 1;
using OutputDeviceV1Global = Wayland::Global<OutputDeviceV1, OutputDeviceV1Version>;
using OutputDeviceV1Bind = Wayland::Resource<OutputDeviceV1, OutputDeviceV1Global>;

class OutputDeviceV1::Private : public OutputDeviceV1Global
{
public:
    struct ResourceData {
        wl_resource* resource;
        uint32_t version;
    };
    Private(OutputDeviceV1* q, Display* display);

    void bindInit(OutputDeviceV1Bind* bind) override;

    void addMode(Mode& mode);
    void setMode(int id);

    void sendInfo(OutputDeviceV1Bind* bind);
    void sendEnabled(OutputDeviceV1Bind* bind);

    void sendMode(OutputDeviceV1Bind* bind, const Mode& mode);
    void sendTransform(OutputDeviceV1Bind* bind);
    void sendGeometry(OutputDeviceV1Bind* bind);

    void sendDone(OutputDeviceV1Bind* bind);

    void broadcast();

    struct State {
        struct Info {
            bool operator==(struct Info& i) const
            {
                return uuid == i.uuid && eisaId == i.eisaId && serialNumber == i.serialNumber
                    && edid == i.edid && manufacturer == i.manufacturer && model == i.model
                    && physicalSize == i.physicalSize;
            }
            QByteArray uuid;
            QString eisaId;
            QString serialNumber;
            QByteArray edid;
            QString manufacturer{QStringLiteral("kwinft")}; // NOLINT(performance-no-automatic-move)
            QString model{QStringLiteral("none")};          // NOLINT(performance-no-automatic-move)
            QSize physicalSize;
        } info;

        Enablement enabled = Enablement::Enabled;

        Mode mode;
        Transform transform = OutputDeviceV1::Transform::Normal;
        QRectF geometry;
    } pending, published;

    QList<Mode> modes;

    Display* displayHandle;

private:
    bool broadcastInfo();
    bool broadcastEnabled();
    bool broadcastMode();
    bool broadcastTransform();
    bool broadcastGeometry();
};

}
