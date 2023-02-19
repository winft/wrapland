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
#include <QRegion>

#include <Wrapland/Server/wraplandserver_export.h>

struct wl_resource;

namespace Wrapland::Server
{
class BlurManager;
class Blur;
class Buffer;
class Client;
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

enum class surface_change {
    none = 0,
    mapped = 1 << 0,
    buffer = 1 << 1,
    size = 1 << 2,
    opaque = 1 << 3,
    scale = 1 << 4,
    transform = 1 << 5,
    offset = 1 << 6,
    source_rectangle = 1 << 7,
    input = 1 << 8,
    children = 1 << 9,
    shadow = 1 << 10,
    blur = 1 << 11,
    slide = 1 << 12,
    contrast = 1 << 13,
    frame = 1 << 14,
};
Q_DECLARE_FLAGS(surface_changes, surface_change)

struct surface_state {
    std::shared_ptr<Buffer> buffer;

    QRegion damage;
    QRegion opaque;

    int32_t scale{1};
    Output::Transform transform{Output::Transform::Normal};
    QPoint offset;

    QRectF source_rectangle;

    QRegion input;
    bool input_is_infinite{true};

    // Stacking order: bottom (first) -> top (last).
    std::vector<Subsurface*> children;

    Shadow* shadow{nullptr};
    Blur* blur{nullptr};
    Slide* slide{nullptr};
    Contrast* contrast{nullptr};

    surface_changes updates{surface_change::none};
};

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

    surface_state const& state() const;

    void frameRendered(quint32 msec);

    QSize size() const;

    /**
     * Bounds of the surface and all subsurfaces. Origin is the top-left corner of the surface.
     */
    QRect expanse() const;

    Subsurface* subsurface() const;

    bool isMapped() const;
    QRegion trackedDamage() const;
    void resetTrackedDamage();

    void setOutputs(std::vector<Output*> const& outputs);
    void setOutputs(std::vector<WlOutput*> const& outputs);
    std::vector<WlOutput*> outputs() const;

    ConfinedPointerV1* confinedPointer() const;
    LockedPointerV1* lockedPointer() const;

    bool inhibitsIdle() const;

    uint32_t id() const;
    Client* client() const;

    uint32_t lockPresentation(Output* output);
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
    friend class data_device;
    friend class Keyboard;
    friend class IdleInhibitManagerV1;
    friend class input_method_v2;
    friend class LayerShellV1;
    friend class LayerSurfaceV1;
    friend class PlasmaShell;
    friend class Pointer;
    friend class PointerConstraintsV1;
    friend class PointerPinchGestureV1;
    friend class PointerSwipeGestureV1;
    friend class PointerHoldGestureV1;
    friend class PresentationManager;
    friend class Seat;
    friend class ShadowManager;
    friend class SlideManager;
    friend class Subsurface;
    friend class text_input_v2;
    friend class text_input_v3;
    friend class Viewporter;
    friend class XdgShell;
    friend class XdgShellSurface;

    explicit Surface(Client* client, uint32_t version, uint32_t id);

    class Private;
    Private* d_ptr;
};

}

Q_DECLARE_METATYPE(Wrapland::Server::Surface*)
Q_DECLARE_OPERATORS_FOR_FLAGS(Wrapland::Server::surface_changes)
Q_DECLARE_OPERATORS_FOR_FLAGS(Wrapland::Server::Surface::PresentationKinds)
