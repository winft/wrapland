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

#include <memory>

#include <Wrapland/Server/wraplandserver_export.h>

namespace Wrapland::Server
{
class Client;
class Display;
class OutputInterface;
class XdgOutput;

class WRAPLANDSERVER_EXPORT XdgOutputManager : public QObject
{
    Q_OBJECT
public:
    ~XdgOutputManager() override;
    XdgOutput* createXdgOutput(Output* output, QObject* parent);

private:
    explicit XdgOutputManager(Display* display, QObject* parent = nullptr);
    friend class Display;

    class Private;
    std::unique_ptr<Private> d_ptr;
};

class WRAPLANDSERVER_EXPORT XdgOutput : public QObject
{
    Q_OBJECT
public:
    ~XdgOutput() override;

    QSize logicalSize() const;
    void setLogicalSize(const QSize& size);

    QPoint logicalPosition() const;
    void setLogicalPosition(const QPoint& pos);

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
    XdgOutputV1(Client* client, uint32_t version, uint32_t id, XdgOutputManager* parent);

    void setLogicalSize(const QSize& size) const;
    void setLogicalPosition(const QPoint& pos) const;
    void done() const;

    class Private;
    Private* d_ptr;

Q_SIGNALS:
    void resourceDestroyed();
};

}
