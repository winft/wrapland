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

#include <Wrapland/Server/wraplandserver_export.h>

#include <memory>
#include <string>
#include <vector>

namespace Wrapland::Server
{
class Display;
class OutputDeviceV1;
class WlOutput;
class XdgOutput;

/**
 * Central class for outputs in Wrapland. Manages and forwards all required information to and from
 * other output related classes such that compositors only need to interact with the Output class
 * under normal circumstances.
 */
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
    Q_ENUM(DpmsMode)

    enum class Subpixel {
        Unknown,
        None,
        HorizontalRGB,
        HorizontalBGR,
        VerticalRGB,
        VerticalBGR,
    };
    Q_ENUM(Subpixel)

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
    Q_ENUM(Transform)

    struct Mode {
        bool operator==(Mode const& mode) const;
        bool operator!=(Mode const& mode) const;
        QSize size;
        static constexpr int defaultRefreshRate = 60000;
        int refresh_rate{defaultRefreshRate};
        bool preferred{false};
        int id{-1};
    };

    explicit Output(Display* display, QObject* parent = nullptr);
    ~Output() override;

    std::string uuid() const;
    std::string eisa_id() const;
    std::string serial_mumber() const;
    std::string edid() const;
    std::string manufacturer() const;
    std::string model() const;
    QSize physical_size() const;

    void set_uuid(std::string const& uuid);
    void set_eisa_id(std::string const& eisa_id);
    void set_serial_number(std::string const& serial_number);
    void set_edid(std::string const& edid);

    void set_manufacturer(std::string const& manufacturer);
    void set_model(std::string const& model);
    void set_physical_size(QSize const& size);

    bool enabled() const;
    void set_enabled(bool enabled);

    std::vector<Mode> modes() const;
    int mode_id() const;
    QSize mode_size() const;
    int refresh_rate() const;

    void add_mode(Mode const& mode);

    bool set_mode(int id);
    bool set_mode(Mode const& mode);
    bool set_mode(QSize const& size, int refresh_rate);

    Transform transform() const;
    QRectF geometry() const;

    void set_transform(Transform transform);
    void set_geometry(QRectF const& geometry);

    int client_scale() const;

    Subpixel subpixel() const;
    void set_subpixel(Subpixel subpixel);

    bool dpms_supported() const;
    void set_dpms_supported(bool supported);

    DpmsMode dpms_mode() const;
    void set_dpms_mode(DpmsMode mode);

    /**
     * Sends all pending changes out to connected clients. Must only be called when all atomic
     * changes to an output has been completed.
     */
    void done();

    OutputDeviceV1* output_device_v1() const;
    WlOutput* wayland_output() const;
    XdgOutput* xdg_output() const;

Q_SIGNALS:
    void dpms_mode_changed();
    void dpms_supported_changed();
    void dpms_mode_requested(Wrapland::Server::Output::DpmsMode mode);

private:
    friend class Display;
    friend class OutputDeviceV1;
    friend class WlOutput;
    friend class XdgOutput;

    class Private;
    std::unique_ptr<Private> d_ptr;
};

}

Q_DECLARE_METATYPE(Wrapland::Server::Output::Subpixel)
Q_DECLARE_METATYPE(Wrapland::Server::Output::Transform)
Q_DECLARE_METATYPE(Wrapland::Server::Output::DpmsMode)
