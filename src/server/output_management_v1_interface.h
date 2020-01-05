/********************************************************************
Copyright © 2015 Sebastian Kügler <sebas@kde.org>
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

#include "global.h"

#include <Wrapland/Server/wraplandserver_export.h>

namespace Wrapland
{
namespace Server
{

class OutputConfigurationV1Interface;

/**
 * @class OutputManagementV1Interface
 *
 * This class is used to change the configuration of the Wayland server's outputs.
 * The client requests an OutputConfigurationV1, changes its OutputDevices and then
 * calls OutputConfigurationV1::apply, which makes this class emit a signal, carrying
 * the new configuration.
 * The server is then expected to make the requested changes by applying the settings
 * of the OutputDevices to the Outputs.
 *
 * @see OutputConfigurationV1
 * @see OutputConfigurationV1Interface
 */
class WRAPLANDSERVER_EXPORT OutputManagementV1Interface : public Global
{
    Q_OBJECT
public:
    virtual ~OutputManagementV1Interface();

Q_SIGNALS:
    /**
     * Emitted after the client has requested an OutputConfigurationV1 to be applied.
     * through OutputConfigurationV1::apply. The compositor can use this object to get
     * notified when the new configuration is set up, and it should be applied to the
     * Wayland server's OutputInterfaces.
     *
     * @param config The OutputConfigurationV1Interface corresponding to the client that
     * called apply().
     * @see OutputConfigurationV1::apply
     * @see OutputConfigurationV1Interface
     * @see OutputDeviceInterface
     * @see OutputInterface
     */
    void configurationChangeRequested(OutputConfigurationV1Interface *configurationInterface);

private:
    explicit OutputManagementV1Interface(Display *display, QObject *parent = nullptr);
    friend class Display;
    class Private;
};

}
}
