/********************************************************************
Copyright © 2013        Martin Gräßlin <mgraesslin@kde.org>
Copyright © 2018-2020   Roman Gilg <subdiff@gmail.com>

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

#include <Wrapland/Client/wraplandclient_export.h>

#include <QObject>
#include <QSize>
#include <QVector>
#include <memory>
#include <vector>

class QRectF;
struct zkwinft_output_device_v1;

namespace Wrapland
{
namespace Client
{

class EventQueue;

/**
 * @short Wrapper for the zkwinft_output_device_v1 interface.
 *
 * This class provides a convenient wrapper for the zkwinft_output_device_v1 interface.
 * Its main purpose is to hold the information about one OutputDeviceV1.
 *
 * To use this class one needs to interact with the Registry. There are two
 * possible ways to create an OutputDeviceV1 interface:
 * @code
 * OutputDeviceV1 *c = registry->createOutputDeviceV1(name, version);
 * @endcode
 *
 * This creates the OutputDeviceV1 and sets it up directly. As an alternative this
 * can also be done in a more low level way:
 * @code
 * OutputDeviceV1 *c = new OutputDeviceV1;
 * c->setup(registry->bindOutputDeviceV1(name, version));
 * @endcode
 *
 * The OutputDeviceV1 can be used as a drop-in replacement for any zkwinft_output_device_v1
 * pointer as it provides matching cast operators.
 *
 * Please note that all properties of OutputDeviceV1 are not valid until the
 * changed signal has been emitted. The wayland server is pushing the
 * information in an async way to the OutputDeviceV1 instance. By emitting changed
 * the OutputDeviceV1 indicates that all relevant information is available.
 *
 * @see Registry
 **/
class WRAPLANDCLIENT_EXPORT OutputDeviceV1 : public QObject
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
        Flipped270,
    };

    enum class Enablement {
        Disabled = 0,
        Enabled = 1,
    };

    struct Mode {
        /**
         * The size of this Mode in pixel space.
         **/
        QSize size;
        /**
         * The refresh rate in mHz of this Mode.
         **/
        int refreshRate = 0;
        bool preferred{false};
        /**
         * The id of this mode, unique per OutputDeviceV1. This id can be used to call
         * OutputConfiguration->setMode();
         * @see OutputConfiguration::setMode
         **/
        int id;

        bool operator==(Mode const& m) const;
    };

    explicit OutputDeviceV1(QObject* parent = nullptr);
    virtual ~OutputDeviceV1();

    /**
     * Setup this Compositor to manage the @p output.
     * When using Registry::createOutputDeviceV1 there is no need to call this
     * method.
     **/
    void setup(zkwinft_output_device_v1* output);

    /**
     * @returns @c true if managing a zkwinft_output_device_v1.
     **/
    bool isValid() const;
    operator zkwinft_output_device_v1*();
    operator zkwinft_output_device_v1*() const;
    zkwinft_output_device_v1* output();

    /**
     * Size in millimeters.
     **/
    QSize physicalSize() const;

    QString name() const;
    QString description() const;

    /**
     * Textual description of the manufacturer.
     **/
    QString make() const;
    /**
     * Textual description of the model.
     **/
    QString model() const;
    /**
     * Textual representation of serial number.
     */
    QString serialNumber() const;
    /**
     * Size in the current mode.
     **/
    QSize pixelSize() const;
    /**
     * The geometry of this OutputDeviceV1 in pixels.
     * Convenient for QRect(globalPosition(), pixelSize()).
     * @see globalPosition
     * @see pixelSize
     **/
    QRectF geometry() const;
    /**
     * Refresh rate in mHz of the current mode.
     **/
    int refreshRate() const;

    /**
     * Transform that maps framebuffer to OutputDeviceV1.
     *
     * The purpose is mainly to allow clients render accordingly and tell the compositor,
     * so that for fullscreen surfaces, the compositor will still be able to scan out
     * directly from client surfaces.
     **/
    Transform transform() const;

    /**
     * @returns The Modes of this OutputDeviceV1.
     **/
    std::vector<Mode> const& modes() const;

    Wrapland::Client::OutputDeviceV1::Mode currentMode() const;

    /**
     * Sets the @p queue to use for bound proxies.
     **/
    void setEventQueue(EventQueue* queue);
    /**
     * @returns The event queue to use for bound proxies.
     **/
    EventQueue* eventQueue() const;

    /**
     * @returns Whether this output is enabled or not.
     **/
    OutputDeviceV1::Enablement enabled() const;

    /**
     * Releases the zkwinft_output_device_v1 interface.
     * After the interface has been released the OutputDeviceV1 instance is no
     * longer valid and can be setup with another zkwinft_output_device_v1 interface.
     **/
    void release();

Q_SIGNALS:
    /**
     * Emitted when the output is fully initialized.
     **/
    void done();

    /**
     * Emitted when the data change has been completed.
     **/
    void changed();

    /**
     * Emitted whenever the enabled property changes.
     **/
    void enabledChanged(OutputDeviceV1::Enablement enabled);

    /**
     * Emitted whenever a new Mode is added.
     * This normally only happens during the initial promoting of modes.
     * Afterwards only modeChanged should be emitted.
     * @param mode The newly added Mode.
     * @see modeChanged
     **/
    void modeAdded(Wrapland::Client::OutputDeviceV1::Mode const& mode);

    /**
     * Emitted whenever a Mode changes.
     * This normally means that the @c Mode::Flag::Current is added or removed.
     * @param mode The changed Mode
     **/
    void modeChanged(Wrapland::Client::OutputDeviceV1::Mode const& mode);

    /**
     * Emitted whenever the geometry changes.
     **/
    void geometryChanged(QRectF const& geometry);

    /**
     * The corresponding global for this interface on the Registry got removed.
     *
     * This signal gets only emitted if the OutputDeviceV1 got created by
     * Registry::createOutputDeviceV1
     **/
    void removed();

private:
    class Private;
    std::unique_ptr<Private> d;
};

}
}

Q_DECLARE_METATYPE(Wrapland::Client::OutputDeviceV1::Transform)
Q_DECLARE_METATYPE(Wrapland::Client::OutputDeviceV1::Enablement)
Q_DECLARE_METATYPE(Wrapland::Client::OutputDeviceV1::Mode)
