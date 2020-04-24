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

#include <QObject>

#include "output_device_v1_interface.h"
#include <Wrapland/Server/wraplandserver_export.h>

#include <memory>

namespace Wrapland
{
namespace Server
{

/**
 * @brief Holds a set of changes to an OutputInterface or OutputDeviceV1Interface.
 *
 * This class implements a set of changes that the compositor can apply to an
 * OutputInterface after OutputConfigurationV1::apply has been called on the client
 * side. The changes are per-configuration.
 *
 * @see OutputConfigurationV1
 **/
class WRAPLANDSERVER_EXPORT OutputChangesetV1 : public QObject
{
    Q_OBJECT
public:
    ~OutputChangesetV1() override;

    /** Whether the enabled() property of the output device changed.
     * @returns @c true if the enabled property of the output device has changed.
     */
    bool enabledChanged() const;
    /** Whether the currentModeId() property of the output device changed.
     * @returns @c true if the enabled property of the output device has changed.
     *    bool modeChanged() const;
     */
     /** Whether the transform() property of the output device changed. */
    bool transformChanged() const;
    /** Whether the currentModeId() property of the output device changed.
     * @returns @c true if the currentModeId() property of the output device has changed.
     */
    bool modeChanged() const;
    /** Whether the geometry() property of the output device changed.
     * @returns @c true if the geometry() property of the output device has changed.
     */
    bool geometryChanged() const;

    /** The new value for enabled. */
    OutputDeviceV1Interface::Enablement enabled() const;
    /** The new mode id.*/
    int mode() const;
    /** The new value for transform. */
    OutputDeviceV1Interface::Transform transform() const;
    /** The new value for globalPosition. */
    QRectF geometry() const;

private:
    friend class OutputConfigurationV1Interface;
    explicit OutputChangesetV1(OutputDeviceV1Interface *outputDevice, QObject *parent = nullptr);


    class Private;
    std::unique_ptr<Private> d;
    Private *d_func() const;
};

}
}
