/********************************************************************
Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>

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
#ifndef WAYLAND_DATASOURCE_H
#define WAYLAND_DATASOURCE_H

#include "buffer.h"
#include "datadevicemanager.h"

#include <QObject>

#include <Wrapland/Client/wraplandclient_export.h>

struct wl_data_source;
class QMimeType;

namespace Wrapland
{
namespace Client
{

/**
 * @short Wrapper for the wl_data_source interface.
 *
 * This class is a convenient wrapper for the wl_data_source interface.
 * To create a DataSource call DataDeviceManager::createSource.
 *
 * @see DataDeviceManager
 **/
class WRAPLANDCLIENT_EXPORT DataSource : public QObject
{
    Q_OBJECT
public:
    explicit DataSource(QObject* parent = nullptr);
    virtual ~DataSource();

    /**
     * Setup this DataSource to manage the @p dataSource.
     * When using DataDeviceManager::createSource there is no need to call this
     * method.
     **/
    void setup(wl_data_source* dataSource);
    /**
     * Releases the wl_data_source interface.
     * After the interface has been released the DataSource instance is no
     * longer valid and can be setup with another wl_data_source interface.
     **/
    void release();

    /**
     * @returns @c true if managing a wl_data_source.
     **/
    bool isValid() const;

    void offer(QString const& mimeType);
    void offer(QMimeType const& mimeType);

    /**
     * Sets the actions that the source side client supports for this
     * operation.
     *
     * This request must be made once only, and can only be made on sources
     * used in drag-and-drop, so it must be performed before
     * @link{DataDevice::startDrag}. Attempting to use the source other than
     * for drag-and-drop will raise a protocol error.
     * @since 0.0.542
     **/
    void setDragAndDropActions(DataDeviceManager::DnDActions actions);

    /**
     * The currently selected drag and drop action by the compositor.
     * @see selectedDragAndDropActionChanged
     * @since 0.0.542
     **/
    DataDeviceManager::DnDAction selectedDragAndDropAction() const;

    operator wl_data_source*();
    operator wl_data_source*() const;

Q_SIGNALS:
    /**
     * Emitted when a target accepts pointer_focus or motion events. If
     * a target does not accept any of the offered types, @p mimeType is empty.
     **/
    void targetAccepts(QString const& mimeType);
    /**
     * Request for data from the client. Send the data as the
     * specified @p mimeType over the passed file descriptor @p fd, then close
     * it.
     **/
    void sendDataRequested(QString const& mimeType, qint32 fd);
    /**
     * This DataSource has been replaced by another DataSource.
     * The client should clean up and destroy this DataSource.
     **/
    void cancelled();

    /**
     * The drag-and-drop operation physically finished.
     *
     * The user performed the drop action. This signal does not
     * indicate acceptance, @link{cancelled} may still be
     * emitted afterwards if the drop destination does not accept any
     * mime type.
     *
     * However, this signal might not be received if the
     * compositor cancelled the drag-and-drop operation before this
     * signal could happen.
     *
     * Note that the DataSource may still be used in the future and
     * should not be destroyed here.
     * @since 0.0.542
     **/
    void dragAndDropPerformed();

    /**
     * The drag-and-drop operation concluded.
     *
     * The drop destination finished interoperating with this DataSource,
     * so the client is now free to destroy this DataSource.
     *
     * If the action used to perform the operation was "move", the
     * source can now delete the transferred data.
     * @since 0.0.542
     */
    void dragAndDropFinished();

    /**
     * Emitted whenever the selected drag and drop action changes.
     * @see selectedDragAndDropAction
     * @since 0.0.542
     **/
    void selectedDragAndDropActionChanged();

private:
    class Private;
    std::unique_ptr<Private> d;
};

}
}

#endif
