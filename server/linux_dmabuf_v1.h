/********************************************************************
Copyright © 2018 Fredrik Höglund <fredrik@kde.org>
Copyright © 2019-2020 Roman Gilg <subdiff@gmail.com>

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

#include <QHash>
#include <QObject>
#include <QSet>
#include <QSize>

#include <memory>

namespace Wrapland
{
namespace Server
{
class Buffer;
class Display;
class LinuxDmabufBufferV1;

/**
 * Represents the global zpw_linux_dmabuf_v1 interface.
 *
 * This interface provides a way for clients to create generic dmabuf based wl_buffers.
 */
class WRAPLANDSERVER_EXPORT LinuxDmabufV1 : public QObject
{
    Q_OBJECT
public:
    enum Flag {
        YInverted = (1 << 0),        /// Contents are y-inverted
        Interlaced = (1 << 1),       /// Content is interlaced
        BottomFieldFirst = (1 << 2), /// Bottom field first
    };

    Q_DECLARE_FLAGS(Flags, Flag)

    /**
     * Represents a plane in a buffer
     */
    struct Plane {
        int fd;            /// The dmabuf file descriptor
        uint32_t offset;   /// The offset from the start of buffer
        uint32_t stride;   /// The distance from the start of a row to the next row in bytes
        uint64_t modifier; /// The layout modifier
    };

    /**
     * The Impl class provides an interface from the LinuxDmabufInterface into the compositor.
     */
    class Impl
    {
    public:
        Impl() = default;
        virtual ~Impl() = default;

        /**
         * Imports a linux-dmabuf buffer into the compositor.
         *
         * The parent LinuxDmabufV1 class takes ownership of returned
         * buffer objects.
         *
         * In return the returned buffer takes ownership of the file descriptor for each
         * plane.
         *
         * Note that it is the responsibility of the caller to close the file descriptors
         * when the import fails.
         *
         * @return The imported buffer on success, and nullptr otherwise.
         */
        virtual LinuxDmabufBufferV1*
        importBuffer(const QVector<Plane>& planes, uint32_t format, const QSize& size, Flags flags)
            = 0;
    };

    ~LinuxDmabufV1() override;

    /**
     * Sets the compositor implementation for the dmabuf interface.
     *
     * The ownership is not transferred by this call.
     */
    void setImpl(Impl* impl);

    void setSupportedFormatsWithModifiers(QHash<uint32_t, QSet<uint64_t>> const& set);

private:
    explicit LinuxDmabufV1(Display* display, QObject* parent = nullptr);

    friend class Display;
    friend class Buffer;
    friend class ParamsWrapperV1;
    friend class ParamsV1;

    class Private;
    std::unique_ptr<Private> d_ptr;
};

class WRAPLANDSERVER_EXPORT LinuxDmabufBufferV1 : public QObject
{
    Q_OBJECT
public:
    LinuxDmabufBufferV1(uint32_t format, const QSize& size, QObject* parent = nullptr);
    ~LinuxDmabufBufferV1() override;

    uint32_t format() const;
    QSize size() const;

Q_SIGNALS:
    void resourceDestroyed();

private:
    friend class ParamsV1;
    class Private;
    Private* d_ptr;
};

}
}

Q_DECLARE_METATYPE(Wrapland::Server::LinuxDmabufV1*)
Q_DECLARE_OPERATORS_FOR_FLAGS(Wrapland::Server::LinuxDmabufV1::Flags)
