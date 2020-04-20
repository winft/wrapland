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
#include <QPoint>
#include <QSize>
#include <QVector>

#include <QDebug>

#include <Wrapland/Server/wraplandserver_export.h>

#include <string>
#include <vector>

struct wl_global;
struct wl_client;
struct wl_resource;

namespace Wrapland
{
namespace Server
{

class Client;
class D_isplay;
class OutputInterface;

class WRAPLANDSERVER_EXPORT Output : public QObject
{
    Q_OBJECT
public:
    enum class DpmsMode {
        On,
        Standby,
        Suspend,
        Off,
    };

    enum class ModeFlag {
        Current = 1,
        Preferred = 2,
    };
    Q_DECLARE_FLAGS(ModeFlags, ModeFlag)

    enum class SubPixel {
        Unknown,
        None,
        HorizontalRGB,
        HorizontalBGR,
        VerticalRGB,
        VerticalBGR,
    };

    enum class Transform {
        Normal,
        Rotated90,
        Rotated180,
        Rotated270,
        Flipped,
        Flipped90,
        Flipped180,
        Flipped270,
    };

    struct Mode {
        QSize size = QSize();
        int refreshRate = 60000;
        ModeFlags flags;
    };

    ~Output() override;

    QSize physicalSize() const;
    QPoint globalPosition() const;
    const std::string& manufacturer() const;
    const std::string& model() const;

    QSize pixelSize() const;
    int refreshRate() const;
    int scale() const;
    Transform transform() const;

    SubPixel subPixel() const;
    const std::vector<Mode>& modes() const;

    bool isDpmsSupported() const;
    DpmsMode dpmsMode() const;

    void setPhysicalSize(const QSize& size);
    void setGlobalPosition(const QPoint& pos);

    void setManufacturer(std::string const& manufacturer);
    void setModel(std::string const& model);

    void setScale(int scale);
    void setSubPixel(SubPixel subPixel);
    void setTransform(Transform transform);
    void addMode(const QSize& size, ModeFlags flags = ModeFlags(), int refreshRate = 60000);
    void setCurrentMode(const QSize& size, int refreshRate = 60000);

    void setDpmsSupported(bool supported);
    void setDpmsMode(DpmsMode mode);

    // legacy
    OutputInterface* legacy;
    static Output* get(void* data);
    QVector<wl_resource*> getResources(Client* client);
    //

Q_SIGNALS:
    void physicalSizeChanged(const QSize&);
    void globalPositionChanged(const QPoint&);
    void manufacturerChanged(std::string const&);
    void modelChanged(std::string const&);
    void pixelSizeChanged(const QSize&);
    void refreshRateChanged(int);
    void scaleChanged(int);
    void subPixelChanged(SubPixel);
    void transformChanged(Transform);
    void modesChanged();
    void currentModeChanged();
    void dpmsModeChanged();
    void dpmsSupportedChanged();

    /**
     * Change of dpms @p mode is requested.
     * A server is free to ignore this request.
     * @since 5.5
     **/
    void dpmsModeRequested(Wrapland::Server::Output::DpmsMode mode);

private:
    friend class D_isplay;
    friend class Surface;

    explicit Output(Wrapland::Server::D_isplay* display, QObject* parent = nullptr);

    class Private;
    Private* d_ptr;
};

}
}

Q_DECLARE_OPERATORS_FOR_FLAGS(Wrapland::Server::Output::ModeFlags)
Q_DECLARE_METATYPE(Wrapland::Server::Output::SubPixel)
Q_DECLARE_METATYPE(Wrapland::Server::Output::Transform)
Q_DECLARE_METATYPE(Wrapland::Server::Output::DpmsMode)
