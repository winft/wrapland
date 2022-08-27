/****************************************************************************
 * Copyright © 2020 Roman Gilg <subdiff@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) version 3, or any
 * later version accepted by the membership of KDE e.V. (or its
 * successor approved by the membership of KDE e.V.), which shall
 * act as a proxy defined in Section 6 of version 3 of the license.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library.  If not, see <http://www.gnu.org/licenses/>.
 ****************************************************************************/
#include "wlr_output_manager_v1.h"

#include "event_queue.h"
#include "wayland_pointer_p.h"
#include "wlr_output_configuration_v1.h"

#include "wayland-wlr-output-management-v1-client-protocol.h"

#include <algorithm>
#include <vector>

namespace Wrapland
{
namespace Client
{

class Q_DECL_HIDDEN WlrOutputManagerV1::Private
{
public:
    explicit Private(WlrOutputManagerV1* q)
        : q(q)
    {
    }
    ~Private() = default;

    void setup(zwlr_output_manager_v1* manager);

    WaylandPointer<zwlr_output_manager_v1, zwlr_output_manager_v1_destroy> outputManager;
    EventQueue* queue = nullptr;

    static void headCallback(void* data, zwlr_output_manager_v1* manager, zwlr_output_head_v1* id);
    static void doneCallback(void* data, zwlr_output_manager_v1* manager, quint32 serial);
    static void finishedCallback(void* data, zwlr_output_manager_v1* manager);

    WlrOutputManagerV1* q;
    static const struct zwlr_output_manager_v1_listener s_listener;

    quint32 serial;
};

const zwlr_output_manager_v1_listener WlrOutputManagerV1::Private::s_listener = {
    headCallback,
    doneCallback,
    finishedCallback,
};

void WlrOutputManagerV1::Private::headCallback(void* data,
                                               zwlr_output_manager_v1* manager,
                                               zwlr_output_head_v1* id)
{
    auto d = reinterpret_cast<Private*>(data);
    Q_ASSERT(d->outputManager == manager);

    auto head = new WlrOutputHeadV1(id, d->q);
    Q_EMIT d->q->head(head);
}

void WlrOutputManagerV1::Private::doneCallback(void* data,
                                               zwlr_output_manager_v1* manager,
                                               quint32 serial)
{
    auto d = reinterpret_cast<Private*>(data);
    Q_ASSERT(d->outputManager == manager);

    d->serial = serial;
    Q_EMIT d->q->done();
}

void WlrOutputManagerV1::Private::finishedCallback(void* data, zwlr_output_manager_v1* manager)
{
    auto d = reinterpret_cast<Private*>(data);
    Q_ASSERT(d->outputManager == manager);

    Q_EMIT d->q->removed();
}

void WlrOutputManagerV1::Private::setup(zwlr_output_manager_v1* manager)
{
    Q_ASSERT(manager);
    Q_ASSERT(!outputManager);

    outputManager.setup(manager);
    zwlr_output_manager_v1_add_listener(manager, &s_listener, this);
}

WlrOutputManagerV1::WlrOutputManagerV1(QObject* parent)
    : QObject(parent)
    , d(new Private(this))
{
}

WlrOutputManagerV1::~WlrOutputManagerV1()
{
    d->outputManager.release();
}

void WlrOutputManagerV1::setup(zwlr_output_manager_v1* outputManager)
{
    d->setup(outputManager);
}

void WlrOutputManagerV1::release()
{
    d->outputManager.release();
}

void WlrOutputManagerV1::setEventQueue(EventQueue* queue)
{
    d->queue = queue;
}

EventQueue* WlrOutputManagerV1::eventQueue()
{
    return d->queue;
}

WlrOutputManagerV1::operator zwlr_output_manager_v1*()
{
    return d->outputManager;
}

WlrOutputManagerV1::operator zwlr_output_manager_v1*() const
{
    return d->outputManager;
}

bool WlrOutputManagerV1::isValid() const
{
    return d->outputManager.isValid();
}

WlrOutputConfigurationV1* WlrOutputManagerV1::createConfiguration(QObject* parent)
{
    Q_UNUSED(parent);
    auto* config = new WlrOutputConfigurationV1(this);
    auto w = zwlr_output_manager_v1_create_configuration(d->outputManager, d->serial);

    if (d->queue) {
        d->queue->addProxy(w);
    }

    config->setup(w);
    return config;
}

class Q_DECL_HIDDEN WlrOutputModeV1::Private
{
public:
    Private(WlrOutputModeV1* q, zwlr_output_mode_v1* mode);
    ~Private() = default;

    WaylandPointer<zwlr_output_mode_v1, zwlr_output_mode_v1_destroy> outputMode;
    EventQueue* queue = nullptr;

    void name(QString const& name);

    static void sizeCallback(void* data, zwlr_output_mode_v1* mode, int width, int height);
    static void refreshCallback(void* data, zwlr_output_mode_v1* mode, int refresh);
    static void preferredCallback(void* data, zwlr_output_mode_v1* mode);
    static void finishedCallback(void* data, zwlr_output_mode_v1* mode);

    WlrOutputModeV1* q;
    static const struct zwlr_output_mode_v1_listener s_listener;

    QSize size;
    int refresh;
    bool preferred = false;
};

const zwlr_output_mode_v1_listener WlrOutputModeV1::Private::s_listener = {
    sizeCallback,
    refreshCallback,
    preferredCallback,
    finishedCallback,
};

void WlrOutputModeV1::Private::sizeCallback(void* data,
                                            zwlr_output_mode_v1* mode,
                                            int width,
                                            int height)
{
    auto d = reinterpret_cast<Private*>(data);
    Q_ASSERT(d->outputMode == mode);

    d->size = QSize(width, height);
}

void WlrOutputModeV1::Private::refreshCallback(void* data, zwlr_output_mode_v1* mode, int refresh)
{
    auto d = reinterpret_cast<Private*>(data);
    Q_ASSERT(d->outputMode == mode);

    d->refresh = refresh;
}

void WlrOutputModeV1::Private::preferredCallback(void* data, zwlr_output_mode_v1* mode)
{
    auto d = reinterpret_cast<Private*>(data);
    Q_ASSERT(d->outputMode == mode);

    d->preferred = true;
}

void WlrOutputModeV1::Private::finishedCallback(void* data, zwlr_output_mode_v1* mode)
{
    auto d = reinterpret_cast<Private*>(data);
    Q_ASSERT(d->outputMode == mode);

    Q_EMIT d->q->removed();
}

WlrOutputModeV1::Private::Private(WlrOutputModeV1* q, zwlr_output_mode_v1* mode)
    : q(q)
{
    outputMode.setup(mode);
    zwlr_output_mode_v1_add_listener(outputMode, &s_listener, this);
}

WlrOutputModeV1::WlrOutputModeV1(zwlr_output_mode_v1* mode, QObject* parent)
    : QObject(parent)
    , d(new Private(this, mode))
{
}

WlrOutputModeV1::~WlrOutputModeV1() = default;

QSize WlrOutputModeV1::size() const
{
    return d->size;
}

int WlrOutputModeV1::refresh() const
{
    return d->refresh;
}

bool WlrOutputModeV1::preferred() const
{
    return d->preferred;
}

class Q_DECL_HIDDEN WlrOutputHeadV1::Private
{
public:
    Private(WlrOutputHeadV1* q, zwlr_output_head_v1* head);
    ~Private() = default;

    WaylandPointer<zwlr_output_head_v1, zwlr_output_head_v1_destroy> outputHead;
    EventQueue* queue = nullptr;

    static void nameCallback(void* data, zwlr_output_head_v1* head, char const* name);
    static void descriptionCallback(void* data, zwlr_output_head_v1* head, char const* description);
    static void physicalSizeCallback(void* data, zwlr_output_head_v1* head, int width, int height);
    static void modeCallback(void* data, zwlr_output_head_v1* head, zwlr_output_mode_v1* mode);
    static void enabledCallback(void* data, zwlr_output_head_v1* head, int enabled);
    static void
    currentModeCallback(void* data, zwlr_output_head_v1* head, zwlr_output_mode_v1* mode);
    static void positionCallback(void* data, zwlr_output_head_v1* head, int x, int y);
    static void transformCallback(void* data, zwlr_output_head_v1* head, int transform);
    static void scaleCallback(void* data, zwlr_output_head_v1* head, wl_fixed_t scale);
    static void finishedCallback(void* data, zwlr_output_head_v1* head);

    static void makeCallback(void* data, zwlr_output_head_v1* head, char const* make);
    static void modelCallback(void* data, zwlr_output_head_v1* head, char const* model);
    static void
    serialNumberCallback(void* data, zwlr_output_head_v1* head, char const* serialNumber);

    WlrOutputHeadV1* q;
    static const struct zwlr_output_head_v1_listener s_listener;

    QString name;
    QString description;
    QSize physicalSize;
    QPoint position;
    Transform transform{Transform::Normal};
    bool enabled{false};
    double scale{1.};

    QString make;
    QString model;
    QString serialNumber;

    std::vector<std::unique_ptr<WlrOutputModeV1>> modes;
    WlrOutputModeV1* currentMode{nullptr};

private:
    WlrOutputModeV1* getMode(zwlr_output_mode_v1* mode) const;
};

const zwlr_output_head_v1_listener WlrOutputHeadV1::Private::s_listener = {
    nameCallback,
    descriptionCallback,
    physicalSizeCallback,
    modeCallback,
    enabledCallback,
    currentModeCallback,
    positionCallback,
    transformCallback,
    scaleCallback,
    finishedCallback,
    makeCallback,
    modelCallback,
    serialNumberCallback,
};

void WlrOutputHeadV1::Private::nameCallback(void* data, zwlr_output_head_v1* head, char const* name)
{
    auto d = reinterpret_cast<Private*>(data);
    Q_ASSERT(d->outputHead == head);

    d->name = name;
    Q_EMIT d->q->changed();
}

void WlrOutputHeadV1::Private::descriptionCallback(void* data,
                                                   zwlr_output_head_v1* head,
                                                   char const* description)
{
    auto d = reinterpret_cast<Private*>(data);
    Q_ASSERT(d->outputHead == head);

    d->description = description;
    Q_EMIT d->q->changed();
}

void WlrOutputHeadV1::Private::physicalSizeCallback(void* data,
                                                    zwlr_output_head_v1* head,
                                                    int width,
                                                    int height)
{
    auto d = reinterpret_cast<Private*>(data);
    Q_ASSERT(d->outputHead == head);

    d->physicalSize = QSize(width, height);
    Q_EMIT d->q->changed();
}

void WlrOutputHeadV1::Private::modeCallback(void* data,
                                            zwlr_output_head_v1* head,
                                            zwlr_output_mode_v1* mode)
{
    auto d = reinterpret_cast<Private*>(data);
    Q_ASSERT(d->outputHead == head);

    auto mode_wrapper = new WlrOutputModeV1(mode, d->q);
    connect(mode_wrapper, &WlrOutputModeV1::removed, d->q, [d, mode_wrapper] {
        auto it = std::find_if(
            d->modes.begin(), d->modes.end(), [mode_wrapper](auto const& stored_mode) {
                return mode_wrapper == stored_mode.get();
            });
        assert(it != d->modes.end());
        d->modes.erase(it);
        if (mode_wrapper == d->currentMode) {
            d->currentMode = nullptr;
            Q_EMIT d->q->changed();
        }
    });
    d->modes.push_back(std::unique_ptr<WlrOutputModeV1>{mode_wrapper});

    Q_EMIT d->q->changed();
}

void WlrOutputHeadV1::Private::enabledCallback(void* data, zwlr_output_head_v1* head, int enabled)
{
    auto d = reinterpret_cast<Private*>(data);
    Q_ASSERT(d->outputHead == head);

    d->enabled = enabled;
    Q_EMIT d->q->changed();
}

void WlrOutputHeadV1::Private::currentModeCallback(void* data,
                                                   zwlr_output_head_v1* head,
                                                   zwlr_output_mode_v1* mode)
{
    auto d = reinterpret_cast<Private*>(data);
    Q_ASSERT(d->outputHead == head);

    d->currentMode = d->getMode(mode);
    Q_EMIT d->q->changed();
}

WlrOutputModeV1* WlrOutputHeadV1::Private::getMode(zwlr_output_mode_v1* mode) const
{
    for (auto const& modeWrapper : modes) {
        auto raw_ptr = modeWrapper.get();
        if (mode == *raw_ptr) {
            return raw_ptr;
        }
    }
    return nullptr;
}

WlrOutputModeV1::operator zwlr_output_mode_v1*()
{
    return d->outputMode;
}

WlrOutputModeV1::operator zwlr_output_mode_v1*() const
{
    return d->outputMode;
}

void WlrOutputHeadV1::Private::positionCallback(void* data, zwlr_output_head_v1* head, int x, int y)
{
    auto d = reinterpret_cast<Private*>(data);
    Q_ASSERT(d->outputHead == head);

    d->position = QPoint(x, y);
    Q_EMIT d->q->changed();
}

void WlrOutputHeadV1::Private::transformCallback(void* data,
                                                 zwlr_output_head_v1* head,
                                                 int transform)
{
    auto d = reinterpret_cast<Private*>(data);
    Q_ASSERT(d->outputHead == head);

    auto toTransform = [transform]() {
        switch (transform) {
        case WL_OUTPUT_TRANSFORM_90:
            return Transform::Rotated90;
        case WL_OUTPUT_TRANSFORM_180:
            return Transform::Rotated180;
        case WL_OUTPUT_TRANSFORM_270:
            return Transform::Rotated270;
        case WL_OUTPUT_TRANSFORM_FLIPPED:
            return Transform::Flipped;
        case WL_OUTPUT_TRANSFORM_FLIPPED_90:
            return Transform::Flipped90;
        case WL_OUTPUT_TRANSFORM_FLIPPED_180:
            return Transform::Flipped180;
        case WL_OUTPUT_TRANSFORM_FLIPPED_270:
            return Transform::Flipped270;
        case WL_OUTPUT_TRANSFORM_NORMAL:
        default:
            return Transform::Normal;
        }
    };
    d->transform = toTransform();
    Q_EMIT d->q->changed();
}

void WlrOutputHeadV1::Private::scaleCallback(void* data,
                                             zwlr_output_head_v1* head,
                                             wl_fixed_t scale)
{
    auto d = reinterpret_cast<Private*>(data);
    Q_ASSERT(d->outputHead == head);

    d->scale = wl_fixed_to_double(scale);
    Q_EMIT d->q->changed();
}

void WlrOutputHeadV1::Private::finishedCallback(void* data, zwlr_output_head_v1* head)
{
    auto d = reinterpret_cast<Private*>(data);
    Q_ASSERT(d->outputHead == head);

    Q_EMIT d->q->removed();
    delete d->q;
}

void WlrOutputHeadV1::Private::makeCallback(void* data, zwlr_output_head_v1* head, char const* make)
{
    auto d = reinterpret_cast<Private*>(data);
    Q_ASSERT(d->outputHead == head);

    d->make = make;
    Q_EMIT d->q->changed();
}

void WlrOutputHeadV1::Private::modelCallback(void* data,
                                             zwlr_output_head_v1* head,
                                             char const* model)
{
    auto d = reinterpret_cast<Private*>(data);
    Q_ASSERT(d->outputHead == head);

    d->model = model;
    Q_EMIT d->q->changed();
}

void WlrOutputHeadV1::Private::serialNumberCallback(void* data,
                                                    zwlr_output_head_v1* head,
                                                    char const* serialNumber)
{
    auto d = reinterpret_cast<Private*>(data);
    Q_ASSERT(d->outputHead == head);

    d->serialNumber = serialNumber;
    Q_EMIT d->q->changed();
}

WlrOutputHeadV1::Private::Private(WlrOutputHeadV1* q, zwlr_output_head_v1* head)
    : q(q)
{
    outputHead.setup(head);
    zwlr_output_head_v1_add_listener(outputHead, &s_listener, this);
}

WlrOutputHeadV1::WlrOutputHeadV1(zwlr_output_head_v1* head, QObject* parent)
    : QObject(parent)
    , d(new Private(this, head))
{
}

WlrOutputHeadV1::~WlrOutputHeadV1() = default;

QString WlrOutputHeadV1::name() const
{
    return d->name;
}

QString WlrOutputHeadV1::description() const
{
    return d->description;
}

QString WlrOutputHeadV1::make() const
{
    return d->make;
}

QString WlrOutputHeadV1::model() const
{
    return d->model;
}

QString WlrOutputHeadV1::serialNumber() const
{
    return d->serialNumber;
}

QSize WlrOutputHeadV1::physicalSize() const
{
    return d->physicalSize;
}

QPoint WlrOutputHeadV1::position() const
{
    return d->position;
}

WlrOutputHeadV1::Transform WlrOutputHeadV1::transform() const
{
    return d->transform;
}

bool WlrOutputHeadV1::enabled() const
{
    return d->enabled;
}

double WlrOutputHeadV1::scale() const
{
    return d->scale;
}

QVector<WlrOutputModeV1*> WlrOutputHeadV1::modes() const
{
    QVector<WlrOutputModeV1*> ret;
    for (auto const& mode : d->modes) {
        ret.append(mode.get());
    }
    return ret;
}

WlrOutputModeV1* WlrOutputHeadV1::currentMode() const
{
    return d->currentMode;
}

WlrOutputHeadV1::operator zwlr_output_head_v1*()
{
    return d->outputHead;
}

WlrOutputHeadV1::operator zwlr_output_head_v1*() const
{
    return d->outputHead;
}

}
}
