/********************************************************************
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
*********************************************************************/
#pragma once

#include <QMarginsF>
#include <QObject>

#include <Wrapland/Server/wraplandserver_export.h>
#include <memory>

namespace Wrapland::Server
{
class Buffer;
class Client;
class Display;

class WRAPLANDSERVER_EXPORT ShadowManager : public QObject
{
    Q_OBJECT
public:
    ~ShadowManager() override;

private:
    explicit ShadowManager(Display* display, QObject* parent = nullptr);
    friend class Display;
    class Private;
    std::unique_ptr<Private> d_ptr;
};

class WRAPLANDSERVER_EXPORT Shadow : public QObject
{
    Q_OBJECT
public:
    std::shared_ptr<Buffer> left() const;
    std::shared_ptr<Buffer> topLeft() const;
    std::shared_ptr<Buffer> top() const;
    std::shared_ptr<Buffer> topRight() const;
    std::shared_ptr<Buffer> right() const;
    std::shared_ptr<Buffer> bottomRight() const;
    std::shared_ptr<Buffer> bottom() const;
    std::shared_ptr<Buffer> bottomLeft() const;

    QMarginsF offset() const;

Q_SIGNALS:
    void resourceDestroyed();

private:
    explicit Shadow(Client* client, uint32_t version, uint32_t id);
    friend class ShadowManager;

    class Private;
    Private* d_ptr;
};

}
