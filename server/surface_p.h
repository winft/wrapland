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

#include "surface.h"

#include "wayland/resource.h"

#include <QHash>
#include <QVector>

#include <deque>
#include <functional>
#include <unordered_map>
#include <wayland-server.h>

namespace Wrapland::Server
{

class Feedbacks;
class IdleInhibitor;
class LayerSurfaceV1;
class XdgShellSurface;

class SurfaceState
{
public:
    SurfaceState() = default;

    SurfaceState(SurfaceState const&) = delete;
    SurfaceState& operator=(SurfaceState const&) = delete;

    SurfaceState(SurfaceState&&) noexcept = delete;
    SurfaceState& operator=(SurfaceState&&) noexcept = default;

    ~SurfaceState() = default;

    surface_state pub;

    QRegion bufferDamage = QRegion();

    bool destinationSizeIsSet = false;

    std::deque<wl_resource*> callbacks;

    QSize destinationSize = QSize();

    std::unique_ptr<Feedbacks> feedbacks{std::make_unique<Feedbacks>()};
};

class Surface::Private : public Wayland::Resource<Surface>
{
public:
    Private(Client* client, uint32_t version, uint32_t id, Surface* q_ptr);
    ~Private() override;

    void addChild(Subsurface* child);
    void removeChild(Subsurface* child);

    bool raiseChild(Subsurface* subsurface, Surface* sibling);
    bool lowerChild(Subsurface* subsurface, Surface* sibling);

    void setShadow(Shadow* shadow);
    void setBlur(Blur* blur);
    void setSlide(Slide* slide);
    void setContrast(Contrast* contrast);

    void setSourceRectangle(QRectF const& source);
    void setDestinationSize(QSize const& dest);
    void addPresentationFeedback(PresentationFeedback* feedback) const;

    void installPointerConstraint(LockedPointerV1* lock);
    void installPointerConstraint(ConfinedPointerV1* confinement);
    void installIdleInhibitor(IdleInhibitor* inhibitor);
    void installViewport(Viewport* vp);

    void commit();

    void updateCurrentState(bool forceChildren);
    void updateCurrentState(SurfaceState& source, bool forceChildren);

    bool has_role() const;

    bool had_buffer_attached{false};

    XdgShellSurface* shellSurface = nullptr;
    Subsurface* subsurface = nullptr;
    LayerSurfaceV1* layer_surface{nullptr};

    SurfaceState current;
    SurfaceState pending;

    QRegion trackedDamage;

    // Workaround for https://bugreports.qt.io/browse/QTBUG-52192:
    // A subsurface needs to be considered mapped even if it doesn't have a buffer attached.
    // Otherwise Qt's sub-surfaces will never be visible and the client will freeze due to
    // waiting on the frame callback of the never visible surface.
    //    bool subsurfaceIsMapped = true;

    std::vector<WlOutput*> outputs;

    uint32_t feedbackId = 0;
    std::unordered_map<uint32_t, std::unique_ptr<Feedbacks>> waitingFeedbacks;

    LockedPointerV1* lockedPointer{nullptr};
    ConfinedPointerV1* confinedPointer{nullptr};
    Viewport* viewport{nullptr};
    QHash<WlOutput*, QMetaObject::Connection> outputDestroyedConnections;
    QVector<IdleInhibitor*> idleInhibitors;

private:
    void update_buffer(SurfaceState const& source, bool& resized);
    void copy_to_current(SurfaceState const& source, bool& resized);
    void synced_child_update();

    void damage(QRect const& rect);
    void damageBuffer(QRect const& rect);

    void setScale(qint32 scale);
    void setTransform(output_transform transform);

    void addFrameCallback(uint32_t callback);
    void attachBuffer(wl_resource* wlBuffer, QPoint const& offset);

    void setOpaque(QRegion const& region);
    void setInput(QRegion const& region, bool isInfinite);

    /**
     * Posts Wayland error in case the source rectangle needs to be integer valued but is not.
     */
    void soureRectangleIntegerCheck(QSize const& destinationSize,
                                    QRectF const& sourceRectangle) const;
    /**
     * Posts Wayland error in case the source rectangle is not contained in surface size.
     */
    void soureRectangleContainCheck(Buffer const* buffer,
                                    output_transform transform,
                                    qint32 scale,
                                    QRectF const& sourceRectangle) const;

    static void destroyFrameCallback(wl_resource* wlResource);

    static void attachCallback(wl_client* wlClient,
                               wl_resource* wlResource,
                               wl_resource* buffer,
                               int32_t pos_x,
                               int32_t pos_y);
    static void damageCallback(wl_client* wlClient,
                               wl_resource* wlResource,
                               int32_t pos_x,
                               int32_t pos_y,
                               int32_t width,
                               int32_t height);
    static void frameCallback(wl_client* wlClient, wl_resource* wlResource, uint32_t callback);
    static void
    opaqueRegionCallback(wl_client* wlClient, wl_resource* wlResource, wl_resource* wlRegion);
    static void
    inputRegionCallback(wl_client* wlClient, wl_resource* wlResource, wl_resource* wlRegion);
    static void commitCallback(wl_client* wlClient, wl_resource* wlResource);

    // Since version 2.
    static void
    bufferTransformCallback(wl_client* wlClient, wl_resource* wlResource, int32_t transform);

    // Since version 3.
    static void bufferScaleCallback(wl_client* wlClient, wl_resource* wlResource, int32_t scale);

    // Since version 4.
    static void damageBufferCallback(wl_client* wlClient,
                                     wl_resource* wlResource,
                                     int32_t pos_x,
                                     int32_t pos_y,
                                     int32_t width,
                                     int32_t height);

    template<typename Obj>
    bool needs_resource_reset(Obj const& current,
                              Obj const& pending,
                              Obj const& obj,
                              surface_change change) const
    {
        auto const is_current = current == obj;
        auto const is_pending_set = this->pending.pub.updates & change;
        auto const obj_is_pending = pending == obj;

        if (is_pending_set && obj_is_pending) {
            // The object is pending. Needs to be changed in any case.
            return true;
        }

        // Needs reset if the object is current and there is no other pending object.
        return is_current || !is_pending_set;
    }

    template<typename Obj>
    void move_state_resource(SurfaceState const& source,
                             surface_change change,
                             Obj*& current,
                             Obj* const& pending,
                             QMetaObject::Connection& destroy_notifier,
                             std::function<void(Obj*)> resetter) const
    {
        if (!(source.pub.updates & change)) {
            return;
        }

        QObject::disconnect(destroy_notifier);
        current = pending;

        if (!current) {
            return;
        }

        destroy_notifier = QObject::connect(
            current, &Obj::resourceDestroyed, handle, [resetter, current] { resetter(current); });
    }

    static const struct wl_surface_interface s_interface;

    QMetaObject::Connection constrainsOneShotConnection;
    QMetaObject::Connection constrainsUnboundConnection;

    struct {
        QMetaObject::Connection blur;
        QMetaObject::Connection contrast;
        QMetaObject::Connection shadow;
        QMetaObject::Connection slide;
    } destroy_notifiers;

    Surface* q_ptr;
};

class Feedbacks : public QObject
{
    Q_OBJECT
public:
    explicit Feedbacks(QObject* parent = nullptr);
    ~Feedbacks() override;

    bool active();
    void add(PresentationFeedback* feedback);
    void setOutput(Server::output* output);
    void handleOutputRemoval();

    void presented(uint32_t tvSecHi,
                   uint32_t tvSecLo,
                   uint32_t tvNsec,
                   uint32_t refresh,
                   uint32_t seqHi,
                   uint32_t seqLo,
                   Surface::PresentationKinds kinds);
    void discard();

private:
    std::vector<PresentationFeedback*> m_feedbacks;
    Server::output* m_output = nullptr;
};

}
