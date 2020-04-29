/********************************************************************
Copyright © 2020 Adrien Faveraux <ad1rie3@hotmail.fr>

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
#include <QRectF>
#include <QSize>
#include <memory>

#include <Wrapland/Server/wraplandserver_export.h>

namespace Wrapland::Server
{

class D_isplay;
class Surface;
class Viewport;

class WRAPLANDSERVER_EXPORT Viewporter : public QObject
{
    Q_OBJECT
public:
    ~Viewporter() override;

Q_SIGNALS:
    /**
     * Emitted whenever this Viewporter created a Viewport.
     **/
    void viewportCreated(Viewport*);

private:
    explicit Viewporter(D_isplay* display, QObject* parent = nullptr);
    friend class D_isplay;
    class Private;
    std::unique_ptr<Private> d_ptr;
};

class WRAPLANDSERVER_EXPORT Viewport : public QObject
{
    Q_OBJECT
public:
    ~Viewport() override;

    explicit Viewport(Wayland::Client* client,
                      uint32_t version,
                      uint32_t id,
                      Surface* surface,
                      QObject* parent = nullptr);
    friend class Viewporter;

    class Private;
    Private* d_ptr;

Q_SIGNALS:
    void destinationSizeSet(const QSize& size);
    void sourceRectangleSet(const QRectF& rect);
    void resourceDestroyed();
};
}
