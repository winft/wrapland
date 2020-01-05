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

#include "resource.h"

#include "output_changeset_v1.h"
#include "output_device_v1_interface.h"
#include "output_management_v1_interface.h"

#include <Wrapland/Server/wraplandserver_export.h>

namespace Wrapland
{
namespace Server
{
/** @class OutputConfigurationV1Interface
 *
 * Holds a new configuration for the outputs.
 *
 * The overall mechanism is to get a new OutputConfigurationV1 from the OutputManagementV1 global and
 * apply changes through the OutputConfigurationV1::set* calls. When all changes are set, the client
 * calls apply, which asks the server to look at the changes and apply them. The server will then
 * signal back whether the changes have been applied successfully (@c setApplied()) or were rejected
 * or failed to apply (@c setFailed()).
 *
 * Once the client has called applied, the OutputManagementV1Interface send the configuration object
 * to the compositor through the OutputManagementV1::configurationChangeRequested(OutputConfigurationV1*)
 * signal, the compositor can then decide what to do with the changes.
 *
 * These Wrapland classes will not apply changes to the OutputDeviceV1s, this is the compositor's
 * task. As such, the configuration set through this interface can be seen as a hint what the
 * compositor should set up, but whether or not the compositor does it (based on hardware or
 * rendering policies, for example), is up to the compositor. The mode setting is passed on to
 * the DRM subsystem through the compositor. The compositor also saves this configuration and reads
 * it on startup, this interface is not involved in that process.
 *
 * @see OutputManagementV1Interface
 * @see OutputConfigurationV1
 */
class WRAPLANDSERVER_EXPORT OutputConfigurationV1Interface : public Resource
{
    Q_OBJECT
public:
    ~OutputConfigurationV1Interface() override;

    /**
     * Accessor for the changes made to OutputDeviceV1s. The data returned from this call
     * will be deleted by the OutputConfigurationV1Interface when
     * OutputManagementV1Interface::setApplied() or OutputManagementV1Interface::setFailed()
     * is called, and on destruction of the OutputConfigurationV1Interface, so make sure you
     * do not keep these pointers around.
     * @returns A QHash of ChangeSets per outputdevice.
     * @see ChangeSet
     * @see OutputDeviceV1Interface
     * @see OutputManagementV1
     */
    QHash<OutputDeviceV1Interface*, OutputChangesetV1*> changes() const;

public Q_SLOTS:
    /**
     * Called by the compositor once the changes have successfully been applied.
     * The compositor is responsible for updating the OutputDeviceV1s. After having
     * done so, calling this function sends applied() through the client.
     * @see setFailed
     * @see OutputConfigurationV1::applied
     */
    void setApplied();
    /**
     * Called by the compositor when the changes as a whole are rejected or
     * failed to apply. This function results in the client OutputConfigurationV1 emitting
     * failed().
     * @see setApplied
     * @see OutputConfigurationV1::failed
     */
    void setFailed();

private:
    explicit OutputConfigurationV1Interface(OutputManagementV1Interface *parent,
                                            wl_resource *parentResource);
    friend class OutputManagementV1Interface;

    class Private;
    Private *d_func() const;
};


}
}

Q_DECLARE_METATYPE(Wrapland::Server::OutputConfigurationV1Interface*)
