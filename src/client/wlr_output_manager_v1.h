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
#include <QVector>
//STD
#include <memory>
#include <Wrapland/Client/wraplandclient_export.h>

struct zwlr_output_head_v1;
struct zwlr_output_manager_v1;
struct zwlr_output_mode_v1;

namespace Wrapland
{
namespace Client
{

class EventQueue;
class WlrOutputConfigurationV1;
class WlrOutputHeadV1;

/**
 * @short Wrapper for the zwlr_output_manager_v1 interface.
 *
 * This class provides a convenient wrapper for the zwlr_output_manager_v1 interface.
 *
 * To use this class one needs to interact with the Registry. There are two
 * possible ways to create the WlrOutputManagerV1 interface:
 * @code
 * WlrOutputManagerV1 *c = registry->createWlrOutputManagerV1(name, version);
 * @endcode
 *
 * This creates the WlrOutputManagerV1 and sets it up directly. As an alternative this
 * can also be done in a more low level way:
 * @code
 * WlrOutputManagerV1 *c = new WlrOutputManagerV1;
 * c->setup(registry->bindWlrOutputManagerV1(name, version));
 * @endcode
 *
 * The WlrOutputManagerV1 can be used as a drop-in replacement for any zwlr_output_manager_v1
 * pointer as it provides matching cast operators.
 *
 * @see Registry
 * @since 0.519.0
 */
class WRAPLANDCLIENT_EXPORT WlrOutputManagerV1 : public QObject
{
    Q_OBJECT
public:
    /**
     * Creates a new WlrOutputManagerV1.
     * Note: after constructing the WlrOutputManagerV1 it is not yet valid and one needs
     * to call setup. In order to get a ready to use WlrOutputManagerV1 prefer using
     * Registry::createWlrOutputManagerV1.
     */
    explicit WlrOutputManagerV1(QObject *parent = nullptr);
    ~WlrOutputManagerV1() override;

    /**
     * Setup this WlrOutputManagerV1 to manage the @p WlrOutputManagerV1.
     * When using Registry::createWlrOutputManagerV1 there is no need to call this
     * method.
     */
    void setup(zwlr_output_manager_v1 *outputManager);
    /**
     * @returns @c true if managing a zwlr_output_manager_v1.
     */
    bool isValid() const;
    /**
     * Releases the zwlr_output_manager_v1 interface.
     * After the interface has been released the WlrOutputManager instance is no
     * longer valid and can be setup with another zwlr_output_manager_v1 interface.
     */
    void release();

    /**
     * Sets the @p queue to use for creating objects with this WlrOutputManager.
     */
    void setEventQueue(EventQueue *queue);
    /**
     * @returns The event queue to use for creating objects with this WlrOutputManager.
     */
    EventQueue *eventQueue();

    /**
      * Create a configuration object ready for being used in a configure operation.
      * @param parent
      * @return configuration object.
      */
    WlrOutputConfigurationV1 *createConfiguration(QObject *parent = nullptr);

    operator zwlr_output_manager_v1*();
    operator zwlr_output_manager_v1*() const;

Q_SIGNALS:
    /**
      * A new output head appeared.
      * @param head
      */
    void head(WlrOutputHeadV1 *head);
    /**
      * All information has been sent.
      */
    void done();

    /**
     * The corresponding global for this interface on the Registry got removed.
     *
     * This signal gets only emitted if the WlrOutputManager got created by
     * Registry::createWlrOutputManagerV1
     */
    void removed();

private:
    class Private;
    std::unique_ptr<Private> d;
};

class WRAPLANDCLIENT_EXPORT WlrOutputModeV1 : public QObject
{
    Q_OBJECT
public:
    ~WlrOutputModeV1() override;

    QSize size() const;
    int refresh() const;
    bool preferred() const;

    operator zwlr_output_mode_v1*();
    operator zwlr_output_mode_v1*() const;

Q_SIGNALS:
    void removed();

private:
    explicit WlrOutputModeV1(zwlr_output_mode_v1 *mode, QObject *parent = nullptr);
    friend class WlrOutputHeadV1;

    class Private;
    std::unique_ptr<Private> d;
};

class WRAPLANDCLIENT_EXPORT WlrOutputHeadV1 : public QObject
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
    ~WlrOutputHeadV1() override;

    QString name() const;
    QString description() const;
    QSize physicalSize() const;

    bool enabled() const;

    QVector<WlrOutputModeV1*> modes() const;
    WlrOutputModeV1* currentMode() const;

    QPoint position() const;
    Transform transform() const;
    double scale() const;

    operator zwlr_output_head_v1*();
    operator zwlr_output_head_v1*() const;

Q_SIGNALS:
    void changed();
    void removed();

private:
    explicit WlrOutputHeadV1(zwlr_output_head_v1 *head, QObject *parent = nullptr);
    friend class WlrOutputManagerV1;

    class Private;
    std::unique_ptr<Private> d;
};

}
}
