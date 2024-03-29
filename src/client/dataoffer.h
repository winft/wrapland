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
#ifndef WAYLAND_DATAOFFER_H
#define WAYLAND_DATAOFFER_H

#include <QObject>
// STD
#include <memory>

#include <Wrapland/Client/wraplandclient_export.h>

#include "datadevicemanager.h"

struct wl_data_offer;

class QMimeType;

namespace Wrapland
{
namespace Client
{
class DataDevice;

/**
 * @short Wrapper for the wl_data_offer interface.
 *
 * This class is a convenient wrapper for the wl_data_offer interface.
 * The DataOffer gets created by DataDevice.
 *
 * @see DataOfferManager
 **/
class WRAPLANDCLIENT_EXPORT DataOffer : public QObject
{
    Q_OBJECT
public:
    explicit DataOffer(DataDevice* parent, wl_data_offer* dataOffer);
    virtual ~DataOffer();

    /**
     * Releases the wl_data_offer interface.
     * After the interface has been released the DataOffer instance is no
     * longer valid and can be setup with another wl_data_offer interface.
     **/
    void release();

    /**
     * @returns @c true if managing a wl_data_offer.
     **/
    bool isValid() const;

    QList<QMimeType> offeredMimeTypes() const;

    void receive(QMimeType const& mimeType, qint32 fd);
    void receive(QString const& mimeType, qint32 fd);

    /**
     * Notifies the compositor that the drag destination successfully
     * finished the drag-and-drop operation.
     *
     * After this operation it is only allowed to release the DataOffer.
     *
     * @since 0.0.542
     **/
    void dragAndDropFinished();

    /**
     * The actions offered by the DataSource.
     * @since 0.0.542
     * @see sourceDragAndDropActionsChanged
     **/
    DataDeviceManager::DnDActions sourceDragAndDropActions() const;

    /**
     * Sets the @p supported and @p preferred Drag and Drop actions.
     * @since 0.0.542
     **/
    void setDragAndDropActions(DataDeviceManager::DnDActions supported,
                               DataDeviceManager::DnDAction preferred);

    /**
     * The currently selected drag and drop action by the compositor.
     * @see selectedDragAndDropActionChanged
     * @since 0.0.542
     **/
    DataDeviceManager::DnDAction selectedDragAndDropAction() const;

    operator wl_data_offer*();
    operator wl_data_offer*() const;

Q_SIGNALS:
    void mimeTypeOffered(QString const&);
    /**
     * Emitted whenever the @link{sourceDragAndDropActions} changed, e.g. on enter or when
     * the DataSource changes the supported actions.
     * @see sourceDragAndDropActions
     * @since 0.0.542
     **/
    void sourceDragAndDropActionsChanged();
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

Q_DECLARE_METATYPE(Wrapland::Client::DataOffer*)

#endif
