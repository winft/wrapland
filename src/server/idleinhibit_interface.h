/****************************************************************************
Copyright 2017  Martin Flöser <mgraesslin@kde.org>

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
****************************************************************************/
#ifndef WRAPLAND_SERVER_IDLEINHIBIT_INTERFACE_H
#define WRAPLAND_SERVER_IDLEINHIBIT_INTERFACE_H

#include "global.h"
#include "resource.h"

#include <Wrapland/Server/wraplandserver_export.h>

namespace Wrapland
{
namespace Server
{

class Display;

/**
 * Enum describing the interface versions the IdleInhibitManagerInterface can support.
 *
 * @since 0.0.541
 **/
enum class IdleInhibitManagerInterfaceVersion {
    /**
     * zwp_idle_inhibit_manager_v1
     **/
     UnstableV1
};

/**
 * The IdleInhibitorManagerInterface is used by clients to inhibit idle on a
 * Surface. Whether a Surface inhibits idle is exposes through
 * @link{Surface::inhibitsIdle}.
 *
 * @since 0.0.541
 **/
class WRAPLANDSERVER_EXPORT IdleInhibitManagerInterface : public Global
{
    Q_OBJECT
public:
    virtual ~IdleInhibitManagerInterface();

    /**
     * @returns The interface version used by this IdleInhibitManagerInterface
     **/
    IdleInhibitManagerInterfaceVersion interfaceVersion() const;

protected:
    class Private;
    explicit IdleInhibitManagerInterface(Private *d, QObject *parent = nullptr);

private:
    Private *d_func() const;
};


}
}

#endif
