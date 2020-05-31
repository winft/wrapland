/****************************************************************************
Copyright 2020  Adrien Faveraux <af@brain-networks.fr>

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

class QMimeType;

namespace Wrapland::Server
{

class Display;
class Client;
class Seat;
class PrimarySelectionSource;
class PrimarySelectionDevice;
class PrimarySelectionOffer;

class WRAPLANDSERVER_EXPORT PrimarySelectionDeviceManager : public QObject
{
    Q_OBJECT
public:
    ~PrimarySelectionDeviceManager() override;

Q_SIGNALS:
    void primarySelectionSourceCreated(Wrapland::Server::PrimarySelectionSource* source);
    void primarySelectionDeviceCreated(Wrapland::Server::PrimarySelectionDevice* device);

private:
    explicit PrimarySelectionDeviceManager(Display* display, QObject* parent = nullptr);
    friend class Display;
    class Private;
    std::unique_ptr<Private> d_ptr;
};

class WRAPLANDSERVER_EXPORT PrimarySelectionDevice : public QObject
{
    Q_OBJECT
public:
    ~PrimarySelectionDevice() override;

    PrimarySelectionSource* selection();
    Seat* seat() const;
    Client* client() const;

    void sendSelection(PrimarySelectionDevice* device);
    void sendClearSelection();

Q_SIGNALS:
    void selectionChanged(PrimarySelectionSource* source);
    void selectionCleared();
    void resourceDestroyed();

private:
    PrimarySelectionDevice(Client* client, uint32_t version, uint32_t id, Seat* seat);
    friend class PrimarySelectionDeviceManager;

    class Private;
    Private* d_ptr;
};

class WRAPLANDSERVER_EXPORT PrimarySelectionOffer : public QObject
{
    Q_OBJECT
public:
    ~PrimarySelectionOffer() override;

    void sendOffer();

Q_SIGNALS:
    void resourceDestroyed();

private:
    explicit PrimarySelectionOffer(Client* client,
                                   uint32_t version,
                                   PrimarySelectionSource* source);
    friend class PrimarySelectionDevice;

    class Private;
    Private* d_ptr;
};

class WRAPLANDSERVER_EXPORT PrimarySelectionSource : public QObject
{
    Q_OBJECT
public:
    ~PrimarySelectionSource() override;
    void cancel();
    void requestData(std::string const& mimeType, qint32 fd);

    std::vector<std::string> mimeTypes();

    Client* client() const;

Q_SIGNALS:
    void mimeTypeOffered(std::string);
    void resourceDestroyed();

private:
    PrimarySelectionSource(Client* client, uint32_t version, uint32_t id);
    friend class PrimarySelectionDeviceManager;

    class Private;
    Private* d_ptr;
};

}
