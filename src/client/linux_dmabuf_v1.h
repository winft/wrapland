/********************************************************************
Copyright 2020  Faveraux Adrien <ad1rie3@hotmail.fr>

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
#include "buffer.h"

#include <QObject>
#include <QPoint>
#include <QSize>

// STD
#include <memory>

#include <Wrapland/Client/wraplandclient_export.h>

struct wl_buffer;
struct zwp_linux_dmabuf_v1;
struct zwp_linux_buffer_params_v1;

namespace Wrapland::Client
{

class EventQueue;
class ParamsV1;

class WRAPLANDCLIENT_EXPORT LinuxDmabufV1 : public QObject
{
    Q_OBJECT
public:
    struct Modifier {
        uint32_t modifier_hi;
        uint32_t modifier_lo;
    };

    explicit LinuxDmabufV1(QObject* parent = nullptr);
    ~LinuxDmabufV1() override;

    bool isValid() const;
    void setup(zwp_linux_dmabuf_v1* dmabuf);
    void release();
    void setEventQueue(EventQueue* queue);
    EventQueue* eventQueue();

    ParamsV1* createParamsV1(QObject* parent = nullptr);
    QHash<uint32_t, Modifier> supportedFormats();

    operator zwp_linux_dmabuf_v1*();
    operator zwp_linux_dmabuf_v1*() const;

Q_SIGNALS:
    void removed();
    void supportedFormatsChanged();

private:
    class Private;
    std::unique_ptr<Private> d_ptr;
};

class WRAPLANDCLIENT_EXPORT ParamsV1 : public QObject
{
    Q_OBJECT
public:
    ~ParamsV1() override;

    void setup(zwp_linux_buffer_params_v1* params);
    void release();
    bool isValid() const;

    void addDmabuf(int32_t fd,
                   uint32_t plane_idx,
                   uint32_t offset,
                   uint32_t stride,
                   uint32_t modifier_hi,
                   uint32_t modifier_lo);
    void createDmabuf(int32_t width, int32_t height, uint32_t format, uint32_t flags);
    wl_buffer*
    createDmabufImmediate(int32_t width, int32_t height, uint32_t format, uint32_t flags);
    wl_buffer* getBuffer();

    operator zwp_linux_buffer_params_v1*();
    operator zwp_linux_buffer_params_v1*() const;

Q_SIGNALS:
    void createSuccess(wl_buffer* buffer);
    void createFail();

private:
    friend class LinuxDmabufV1;
    explicit ParamsV1(QObject* parent = nullptr);
    class Private;
    std::unique_ptr<Private> d_ptr;
};

}
