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
// STD
#include <Wrapland/Client/wraplandclient_export.h>
#include <memory>

struct zkwinft_output_configuration_v1;
struct zkwinft_output_management_v1;

namespace Wrapland
{
namespace Client
{

class EventQueue;
class OutputConfigurationV1;
class OutputDeviceV1;

/**
 * @short Wrapper for the zkwinft_output_management_v1 interface.
 *
 * This class provides a convenient wrapper for the zkwinft_output_management_v1 interface.
 *
 * To use this class one needs to interact with the Registry. There are two
 * possible ways to create the OutputManagementV1 interface:
 * @code
 * OutputManagementV1 *c = registry->createOutputManagementV1(name, version);
 * @endcode
 *
 * This creates the OutputManagementV1 and sets it up directly. As an alternative this
 * can also be done in a more low level way:
 * @code
 * OutputManagementV1 *c = new OutputManagementV1;
 * c->setup(registry->bindOutputManagementV1(name, version));
 * @endcode
 *
 * The OutputManagementV1 can be used as a drop-in replacement for any zkwinft_output_management_v1
 * pointer as it provides matching cast operators.
 *
 * @see Registry
 * @since 5.5
 **/
class WRAPLANDCLIENT_EXPORT OutputManagementV1 : public QObject
{
    Q_OBJECT
public:
    /**
     * Creates a new OutputManagementV1.
     * Note: after constructing the OutputManagementV1 it is not yet valid and one needs
     * to call setup. In order to get a ready to use OutputManagementV1 prefer using
     * Registry::createOutputManagementV1.
     **/
    explicit OutputManagementV1(QObject* parent = nullptr);
    ~OutputManagementV1() override;

    /**
     * Setup this OutputManagementV1 to manage the @p OutputManagementV1.
     * When using Registry::createOutputManagementV1 there is no need to call this
     * method.
     **/
    void setup(zkwinft_output_management_v1* outputManagement);
    /**
     * @returns @c true if managing a zkwinft_output_management_v1.
     **/
    bool isValid() const;
    /**
     * Releases the zkwinft_output_management_v1 interface.
     * After the interface has been released the OutputManagement instance is no
     * longer valid and can be setup with another zkwinft_output_management_v1 interface.
     **/
    void release();

    /**
     * Sets the @p queue to use for creating objects with this OutputManagement.
     **/
    void setEventQueue(EventQueue* queue);
    /**
     * @returns The event queue to use for creating objects with this OutputManagement.
     **/
    EventQueue* eventQueue();

    OutputConfigurationV1* createConfiguration(QObject* parent = nullptr);

    operator zkwinft_output_management_v1*();
    operator zkwinft_output_management_v1*() const;

Q_SIGNALS:
    /**
     * The corresponding global for this interface on the Registry got removed.
     *
     * This signal gets only emitted if the OutputManagement got created by
     * Registry::createOutputManagement
     **/
    void removed();

private:
    class Private;
    std::unique_ptr<Private> d;
};

}
}
