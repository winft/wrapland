/****************************************************************************
Copyright 2019 Vlad Zagorodniy <vlad.zahorodnii@kde.org>

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

#ifndef WRAPLAND_SERVER_SURFACEROLE_P_H
#define WRAPLAND_SERVER_SURFACEROLE_P_H

#include <QPointer>

namespace Wrapland
{
namespace Server
{

class Surface;

class SurfaceRole
{
public:
    explicit SurfaceRole(Surface *surface);
    virtual ~SurfaceRole();

    virtual void commit() = 0;

private:
    QPointer<Surface> m_surface;

    Q_DISABLE_COPY(SurfaceRole)
};

}
}

#endif // WRAPLAND_SERVER_SURFACEROLE_P_H
