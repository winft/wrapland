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

    QRegion damage = QRegion();
    QRegion bufferDamage = QRegion();
    QRegion opaque = QRegion();
    QRegion input = QRegion();

    bool inputIsSet = false;
    bool opaqueIsSet = false;
    bool bufferIsSet = false;
    bool shadowIsSet = false;
    bool blurIsSet = false;
    bool contrastIsSet = false;
    bool slideIsSet = false;
    bool inputIsInfinite = true;
    bool childrenChanged = false;
    bool scaleIsSet = false;
    bool transformIsSet = false;
    bool sourceRectangleIsSet = false;
    bool destinationSizeIsSet = false;

    qint32 scale = 1;
    Output::Transform transform = Output::Transform::Normal;

    std::deque<wl_resource*> callbacks;

    QPoint offset = QPoint();
    std::shared_ptr<Buffer> buffer;

    QRectF sourceRectangle = QRectF();
    QSize destinationSize = QSize();

    // Stacking order: bottom (first) -> top (last).
    std::vector<Subsurface*> children;

    QPointer<Shadow> shadow;
    QPointer<Blur> blur;
    QPointer<Slide> slide;
    QPointer<Contrast> contrast;
    std::unique_ptr<Feedbacks> feedbacks{std::make_unique<Feedbacks>()};
};

class Surface::Private : public Wayland::Resource<Surface>
{
public:
    Private(Client* client, uint32_t version, uint32_t id, Surface* q);
    ~Private() override;

    void addChild(Subsurface* child);
    void removeChild(Subsurface* child);

    bool raiseChild(Subsurface* subsurface, Surface* sibling);
    bool lowerChild(Subsurface* subsurface, Surface* sibling);

    void setShadow(const QPointer<Shadow>& shadow);
    void setBlur(const QPointer<Blur>& blur);
    void setSlide(const QPointer<Slide>& slide);
    void setContrast(const QPointer<Contrast>& contrast);

    void setSourceRectangle(const QRectF& source);
    void setDestinationSize(const QSize& dest);
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

    QPointer<LockedPointerV1> lockedPointer;
    QPointer<ConfinedPointerV1> confinedPointer;
    QPointer<Viewport> viewport;
    QHash<WlOutput*, QMetaObject::Connection> outputDestroyedConnections;
    QVector<IdleInhibitor*> idleInhibitors;

    Surface* dataProxy = nullptr;

private:
    void update_buffer(SurfaceState const& source, bool& resized);
    void copy_to_current(SurfaceState const& source, bool& resized);

    void damage(const QRect& rect);
    void damageBuffer(const QRect& rect);

    void setScale(qint32 scale);
    void setTransform(Output::Transform transform);

    void addFrameCallback(uint32_t callback);
    void attachBuffer(wl_resource* wlBuffer, const QPoint& offset);

    void setOpaque(const QRegion& region);
    void setInput(const QRegion& region, bool isInfinite);

    /**
     * Posts Wayland error in case the source rectangle needs to be integer valued but is not.
     */
    void soureRectangleIntegerCheck(const QSize& destinationSize,
                                    const QRectF& sourceRectangle) const;
    /**
     * Posts Wayland error in case the source rectangle is not contained in surface size.
     */
    void soureRectangleContainCheck(const Buffer* buffer,
                                    Output::Transform transform,
                                    qint32 scale,
                                    const QRectF& sourceRectangle) const;

    static void destroyFrameCallback(wl_resource* wlResource);

    static void attachCallback(wl_client* wlClient,
                               wl_resource* wlResource,
                               wl_resource* buffer,
                               int32_t sx,
                               int32_t sy);
    static void damageCallback(wl_client* wlClient,
                               wl_resource* wlResource,
                               int32_t x,
                               int32_t y,
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
                                     int32_t x,
                                     int32_t y,
                                     int32_t width,
                                     int32_t height);

    static const struct wl_surface_interface s_interface;

    QMetaObject::Connection constrainsOneShotConnection;
    QMetaObject::Connection constrainsUnboundConnection;

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
    void setOutput(Output* output);
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
    Output* m_output = nullptr;
};

}
