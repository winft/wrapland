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

#include <QObject>

#include <Wrapland/Server/wraplandserver_export.h>

#include <memory>

namespace Wrapland
{
namespace Server
{

class D_isplay;

/**
 * @brief Global for server side Display Power Management Signaling interface.
 *
 * A DpmsManagerInterface allows a client to query the DPMS state
 * on a given OutputInterface and request changes to it.
 * Server-side the interaction happens only via the OutputInterface,
 * for clients the Dpms class provides the API.
 * This global implements org_kde_kwin_dpms_manager.
 *
 * To create a DpmsManagerInterface use:
 * @code
 * auto manager = display->createDpmsManager();
 * manager->create();
 * @endcode
 *
 * To interact with Dpms use one needs to mark it as enabled and set the
 * proper mode on the OutputInterface.
 * @code
 * // We have our OutputInterface called output.
 * output->setDpmsSupported(true);
 * output->setDpmsMode(OutputInterface::DpmsMode::On);
 * @endcode
 *
 * To connect to Dpms change requests use:
 * @code
 * connect(output, &OutputInterface::dpmsModeRequested,
 *         [] (Wrapland::Server::OutputInterface::DpmsMode requestedMode) {
 *             qDebug() << "Mode change requested";
 *         });
 * @endcode
 *
 * @see Output
 * @since 0.5XX.0
 **/
class WRAPLANDSERVER_EXPORT DpmsManager : public QObject
{
    Q_OBJECT
public:
    ~DpmsManager() override;

private:
    explicit DpmsManager(D_isplay* display, QObject* parent = nullptr);
    friend class D_isplay;

    class Private;
    std::unique_ptr<Private> d_ptr;
};

}
}
