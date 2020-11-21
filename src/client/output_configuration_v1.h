/********************************************************************
Copyright © 2015  Sebastian Kügler <sebas@kde.org>
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

#include <Wrapland/Client/wraplandclient_export.h>

#include <QObject>
#include <QPoint>
#include <QVector>
// STD
#include <memory>

class QRectF;
struct zkwinft_output_management_v1;
struct zkwinft_output_configuration_v1;

namespace Wrapland
{
namespace Client
{

class EventQueue;

/** @class OutputConfigurationV1
 *
 * OutputConfigurationV1 provides access to changing OutputDevices. The interface is async
 * and atomic. An OutputConfigurationV1 is created through
 OutputManagementV1::createConfiguration().
 *
 * The overall mechanism is to get a new OutputConfigurationV1 from the OutputManagementV1 global
 and
 * apply changes through the OutputConfigurationV1::set* calls. When all changes are set, the client
 * calls apply, which asks the server to look at the changes and apply them. The server will then
 * signal back whether the changes have been applied successfully (@c applied()) or were rejected
 * or failed to apply (@c failed()).
 *
 * The current settings for outputdevices can be gotten from @c Registry::outputDevices(), these
 * are used in the set* calls to identify the output the setting applies to.
 *
 * These Wrapland classes will not apply changes to the OutputDevices, this is the compositor's
 * task. As such, the configuration set through this interface can be seen as a hint what the
 * compositor should set up, but whether or not the compositor does it (based on hardware or
 * rendering policies, for example), is up to the compositor. The mode setting is passed on to
 * the DRM subsystem through the compositor. The compositor also saves this configuration and reads
 * it on startup, this interface is not involved in that process.
 *
 * @c apply() should only be called after changes to all output devices have been made, not after
 * each change. This allows to test the new configuration as a whole, and is a lot faster since
 * hardware changes can be tested in their new combination, they done in parallel.and rolled back
 * as a whole.
 *
 * \verbatim
    // We're just picking the first of our outputdevices
    Wrapland::Client::OutputDevice *output = m_clientOutputs.first();

    // Create a new configuration object
    auto config = m_outputManagement.createConfiguration();

    // handle applied and failed signals
    connect(config, &OutputConfigurationV1::applied, []() {
        qDebug() << "Configuration applied!";
    });
    connect(config, &OutputConfigurationV1::failed, []() {
        qDebug() << "Configuration failed!";
    });

    // Change settings
    config->setMode(output, m_clientOutputs.first()->modes().last().id);
    config->setTransform(output, OutputDevice::Transform::Normal);
    config->setPosition(output, QPoint(0, 1920));
    config->setScale(output, 2);

    // Now ask the compositor to apply the changes
    config->apply();
    // You may wait for the applied() or failed() signal here
   \endverbatim

 * @see OutputDevice
 * @see OutputManagementV1
 * @see OutputManagementV1::createConfiguration()
 */
class WRAPLANDCLIENT_EXPORT OutputConfigurationV1 : public QObject
{
    Q_OBJECT
public:
    ~OutputConfigurationV1() override;

    /**
     * Setup this OutputConfigurationV1 to manage the @p outputconfiguration.
     * When using OutputManagementV1::createOutputConfiguration there is no need to call this
     * method.
     * @param outputconfiguration the outputconfiguration object to set up.
     **/
    void setup(zkwinft_output_configuration_v1* outputconfiguration);
    /**
     * @returns @c true if managing a zkwinft_output_configuration_v1.
     **/
    bool isValid() const;
    /**
     * Releases the zkwinft_output_configuration_v1 interface.
     * After the interface has been released the OutputConfigurationV1 instance is no
     * longer valid and can be setup with another zkwinft_output_configuration_v1 interface.
     **/
    void release();

    /**
     * Sets the @p queue to use for creating a OutputConfigurationV1.
     **/
    void setEventQueue(EventQueue* queue);
    /**
     * @returns The event queue to use for creating a OutputConfigurationV1
     **/
    EventQueue* eventQueue();

    /**
     * Enable or disable an output. Enabled means it's used by the
     * compositor for rendering, Disabled means, that no wl_output
     * is connected to this, and the device is sitting there unused
     * by this compositor.
     * The changes done in this call will be recorded in the
     * OutputDevice and only applied after apply() has been called.
     *
     * @param outputdevice the OutputDevice this change applies to.
     * @param enable new Enablement state of this output device.
     */
    void setEnabled(OutputDeviceV1* outputDevice, OutputDeviceV1::Enablement enable);

    /**
     * Set the mode of this output, identified by its mode id.
     * The changes done in this call will be recorded in the
     * OutputDevice and only applied after apply() has been called.
     *
     * @param outputdevice the OutputDevice this change applies to.
     * @param modeId the id of the mode.
     */
    void setMode(OutputDeviceV1* outputDevice, const int modeId);
    /**
     * Set transformation for this output, for example rotated or flipped.
     * The changes done in this call will be recorded in the
     * OutputDevice and only applied after apply() has been called.
     *
     * @param outputdevice the OutputDevice this change applies to.
     * @param scale the scaling factor for this output device.
     */
    void setTransform(OutputDeviceV1* outputDevice, OutputDeviceV1::Transform transform);

    /**
     * Sets the geometry of this output in the global space, relative to other outputs.
     * QPoint(0, 0) for top-left. The position is the top-left corner of this output.
     *
     * When width and height are both set to 0, no new content is posted to the output what
     * implicitly disables it.
     *
     * The x, width and height argument must be non-negative and width must be 0 if and only if
     * height is 0.
     *
     * The changes done in this call will be recorded in the
     * OutputDevice and only applied after apply() has been called.
     *
     * @param outputdevice the OutputDevice this change applies to.
     * @param geo the OutputDevice geometry relative to other outputs,
     *
     */
    void setGeometry(OutputDeviceV1* outputDevice, const QRectF& geo);

    /**
     * Ask the compositor to apply the changes.
     * This results in the compositor looking at all outputdevices and if they have
     * pending changes from the set* calls, these changes will be tested with the
     * hardware and applied if possible. The compositor will react to these changes
     * with the applied() or failed() signals. Note that mode setting may take a
     * while, so the interval between calling apply() and receiving the applied()
     * signal may be considerable, depending on the hardware.
     *
     * @see applied()
     * @see failed()
     */
    void apply();

    operator zkwinft_output_configuration_v1*();
    operator zkwinft_output_configuration_v1*() const;

Q_SIGNALS:
    /**
     * The server has applied all settings successfully. Pending changes in the
     * OutputDevices have been cleared, changed signals from the OutputDevice have
     * been emitted.
     */
    void applied();
    /**
     * The server has failed to apply the settings or rejected them. Pending changes
     * in the * OutputDevices have been cleared. No changes have been applied to the
     * OutputDevices.
     */
    void failed();

private:
    friend class OutputManagementV1;
    explicit OutputConfigurationV1(QObject* parent = nullptr);
    class Private;
    std::unique_ptr<Private> d;
};

}
}

Q_DECLARE_METATYPE(Wrapland::Client::OutputConfigurationV1*)
