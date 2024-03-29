/********************************************************************
Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>

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
#ifndef WAYLAND_SURFACE_H
#define WAYLAND_SURFACE_H

#include "buffer.h"

#include <QObject>
#include <QPoint>
#include <QSize>
#include <QWindow>

#include <Wrapland/Client/wraplandclient_export.h>

struct wl_buffer;
struct wl_surface;

class QWindow;

namespace Wrapland
{
namespace Client
{

class Output;
class Region;

/**
 * @short Wrapper for the wl_surface interface.
 *
 * This class is a convenient wrapper for the wl_surface interface.
 * To create a Surface call Compositor::createSurface.
 *
 * The main purpose of this class is to setup the next frame which
 * should be rendered. Therefore it provides methods to add damage
 * and to attach a new Buffer and to finalize the frame by calling
 * commit.
 *
 * @see Compositor
 **/
class WRAPLANDCLIENT_EXPORT Surface : public QObject
{
    Q_OBJECT
public:
    explicit Surface(QObject* parent = nullptr);
    virtual ~Surface();

    /**
     * Creates a Surface for the given @p window.
     * This is an integration feature for QtWayland. On non-wayland platforms this method returns
     * @c nullptr as well as for not created QWindows.
     *
     * The returned Surface will be fully setup, but won't be released. It gets automatically
     * destroyed together with the @p window.
     * @since 5.4
     **/
    static Surface* fromWindow(QWindow* window);

    /**
     * Creates a Surface for the given @p winId.
     * This is an integration feature for QtWayland. On non-wayland platforms this method returns
     * @c nullptr as well as for not created QWindows.
     *
     * The returned Surface will be fully setup, but won't be released. It gets automatically
     * destroyed together with the QWindow corresponding
     * the @p wid.
     * @since 5.5
     **/
    static Surface* fromQtWinId(WId wid);

    /**
     * Setup this Surface to manage the @p surface.
     * When using Compositor::createSurface there is no need to call this
     * method.
     **/
    void setup(wl_surface* surface);
    /**
     * Releases the wl_surface interface.
     * After the interface has been released the Surface instance is no
     * longer valid and can be setup with another wl_surface interface.
     **/
    void release();

    /**
     * @returns @c true if managing a wl_surface.
     **/
    bool isValid() const;
    /**
     * Registers a frame rendered callback.
     * This registers a callback in the Wayland server to be notified once the
     * next frame for this Surface has been rendered. The Surface will emit the
     * signal frameRendered after receiving the callback from the server.
     *
     * Instead of using this method one should prefer using the CommitFlag::FrameCallback
     * in commit. This method is intended for cases when the Surface is going to
     * be committed on other ways, e.g. through the OpenGL/EGL stack.
     *
     * @see frameRendered
     * @see commit
     **/
    void setupFrameCallback();
    /**
     * Flags to be added to commit.
     * @li None: no flag
     * @li FrameCallback: register a frame rendered callback
     *
     * Instead of setting the FrameCallback flag one can also call
     * setupFrameCallback. If one uses setupFrameCallback one may not
     * use the FrameCallback flag when committing the Surface.
     *
     * @see commit
     * @see setupFrameCallback
     **/
    enum class CommitFlag {
        None,
        FrameCallback,
    };
    void commit(CommitFlag flag = CommitFlag::FrameCallback);
    /**
     * Mark @p rect as damaged for the next frame.
     * @see damageBuffer
     **/
    void damage(QRect const& rect);
    /**
     * Mark @p region as damaged for the next frame.
     * @see damageBuffer
     **/
    void damage(QRegion const& region);
    /**
     * Mark @p rect in buffer coordinates as damaged for the next frame.
     * @see damage
     * @since 0.0.559
     **/
    void damageBuffer(QRect const& rect);
    /**
     * Mark @p region in buffer coordinates as damaged for the next frame.
     * @see damage
     * @since 0.0.559
     **/
    void damageBuffer(QRegion const& region);
    /**
     * Attaches the @p buffer to this Surface for the next frame.
     * @param buffer The buffer to attach to this Surface
     * @param offset Position of the new upper-left corner in relation to previous frame
     **/
    void attachBuffer(wl_buffer* buffer, QPoint const& offset = QPoint());
    /**
     * Overloaded method for convenience.
     **/
    void attachBuffer(Buffer* buffer, QPoint const& offset = QPoint());
    /**
     * Overloaded method for convenience.
     **/
    void attachBuffer(Buffer::Ptr buffer, QPoint const& offset = QPoint());
    /**
     * Sets the input region to @p region.
     *
     * This is a double buffered state and will be applied with the next Surface
     * commit. Initially the Surface is set up to an infinite input region.
     * By passing @c null as the input region, it gets reset to an infinite input
     * region.
     *
     * Note: the Region is being copied and can be destroyed directly after passing
     * to this method.
     *
     * @param region The new input region or an infinite region if @c null
     * @see commit
     **/
    void setInputRegion(Region const* region = nullptr);
    /**
     * Sets the opaque region to @p region.
     *
     * This is a double buffered state and will be applied with the next Surface
     * commit. Initially the Surface is set up to an empty opaque region.
     * By passing @c null as the opaque region, it gets reset to an empty opaque
     * region.
     *
     * Note: the Region is being copied and can be destroyed directly after passing
     * to this method.
     *
     * @param region The new opaque region or an empty region if @c null
     * @see commit
     **/
    void setOpaqueRegion(Region const* region = nullptr);
    void setSize(QSize const& size);
    QSize size() const;

    /**
     * The purpose of this method is to allow to supply higher resolution buffer data for use
     * on high resolution outputs. It's intended that the same buffer scale as the scale of the
     * output that the surface is displayed on is used.
     * This means the compositor can avoid scaling when rendering the surface on that output.
     *
     * Note that if @p scale is larger than 1 you have to attach a buffer that is larger
     * (by a factor of scale in each dimension) than the desired surface size.
     *
     * The default scale factor is 1.
     *
     * The state is only applied with the next commit.
     *
     * @see scale
     * @see commit
     * @since 0.0.522
     **/
    void setScale(qint32 scale);
    /**
     * @returns The current scale factor, if not explicitly set it's @c 1.
     * @see setScale
     * @since 0.0.522
     **/
    qint32 scale() const;

    operator wl_surface*();
    operator wl_surface*() const;

    /**
     * @returns the id of the referenced wl_proxy.
     * @since 5.4
     **/
    quint32 id() const;

    /**
     * @returns All Outputs the Surface is on, may be none.
     * @see outputEntered
     * @see outputLeft
     * @since 0.0.527
     **/
    QVector<Output*> outputs() const;

    /**
     * All Surfaces which are currently created.
     * TODO: KF6 return QList<Surface*> instead of const-ref
     **/
    static QList<Surface*> const& all(); // krazy:exclude=constref
    /**
     * @returns The Surface referencing the @p native wl_surface or @c null if there is no such
     * Surface.
     **/
    static Surface* get(wl_surface* native);

Q_SIGNALS:
    /**
     * Emitted when the server indicates that the last committed frame has been rendered.
     * The signal will only be emitted if a callback has been registered by either calling
     * setupFrameCallback or calling commit with the CommitFlag::FrameCallback.
     * @see setupFrameCallback
     * @see commit
     **/
    void frameRendered();
    void sizeChanged(QSize const&);

    /**
     * Emitted whenever a change in the Surface (e.g. creation, movement, resize) results in
     * a part of the Surface being within the scanout region of the Output @p o.
     *
     * @param o The Output the Surface intersects with
     * @see outputLeft
     * @see outputs
     * @since 0.0.527
     **/
    void outputEntered(Wrapland::Client::Output* o);

    /**
     * Emitted whenever a change in the Surface (e.g. creation, movement, resize, unmapping)
     * results in the Surface no longer being within the scanout region of the Output @p o.
     *
     * @param o The Output the Surface no longer intersects with
     * @see outputEntered
     * @see outputs
     * @since 0.0.527
     **/
    void outputLeft(Wrapland::Client::Output* o);

private:
    class Private;
    std::unique_ptr<Private> d;
};

}
}

#endif
