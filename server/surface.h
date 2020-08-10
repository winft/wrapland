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

#include "output.h"

#include <QObject>
#include <QPointer>
#include <QRegion>

#include <Wrapland/Server/wraplandserver_export.h>

namespace Wrapland::Server
{
class BlurManager;
class Blur;
class Buffer;
class ConfinedPointerV1;
class Contrast;
class ContrastManager;
class Compositor;
class IdleInhibitManagerV1;
class IdleInhibitor;
class LockedPointerV1;
class PointerConstraintsV1;
class PresentationFeedback;
class ShadowManager;
class Shadow;
class Slide;
class Subsurface;
class Viewport;
class Viewporter;

class WRAPLANDSERVER_EXPORT Surface : public QObject
{
    Q_OBJECT
public:
    enum class PresentationKind {
        None = 0,
        Vsync = 1 << 0,
        HwClock = 1 << 1,
        HwCompletion = 1 << 2,
        ZeroCopy = 1 << 3
    };
    Q_DECLARE_FLAGS(PresentationKinds, PresentationKind)

    void frameRendered(quint32 msec);

    QRegion damage() const;
    QRegion opaque() const;
    QRegion input() const;

    bool inputIsInfinite() const;

    qint32 scale() const;
    WlOutput::Transform transform() const;

    std::shared_ptr<Buffer> buffer() const;

    QPoint offset() const;
    QSize size() const;

    Subsurface* subsurface() const;
    std::vector<Subsurface*> childSubsurfaces() const;

    QPointer<Shadow> shadow() const;
    QPointer<Blur> blur() const;
    QPointer<Slide> slideOnShowHide() const;
    QPointer<Contrast> contrast() const;
    QPointer<Viewport> viewport() const;

    bool isMapped() const;
    QRegion trackedDamage() const;
    void resetTrackedDamage();

    Surface* surfaceAt(const QPointF& position);
    Surface* inputSurfaceAt(const QPointF& position);

    void setOutputs(std::vector<WlOutput*> const& outputs);
    std::vector<WlOutput*> outputs() const;

    QPointer<ConfinedPointerV1> confinedPointer() const;
    QPointer<LockedPointerV1> lockedPointer() const;

    bool inhibitsIdle() const;

    uint32_t id() const;
    Client* client() const;

    void setDataProxy(Surface* surface);
    Surface* dataProxy() const;

    QRectF sourceRectangle() const;

    uint32_t lockPresentation(WlOutput* output);
    void presentationFeedback(uint32_t presentationId,
                              uint32_t tvSecHi,
                              uint32_t tvSecLo,
                              uint32_t tvNsec,
                              uint32_t refresh,
                              uint32_t seqHi,
                              uint32_t seqLo,
                              PresentationKinds kinds);
    void presentationDiscarded(uint32_t presentationId);

    wl_resource* resource() const;

Q_SIGNALS:
    void damaged(const QRegion&);
    void opaqueChanged(const QRegion&);
    void inputChanged(const QRegion&);
    void scaleChanged(qint32);
    void transformChanged(Wrapland::Server::WlOutput::Transform);
    void unmapped();
    void sizeChanged();
    void shadowChanged();
    void blurChanged();
    void slideOnShowHideChanged();
    void contrastChanged();
    void sourceRectangleChanged();
    void subsurfaceTreeChanged();
    void pointerConstraintsChanged();
    void inhibitsIdleChanged();
    void committed();
    void resourceDestroyed();

private:
    friend class AppMenuManager;
    friend class BlurManager;
    friend class ContrastManager;
    friend class Compositor;
    friend class DataDevice;
    friend class Keyboard;
    friend class IdleInhibitManagerV1;
    friend class PlasmaShell;
    friend class Pointer;
    friend class PointerConstraintsV1;
    friend class PointerPinchGestureV1;
    friend class PointerSwipeGestureV1;
    friend class PresentationManager;
    friend class Seat;
    friend class ShadowManager;
    friend class SlideManager;
    friend class Subsurface;
    friend class TextInputV2;
    friend class Viewporter;
    friend class XdgShell;
    friend class XdgShellSurface;

    explicit Surface(Client* client, uint32_t version, uint32_t id);

    class Private;
    Private* d_ptr;
};

}

Q_DECLARE_METATYPE(Wrapland::Server::Surface*)
Q_DECLARE_OPERATORS_FOR_FLAGS(Wrapland::Server::Surface::PresentationKinds)
