/********************************************************************
Copyright 2020 Faveraux Adrien <ad1rie3@hotmail.fr>

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

#include <Wrapland/Server/wraplandserver_export.h>

#include <memory>

namespace Wrapland::Server
{

class D_isplay;
class Seat;

class WRAPLANDSERVER_EXPORT KdeIdle : public QObject
{
    Q_OBJECT
public:
    ~KdeIdle() override;

    void inhibit();
    void uninhibit();
    bool isInhibited() const;
    void simulateUserActivity();

Q_SIGNALS:
    void inhibitedChanged();

private:
    explicit KdeIdle(D_isplay* display, QObject* parent = nullptr);
    friend class D_isplay;
    class Private;
    std::unique_ptr<Private> d_ptr;
};

}
