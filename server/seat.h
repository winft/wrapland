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

#include <QMatrix4x4>
#include <QObject>
#include <QPoint>

#include <Wrapland/Server/wraplandserver_export.h>

#include "global.h"
#include "keyboard_interface.h"
#include "pointer_interface.h"
#include "touch_interface.h"

struct wl_client;
struct wl_resource;

namespace Wrapland
{
namespace Server
{

class DataDeviceInterface;
class D_isplay;
class SurfaceInterface;
class TextInputInterface;

enum class PointerAxisSource {
    Unknown,
    Wheel,
    Finger,
    Continuous,
    WheelTilt,
};

class WRAPLANDSERVER_EXPORT Seat : public QObject
{
    Q_OBJECT
public:
    virtual ~Seat();

    std::string name() const;
    bool hasPointer() const;
    bool hasKeyboard() const;
    bool hasTouch() const;

    void setName(const std::string& name);
    void setHasPointer(bool has);
    void setHasKeyboard(bool has);
    void setHasTouch(bool has);

    void setTimestamp(quint32 time);
    quint32 timestamp() const;

    bool isDrag() const;
    bool isDragPointer() const;
    bool isDragTouch() const;
    QMatrix4x4 dragSurfaceTransformation() const;
    SurfaceInterface* dragSurface() const;
    PointerInterface* dragPointer() const;
    DataDeviceInterface* dragSource() const;
    void setDragTarget(SurfaceInterface* surface,
                       const QPointF& globalPosition,
                       const QMatrix4x4& inputTransformation);
    void setDragTarget(SurfaceInterface* surface,
                       const QMatrix4x4& inputTransformation = QMatrix4x4());

    void setPointerPos(const QPointF& pos);
    QPointF pointerPos() const;
    void setFocusedPointerSurface(SurfaceInterface* surface,
                                  const QPointF& surfacePosition = QPoint());
    void setFocusedPointerSurface(SurfaceInterface* surface, const QMatrix4x4& transformation);
    SurfaceInterface* focusedPointerSurface() const;
    PointerInterface* focusedPointer() const;
    void setFocusedPointerSurfacePosition(const QPointF& surfacePosition);
    QPointF focusedPointerSurfacePosition() const;
    void setFocusedPointerSurfaceTransformation(const QMatrix4x4& transformation);
    QMatrix4x4 focusedPointerSurfaceTransformation() const;
    void pointerButtonPressed(quint32 button);
    void pointerButtonPressed(Qt::MouseButton button);
    void pointerButtonReleased(quint32 button);
    void pointerButtonReleased(Qt::MouseButton button);
    bool isPointerButtonPressed(quint32 button) const;
    bool isPointerButtonPressed(Qt::MouseButton button) const;
    quint32 pointerButtonSerial(quint32 button) const;
    quint32 pointerButtonSerial(Qt::MouseButton button) const;
    void pointerAxisV5(Qt::Orientation orientation,
                       qreal delta,
                       qint32 discreteDelta,
                       PointerAxisSource source);
    void pointerAxis(Qt::Orientation orientation, quint32 delta);
    bool hasImplicitPointerGrab(quint32 serial) const;
    void relativePointerMotion(const QSizeF& delta,
                               const QSizeF& deltaNonAccelerated,
                               quint64 microseconds);
    void startPointerSwipeGesture(quint32 fingerCount);
    void updatePointerSwipeGesture(const QSizeF& delta);
    void endPointerSwipeGesture();
    void cancelPointerSwipeGesture();
    void startPointerPinchGesture(quint32 fingerCount);
    void updatePointerPinchGesture(const QSizeF& delta, qreal scale, qreal rotation);
    void endPointerPinchGesture();
    void cancelPointerPinchGesture();

    void setKeymap(int fd, quint32 size);
    void keyPressed(quint32 key);
    void keyReleased(quint32 key);
    void updateKeyboardModifiers(quint32 depressed, quint32 latched, quint32 locked, quint32 group);
    void setKeyRepeatInfo(qint32 charactersPerSecond, qint32 delay);
    quint32 depressedModifiers() const;
    quint32 latchedModifiers() const;
    quint32 lockedModifiers() const;
    quint32 groupModifiers() const;
    quint32 lastModifiersSerial() const;
    int keymapFileDescriptor() const;
    quint32 keymapSize() const;
    bool isKeymapXkbCompatible() const;
    QVector<quint32> pressedKeys() const;
    qint32 keyRepeatRate() const;
    qint32 keyRepeatDelay() const;
    void setFocusedKeyboardSurface(SurfaceInterface* surface);
    SurfaceInterface* focusedKeyboardSurface() const;
    KeyboardInterface* focusedKeyboard() const;

    void setFocusedTouchSurface(SurfaceInterface* surface,
                                const QPointF& surfacePosition = QPointF());
    SurfaceInterface* focusedTouchSurface() const;
    TouchInterface* focusedTouch() const;
    void setFocusedTouchSurfacePosition(const QPointF& surfacePosition);
    QPointF focusedTouchSurfacePosition() const;
    qint32 touchDown(const QPointF& globalPosition);
    void touchUp(qint32 id);
    void touchMove(qint32 id, const QPointF& globalPosition);
    void touchFrame();
    void cancelTouchSequence();
    bool isTouchSequence() const;
    bool hasImplicitTouchGrab(quint32 serial) const;

    void setFocusedTextInputSurface(SurfaceInterface* surface);
    SurfaceInterface* focusedTextInputSurface() const;
    TextInputInterface* focusedTextInput() const;

    DataDeviceInterface* selection() const;
    void setSelection(DataDeviceInterface* dataDevice);

    // legacy
    SeatInterface* legacy;
    static Seat* get(void* data);
    //

Q_SIGNALS:
    void nameChanged(const std::string&);
    void hasPointerChanged(bool);
    void hasKeyboardChanged(bool);
    void hasTouchChanged(bool);
    void pointerPosChanged(const QPointF& pos);
    void touchMoved(qint32 id, quint32 serial, const QPointF& globalPosition);
    void timestampChanged(quint32);

    void pointerCreated(Wrapland::Server::PointerInterface*);
    void keyboardCreated(Wrapland::Server::KeyboardInterface*);
    void touchCreated(Wrapland::Server::TouchInterface*);

    void focusedPointerChanged(Wrapland::Server::PointerInterface*);

    void selectionChanged(DataDeviceInterface*);
    void dragStarted();
    void dragEnded();
    void dragSurfaceChanged();
    void focusedTextInputChanged();

private:
    friend class D_isplay;

    // legacy
    friend class SeatInterface;
    friend class DataDeviceManagerInterface;
    friend class TextInputManagerUnstableV0Interface;
    friend class TextInputManagerUnstableV2Interface;

    Seat(D_isplay* display, QObject* parent);

    class Private;
    Private* d_ptr;
};

}
}

Q_DECLARE_METATYPE(Wrapland::Server::Seat*)
