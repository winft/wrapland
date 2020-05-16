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
#include <QPointer>

#include <Wrapland/Server/wraplandserver_export.h>
#include <memory>

namespace Wrapland
{
namespace Server
{
class Client;
class D_isplay;
class Subsurface;
class Surface;

class WRAPLANDSERVER_EXPORT Subcompositor : public QObject
{
    Q_OBJECT
public:
    ~Subcompositor() override;

Q_SIGNALS:
    void subsurfaceCreated(Wrapland::Server::Subsurface*);

private:
    friend class D_isplay;
    explicit Subcompositor(D_isplay* display, QObject* parent = nullptr);

    class Private;
    std::unique_ptr<Private> d_ptr;
};

class WRAPLANDSERVER_EXPORT Subsurface : public QObject
{
    Q_OBJECT
public:
    ~Subsurface() override;

    QPoint position() const;

    enum class Mode {
        Synchronized,
        Desynchronized,
    };
    Mode mode() const;

    bool isSynchronized() const;

    Surface* surface() const;
    Surface* parentSurface() const;
    Surface* mainSurface() const;

Q_SIGNALS:
    void positionChanged(const QPoint&);
    void modeChanged(Wrapland::Server::Subsurface::Mode);
    void resourceDestroyed();

private:
    friend class Subcompositor;
    friend class Surface;
    explicit Subsurface(Client* client,
                        uint32_t version,
                        uint32_t id,
                        Surface* surface,
                        Surface* parentSurface);

    class Private;
    Private* d_ptr;
};

}
}

Q_DECLARE_METATYPE(Wrapland::Server::Subsurface::Mode)
