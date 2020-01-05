/********************************************************************
Copyright © 2013 Martin Gräßlin <mgraesslin@kde.org>
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
#include <QPoint>
#include <QSize>
#include <QVector>

#include <Wrapland/Server/wraplandserver_export.h>
#include "global.h"

class QRectF;
struct wl_resource;

namespace Wrapland
{
namespace Server
{

class Display;

/** @class OutputDeviceV1Interface
 *
 * Represents an output device, the difference to Output is that this output can be disabled, in
 * this case not used to display content.
 *
 * @see OutputManagementV1Interface
 */
class WRAPLANDSERVER_EXPORT OutputDeviceV1Interface : public Global
{
    Q_OBJECT
    Q_PROPERTY(QByteArray uuid READ uuid WRITE setUuid NOTIFY uuidChanged)
    Q_PROPERTY(QString eisaId READ eisaId WRITE setEisaId NOTIFY eisaIdChanged)
    Q_PROPERTY(QString serialNumber READ serialNumber WRITE setSerialNumber
               NOTIFY serialNumberChanged)
    Q_PROPERTY(QByteArray edid READ edid WRITE setEdid NOTIFY edidChanged)
    Q_PROPERTY(QString manufacturer READ manufacturer WRITE setManufacturer
               NOTIFY manufacturerChanged)
    Q_PROPERTY(QString model READ model WRITE setModel NOTIFY modelChanged)
    Q_PROPERTY(QSize physicalSize READ physicalSize WRITE setPhysicalSize
               NOTIFY physicalSizeChanged)

    Q_PROPERTY(Enablement enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)

    Q_PROPERTY(QSize modeSize READ modeSize NOTIFY modeSizeChanged)
    Q_PROPERTY(int refreshRate READ refreshRate NOTIFY refreshRateChanged)

    Q_PROPERTY(QRectF geometry READ geometry WRITE setGeometry NOTIFY geometryChanged)

public:
    enum class Transform {
        Normal,
        Rotated90,
        Rotated180,
        Rotated270,
        Flipped,
        Flipped90,
        Flipped180,
        Flipped270
    };

    enum class Enablement {
        Disabled = 0,
        Enabled = 1
    };

    enum class ModeFlag {
        Current = 1,
        Preferred = 2
    };
    Q_DECLARE_FLAGS(ModeFlags, ModeFlag)

    struct Mode {
        QSize size = QSize();
        int refreshRate = 60000;
        ModeFlags flags;
        int id = -1;
    };

    ~OutputDeviceV1Interface() override;

    QByteArray uuid() const;
    QString eisaId() const;
    QString serialNumber() const;
    QByteArray edid() const;
    QString manufacturer() const;
    QString model() const;
    QSize physicalSize() const;

    Enablement enabled() const;

    QList<Mode> modes() const;
    int modeId() const;

    QSize modeSize() const;
    int refreshRate() const;

    Transform transform() const;

    QRectF geometry() const;

    void setUuid(const QByteArray &uuid);
    void setEisaId(const QString &eisaId);
    void setSerialNumber(const QString &serialNumber);
    void setEdid(const QByteArray &edid);
    void setManufacturer(const QString &manufacturer);
    void setModel(const QString &model);
    void setPhysicalSize(const QSize &size);

    void setEnabled(Enablement enabled);

    /**
     * Add an additional mode to this output device. This is only allowed before create() is called
     * on the object.
     *
     * Arbitrary many modes may be added, but only one of them should have the ModeFlag::Current
     * flag and one the ModeFlag::Preferred flag.
     *
     * @param mode must have a valid size and non-negative id.
     */
    void addMode(Mode &mode);
    void setMode(int id);

    void setTransform(Transform transform);

    void setGeometry(const QRectF &rect);

    /**
     * Sends all pending changes out to connected clients. Must only be called when all atomic
     * changes to an output has been completed.
     */
    void broadcast();

    static OutputDeviceV1Interface *get(wl_resource *native);
    static QList<OutputDeviceV1Interface *>list();

Q_SIGNALS:
    void uuidChanged(const QByteArray&);
    void eisaIdChanged(const QString &);
    void serialNumberChanged(const QString&);
    void edidChanged(const QByteArray&);
    void manufacturerChanged(const QString&);
    void modelChanged(const QString&);
    void physicalSizeChanged(const QSize&);

    void enabledChanged(Enablement);

    void modesChanged();
    void currentModeChanged();
    void modeSizeChanged(const QSize&);
    void refreshRateChanged(int);

    void transformChanged(Transform);
    void geometryChanged(const QRectF&);

private:
    friend class Display;
    explicit OutputDeviceV1Interface(Display *display, QObject *parent = nullptr);
    class Private;
    Private *d_func() const;
};

}
}

Q_DECLARE_OPERATORS_FOR_FLAGS(Wrapland::Server::OutputDeviceV1Interface::ModeFlags)
Q_DECLARE_METATYPE(Wrapland::Server::OutputDeviceV1Interface::Enablement)
Q_DECLARE_METATYPE(Wrapland::Server::OutputDeviceV1Interface::Transform)
