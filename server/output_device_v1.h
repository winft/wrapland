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
#include <QSize>
#include <memory>

#include <Wrapland/Server/wraplandserver_export.h>

class QRectF;

namespace Wrapland::Server
{

class Display;

class WRAPLANDSERVER_EXPORT OutputDeviceV1 : public QObject
{
    Q_OBJECT
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

    enum class Enablement { Disabled = 0, Enabled = 1 };

    enum class ModeFlag { Current = 1, Preferred = 2 };
    Q_DECLARE_FLAGS(ModeFlags, ModeFlag)

    struct Mode {
        QSize size = QSize();
        static constexpr int defaultRefreshRate = 60000;
        int refreshRate = defaultRefreshRate;
        ModeFlags flags;
        int id = -1;
    };

    ~OutputDeviceV1() override;

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

    void setUuid(const QByteArray& uuid);              // NOLINT
    void setEisaId(const QString& eisaId);             // NOLINT
    void setSerialNumber(const QString& serialNumber); // NOLINT
    void setEdid(const QByteArray& edid);              // NOLINT
    void setManufacturer(const QString& manufacturer); // NOLINT
    void setModel(const QString& model);               // NOLINT
    void setPhysicalSize(const QSize& size);           // NOLINT

    void setEnabled(Enablement enabled); // NOLINT

    void addMode(Mode& mode);
    void setMode(int id);

    void setTransform(Transform transform); // NOLINT
    void setGeometry(const QRectF& rect);   // NOLINT

    /**
     * Sends all pending changes out to connected clients. Must only be called when all atomic
     * changes to an output has been completed.
     */
    void done();

Q_SIGNALS:
    void uuidChanged(const QByteArray&);
    void eisaIdChanged(const QString&);
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
    explicit OutputDeviceV1(Display* display, QObject* parent = nullptr);
    friend class Display;

    class Private;
    std::unique_ptr<Private> d_ptr;
};

}

Q_DECLARE_OPERATORS_FOR_FLAGS(Wrapland::Server::OutputDeviceV1::ModeFlags)
Q_DECLARE_METATYPE(Wrapland::Server::OutputDeviceV1::Enablement)
Q_DECLARE_METATYPE(Wrapland::Server::OutputDeviceV1::Transform)
