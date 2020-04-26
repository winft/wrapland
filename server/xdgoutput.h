/****************************************************************************
Copyright 2020  Adrien Faveraux <ad1rie3@hotmail.fr>

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

#include "display.h"
#include "output.h"

#include "wayland/client.h"
#include "wayland/display.h"
#include "wayland/global.h"
#include "wayland/resource.h"

#include <memory>

#include <Wrapland/Server/wraplandserver_export.h>

namespace Wrapland::Server
{
class D_isplay;
class OutputInterface;
class XdgOutput;

class WRAPLANDSERVER_EXPORT XdgOutputManager : public QObject
{
    Q_OBJECT
public:
    ~XdgOutputManager() override;
    XdgOutput* createXdgOutput(Output* output, QObject* parent);

private:
    explicit XdgOutputManager(D_isplay* display, QObject* parent = nullptr);
    friend class D_isplay;
    class Private;
    Private* d_ptr;
};

class WRAPLANDSERVER_EXPORT XdgOutput : public QObject
{
    Q_OBJECT
public:
    ~XdgOutput() override;

    /**
     * Sets the size of this output in logical co-ordinates.
     * Users should call done() after setting all values
     */
    void setLogicalSize(const QSize& size);

    /**
     * Returns the last set logical size on this output
     */
    QSize logicalSize() const;

    /**
     * Sets the topleft position of this output in logical co-ordinates.
     * Users should call done() after setting all values
     * @see OutputInterface::setPosition
     */
    void setLogicalPosition(const QPoint& pos);

    /**
     * Returns the last set logical position on this output
     */
    QPoint logicalPosition() const;

    /**
     * Submit changes to all clients
     */
    void done();

private:
    explicit XdgOutput(QObject* parent);
    friend class XdgOutputManager;

    class Private;
    std::unique_ptr<Private> d_ptr;
};

class XdgOutputV1 : public QObject
{
    Q_OBJECT
public:
    XdgOutputV1(Wayland::Client* client, uint32_t version, uint32_t id, XdgOutputManager* parent);
    ~XdgOutputV1() override;
    void setLogicalSize(const QSize& size);
    void setLogicalPosition(const QPoint& pos);
    void done();
    class Private;
    Private* d_ptr;

Q_SIGNALS:
    void resourceDestroyed();
};

}
