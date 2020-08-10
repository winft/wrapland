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
#include <memory>

#include <Wrapland/Server/wraplandserver_export.h>

namespace Wrapland::Server
{

class Display;
class WlOutput;
class RemoteBufferHandle;

class WRAPLANDSERVER_EXPORT RemoteAccessManager : public QObject
{
    Q_OBJECT
public:
    ~RemoteAccessManager() override;

    void sendBufferReady(WlOutput* output, RemoteBufferHandle* buf);
    bool bound() const;

Q_SIGNALS:
    void bufferReleased(RemoteBufferHandle* buf);

private:
    explicit RemoteAccessManager(Display* display, QObject* parent = nullptr);
    friend class Display;

    class Private;
    std::unique_ptr<Private> d_ptr;
};

class WRAPLANDSERVER_EXPORT RemoteBufferHandle : public QObject
{
    Q_OBJECT
public:
    RemoteBufferHandle();
    ~RemoteBufferHandle() override;

    void setFd(uint32_t fd);
    void setSize(uint32_t width, uint32_t height);
    void setStride(uint32_t stride);
    void setFormat(uint32_t format);

    uint32_t fd() const;
    uint32_t height() const;
    uint32_t width() const;
    uint32_t stride() const;
    uint32_t format() const;

private:
    friend class RemoteAccessManager;
    friend class RemoteBuffer;

    class Private;
    std::unique_ptr<Private> d_ptr;
};

}
