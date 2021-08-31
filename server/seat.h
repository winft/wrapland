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

#include <wayland-server.h>

#include <cstdint>
#include <memory>
#include <vector>

namespace Wrapland::Server
{

class DataDevice;
class Display;
class input_method_v2;
class Keyboard;
class keyboard_pool;
class Pointer;
class pointer_pool;
class PrimarySelectionDevice;
class Surface;
class TextInputV2;
class text_input_v3;
class Touch;
class touch_pool;

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
    ~Seat() override;

    std::string name() const;
    bool hasPointer() const;
    bool hasKeyboard() const;
    bool hasTouch() const;

    void setName(const std::string& name);
    void setHasPointer(bool has);
    void setHasKeyboard(bool has);
    void setHasTouch(bool has);

    pointer_pool& pointers() const;
    keyboard_pool& keyboards() const;
    touch_pool& touches() const;

    void setTimestamp(uint32_t time);
    uint32_t timestamp() const;

    bool isDrag() const;
    bool isDragPointer() const;
    bool isDragTouch() const;
    QMatrix4x4 dragSurfaceTransformation() const;
    Surface* dragSurface() const;
    Pointer* dragPointer() const;
    DataDevice* dragSource() const;
    void setDragTarget(Surface* surface,
                       const QPointF& globalPosition,
                       const QMatrix4x4& inputTransformation);
    void setDragTarget(Surface* surface, const QMatrix4x4& inputTransformation = QMatrix4x4());

    void setFocusedKeyboardSurface(Surface* surface);

    input_method_v2* get_input_method_v2() const;

    void setFocusedTextInputSurface(Surface* surface);
    Surface* focusedTextInputSurface() const;
    TextInputV2* focusedTextInputV2() const;
    text_input_v3* focusedTextInputV3() const;

    DataDevice* selection() const;
    void setSelection(DataDevice* dataDevice);
    PrimarySelectionDevice* primarySelection() const;
    void setPrimarySelection(PrimarySelectionDevice* dataDevice);

Q_SIGNALS:
    void pointerPosChanged(const QPointF& pos);
    void touchMoved(int32_t id, uint32_t serial, const QPointF& globalPosition);
    void timestampChanged(uint32_t);

    void pointerCreated(Wrapland::Server::Pointer*);
    void keyboardCreated(Wrapland::Server::Keyboard*);
    void touchCreated(Wrapland::Server::Touch*);
    void input_method_v2_changed();

    void focusedPointerChanged(Wrapland::Server::Pointer*);

    void selectionChanged(DataDevice*);
    void primarySelectionChanged(PrimarySelectionDevice*);
    void dragStarted();
    void dragEnded();
    void dragSurfaceChanged();
    void focusedTextInputChanged();

private:
    friend class Display;
    friend class DataDeviceManager;
    friend class drag_pool;
    friend class keyboard_pool;
    friend class pointer_pool;
    friend class PrimarySelectionDeviceManager;
    friend class input_method_manager_v2;
    friend class TextInputManagerV2;
    friend class text_input_manager_v3;
    friend class text_input_pool;
    friend class touch_pool;

    Seat(Display* display, QObject* parent);

    // Returns whether an actual change took place.
    bool setFocusedTextInputV2Surface(Surface* surface);
    bool setFocusedTextInputV3Surface(Surface* surface);

    class Private;
    std::unique_ptr<Private> d_ptr;
};

}

Q_DECLARE_METATYPE(Wrapland::Server::Seat*)
