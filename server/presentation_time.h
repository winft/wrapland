/****************************************************************************
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
****************************************************************************/
#pragma once

#include <QObject>

#include <Wrapland/Server/wraplandserver_export.h>

#include <ctime>
#include <memory>

namespace Wrapland::Server
{

class Client;
class Display;
class output;
class Surface;

class WRAPLANDSERVER_EXPORT PresentationManager : public QObject
{
    Q_OBJECT
public:
    explicit PresentationManager(Display* display);
    ~PresentationManager() override;

    clockid_t clockId() const;
    void setClockId(clockid_t clockId);

private:
    class Private;
    std::unique_ptr<Private> d_ptr;
};

class WRAPLANDSERVER_EXPORT PresentationFeedback : public QObject
{
    Q_OBJECT
public:
    enum class Kind : std::uint8_t {
        Vsync = 1 << 0,
        HwClock = 1 << 1,
        HwCompletion = 1 << 2,
        ZeroCopy = 1 << 3,
    };
    Q_DECLARE_FLAGS(Kinds, Kind)

    ~PresentationFeedback() override;

    void sync(Server::output* output);
    void presented(uint32_t tvSecHi,
                   uint32_t tvSecLo,
                   uint32_t tvNsec,
                   uint32_t refresh,
                   uint32_t seqHi,
                   uint32_t seqLo,
                   Kinds kinds);
    void discarded();

Q_SIGNALS:
    void resourceDestroyed();

private:
    friend class PresentationManager;
    explicit PresentationFeedback(Wrapland::Server::Client* client, uint32_t version, uint32_t id);

    class Private;
    Private* d_ptr;
};

}

Q_DECLARE_METATYPE(Wrapland::Server::PresentationFeedback*)
Q_DECLARE_OPERATORS_FOR_FLAGS(Wrapland::Server::PresentationFeedback::Kinds)
