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

#include "wlr_output_manager_v1.h"

#include <Wrapland/Client/wraplandclient_export.h>

#include <QObject>
#include <QPoint>

struct zkwinft_output_management_v1;
struct zwlr_output_configuration_v1;

namespace Wrapland
{
namespace Client
{

class EventQueue;

/** @class WlrOutputConfigurationV1
 *
 * WlrOutputConfigurationV1 provides access to changing WlrOutputHeadV1s. The interface is async
 * and atomic. An WlrOutputConfigurationV1 is created through
 * WlrOutputManagerV1::createConfiguration().
 *
 * The overall mechanism is to get a new WlrOutputConfigurationV1 from the WlrOutputManagerV1 global
 * and apply changes through the WlrOutputConfigurationV1::set* calls. When all changes are set, the
 * client calls apply, which asks the server to look at the changes and apply them. The server will
 * then signal back whether the changes have been applied successfully (@c applied()) or were
 * rejected or failed to apply (@c failed()).
 *
 * The current settings for heads can be gotten from @c Registry::WlrOutputHeadV1s(), these
 * are used in the set* calls to identify the output the setting applies to.
 *
 * These Wrapland classes will not apply changes to the heads, this is the compositor's
 * task. As such, the configuration set through this interface can be seen as a hint what the
 * compositor should set up, but whether or not the compositor does it (based on hardware or
 * rendering policies, for example), is up to the compositor. The mode setting is passed on to
 * the DRM subsystem through the compositor. The compositor also saves this configuration and reads
 * it on startup, this interface is not involved in that process.
 *
 * @c apply() should only be called after changes to all heads have been made, not after
 * each change. This allows to test the new configuration as a whole, and is a lot faster since
 * hardware changes can be tested in their new combination, they done in parallel.and rolled back
 * as a whole.
 *
 * \verbatim
    // We're just picking the first of our heads
    Wrapland::Client::WlrOutputHeadV1 *output = m_clientOutputs.first();

    // Create a new configuration object
    auto config = m_outputManagement.createConfiguration();

    // handle applied and failed signals
    connect(config, &WlrOutputConfigurationV1::applied, []() {
        qDebug() << "Configuration applied!";
    });
    connect(config, &WlrOutputConfigurationV1::failed, []() {
        qDebug() << "Configuration failed!";
    });

    // Change settings
    config->setMode(output, m_clientOutputs.first()->modes().last().id);
    config->setTransform(output, WlrOutputHeadV1::Transform::Normal);
    config->setPosition(output, QPoint(0, 1920));
    config->setScale(output, 2);

    // Now ask the compositor to apply the changes
    config->apply();
    // You may wait for the applied() or failed() signal here
   \endverbatim

 * @see WlrOutputHeadV1
 * @see WlrOutputManagerV1
 * @see WlrOutputManagerV1::createConfiguration()
 * @since 0.519.0
 */
class WRAPLANDCLIENT_EXPORT WlrOutputConfigurationV1 : public QObject
{
    Q_OBJECT
public:
    ~WlrOutputConfigurationV1() override;

    /**
     * Setup this WlrOutputConfigurationV1 to manage the @p outputconfiguration.
     * When using WlrOutputManagerV1::createOutputConfiguration there is no need to call this
     * method.
     * @param outputconfiguration the outputconfiguration object to set up.
     */
    void setup(zwlr_output_configuration_v1* outputconfiguration);
    /**
     * @returns @c true if managing a zwlr_output_configuration_v1.
     */
    bool isValid() const;
    /**
     * Releases the zwlr_output_configuration_v1 interface.
     * After the interface has been released the WlrOutputConfigurationV1 instance is no
     * longer valid and can be setup with another zwlr_output_configuration_v1 interface.
     */
    void release();

    /**
     * Sets the @p queue to use for creating a WlrOutputConfigurationV1.
     */
    void setEventQueue(EventQueue* queue);
    /**
     * @returns The event queue to use for creating a WlrOutputConfigurationV1
     */
    EventQueue* eventQueue();

    /**
     * Enable or disable an output. Enabled means it's used by the
     * compositor for rendering, Disabled means, that no wl_output
     * is connected to this, and the head is sitting there unused
     * by this compositor.
     * The changes done in this call will be recorded in the
     * head and only applied after apply() has been called.
     *
     * @param head the head this change applies to.
     * @param enable new Enablement state of this head.
     */
    void setEnabled(WlrOutputHeadV1* head, bool enable);

    /**
     * Set the mode of this output.
     *
     * @param head the head this change applies to.
     * @param mode the mode to set as current.
     */
    void setMode(WlrOutputHeadV1* head, WlrOutputModeV1* mode);

    /**
     * Set a custom mode for this output.
     *
     * @param head the head this change applies to.
     * @param size the custom mode size of the head.
     * @param refresh the custom refresh rate of the head.
     */
    void setCustomMode(WlrOutputHeadV1* head, QSize const& size, int refresh);

    /**
     * Set transformation for this output, for example rotated or flipped.
     * The changes done in this call will be recorded in the
     * head and only applied after apply() has been called.
     *
     * @param head the head this change applies to.
     * @param scale the scaling factor for this head.
     */
    void setTransform(WlrOutputHeadV1* head, WlrOutputHeadV1::Transform transform);

    /**
     * Sets the position of this output in the global space.
     *
     * @param head the head this change applies to.
     * @param geo the head geometry relative to other outputs,
     *
     */
    void setPosition(WlrOutputHeadV1* head, QPoint const& pos);

    /**
     * Sets the position of this output in the global space.
     *
     * @param head the head this change applies to.
     * @param geo the head geometry relative to other outputs,
     *
     */
    void setScale(WlrOutputHeadV1* head, double scale);

    void set_adaptive_sync(WlrOutputHeadV1* head, bool enable);
    void set_viewport(WlrOutputHeadV1* head, QSize const& size);

    /**
     * Check if current configuration is valid. Either succeeded() or failed() will be emitted
     * later on as a result of this call.
     */
    void test();

    /**
     * Ask the compositor to apply the changes.
     * This results in the compositor looking at all heads and if they have
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

    operator zwlr_output_configuration_v1*();
    operator zwlr_output_configuration_v1*() const;

Q_SIGNALS:
    /**
     * The server has applied all settings successfully. In case test() was called before that
     * nothing else happens. Otherwise pending changes in the heads have been cleared, changed
     * signals from the head have been emitted.
     */
    void succeeded();
    /**
     * The server has failed to apply the settings or rejected them. Pending changes
     * in the * heads have been cleared. No changes have been applied to the
     * heads.
     */
    void failed();

    /**
     * Sent if the current configuration is outdated. A new configuration object should be created
     * via the output manager global and this configura.
     */
    void cancelled();

private:
    explicit WlrOutputConfigurationV1(QObject* parent = nullptr);
    friend class WlrOutputManagerV1;

    class Private;
    std::unique_ptr<Private> d;
};

}
}

Q_DECLARE_METATYPE(Wrapland::Client::WlrOutputConfigurationV1*)
