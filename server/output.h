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

#include <Wrapland/Server/wraplandserver_export.h>

#include <QObject>
#include <QRectF>
#include <QSize>
#include <memory>
#include <string>
#include <vector>

namespace Wrapland::Server
{

class Display;
class OutputDeviceV1;
class WlOutput;
class XdgOutput;

enum class output_dpms_mode {
    on,
    standby,
    suspend,
    off,
};

enum class output_subpixel {
    unknown,
    none,
    horizontal_rgb,
    horizontal_bgr,
    vertical_rgb,
    vertical_bgr,
};

enum class output_transform {
    normal,
    rotated_90,
    rotated_180,
    rotated_270,
    flipped,
    flipped_90,
    flipped_180,
    flipped_270,
};

struct output_mode {
    bool operator==(output_mode const& mode) const;
    bool operator!=(output_mode const& mode) const;
    QSize size;
    static constexpr int defaultRefreshRate = 60000;
    int refresh_rate{defaultRefreshRate};
    bool preferred{false};
    int id{-1};
};

struct output_metadata {
    std::string name{"Unknown"};
    std::string description;
    std::string make;
    std::string model;
    std::string serial_number;
    QSize physical_size;
};

struct output_state {
    output_metadata meta;
    bool enabled{false};

    output_mode mode;
    output_subpixel subpixel{output_subpixel::unknown};

    output_transform transform{output_transform::normal};
    QRectF geometry;
    int client_scale = 1;
};

/**
 * Central class for outputs in Wrapland. Manages and forwards all required information to and from
 * other output related classes such that compositors only need to interact with the Output class
 * under normal circumstances.
 */
class WRAPLANDSERVER_EXPORT output : public QObject
{
    Q_OBJECT
public:
    explicit output(Display* display);
    ~output() override;

    std::string name() const;
    std::string description() const;
    std::string make() const;
    std::string model() const;
    std::string serial_mumber() const;
    QSize physical_size() const;
    int connector_id() const;

    void set_name(std::string const& name);
    void set_description(std::string const& description);
    void set_make(std::string const& make);
    void set_model(std::string const& model);
    void set_serial_number(std::string const& serial_number);
    void set_physical_size(QSize const& size);
    void set_connector_id(int id);

    /**
     * Produces a description from available data. The pattern will be:
     * - if make or model available: "<make> <model> (<name>)"
     * - otherwise: "<name>"
     */
    void generate_description();

    bool enabled() const;
    void set_enabled(bool enabled);

    std::vector<output_mode> modes() const;
    int mode_id() const;
    QSize mode_size() const;
    int refresh_rate() const;

    void add_mode(output_mode const& mode);

    bool set_mode(int id);
    bool set_mode(output_mode const& mode);
    bool set_mode(QSize const& size, int refresh_rate);

    output_transform transform() const;
    QRectF geometry() const;

    void set_transform(output_transform transform);
    void set_geometry(QRectF const& geometry);

    int client_scale() const;

    output_subpixel subpixel() const;
    void set_subpixel(output_subpixel subpixel);

    bool dpms_supported() const;
    void set_dpms_supported(bool supported);

    output_dpms_mode dpms_mode() const;
    void set_dpms_mode(output_dpms_mode mode);

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
    void dpms_mode_requested(Wrapland::Server::output_dpms_mode mode);

private:
    friend class OutputDeviceV1;
    friend class WlOutput;
    friend class XdgOutput;

    class Private;
    std::unique_ptr<Private> d_ptr;
};

}

Q_DECLARE_METATYPE(Wrapland::Server::output_subpixel)
Q_DECLARE_METATYPE(Wrapland::Server::output_transform)
Q_DECLARE_METATYPE(Wrapland::Server::output_dpms_mode)
