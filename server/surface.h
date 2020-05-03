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

namespace Wrapland
{
namespace Server
{
class BlurManagerInterface;
class BlurInterface;
class Buffer;
class ConfinedPointerV1;
class ContrastInterface;
class ContrastManagerInterface;
class Compositor;
class IdleInhibitManagerV1;
class IdleInhibitor;
class LockedPointerV1;
class PointerConstraintsV1;
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
    ~Surface() override;

    void frameRendered(quint32 msec);

    QRegion damage() const;
    QRegion opaque() const;
    QRegion input() const;

    bool inputIsInfinite() const;

    qint32 scale() const;
    Output::Transform transform() const;

    Buffer* buffer();
    Buffer* buffer() const;

    QPoint offset() const;
    QSize size() const;

    Subsurface* subsurface() const;
    std::vector<Subsurface*> childSubsurfaces() const;

    QPointer<Shadow> shadow() const;
    QPointer<BlurInterface> blur() const;
    QPointer<ContrastInterface> contrast() const;
    QPointer<Slide> slideOnShowHide() const;
    QPointer<Viewport> viewport() const;

    bool isMapped() const;
    QRegion trackedDamage() const;
    void resetTrackedDamage();

    Surface* surfaceAt(const QPointF& position);
    Surface* inputSurfaceAt(const QPointF& position);

    void setOutputs(std::vector<Output*> outputs);
    std::vector<Output*> outputs() const;

    QPointer<ConfinedPointerV1> confinedPointer() const;
    QPointer<LockedPointerV1> lockedPointer() const;

    bool inhibitsIdle() const;

    Client* client() const;

    void setDataProxy(Surface* surface);
    Surface* dataProxy() const;

    QRectF sourceRectangle() const;

    wl_resource* resource() const;

Q_SIGNALS:
    void damaged(const QRegion&);
    void opaqueChanged(const QRegion&);
    void inputChanged(const QRegion&);
    void scaleChanged(qint32);
    void transformChanged(Wrapland::Server::Output::Transform);
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
    friend class Compositor;
    friend class Subsurface;
    friend class ShadowManager;
    friend class BlurManagerInterface;
    friend class SlideManager;
    friend class ContrastManagerInterface;
    friend class IdleInhibitManagerV1;
    friend class IdleInhibitor;

    friend class AppMenuManagerInterface;
    friend class PlasmaShellInterface;
    friend class XdgShell;
    friend class XdgShellSurface;

    friend class Seat;
    friend class Keyboard;
    friend class Pointer;
    friend class DataDevice;
    friend class PointerConstraintsV1;
    friend class PointerPinchGestureV1;
    friend class PointerSwipeGestureV1;
    friend class TextInputUnstableV0Interface;
    friend class TextInputUnstableV2Interface;

    friend class SurfaceRole;
    friend class Viewporter;

    explicit Surface(Client* client, uint32_t version, uint32_t id);

    class Private;
    Private* d_ptr;
};

}
}

Q_DECLARE_METATYPE(Wrapland::Server::Surface*)
