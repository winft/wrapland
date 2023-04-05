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
#include "wlr_output_configuration_v1.h"

#include "event_queue.h"
#include "wayland_pointer_p.h"
#include "wlr_output_manager_v1.h"

#include "wayland-wlr-output-management-v1-client-protocol.h"

#include <memory>
#include <vector>

namespace Wrapland
{
namespace Client
{

struct ConfigurationHead {
    WlrOutputHeadV1* head = nullptr;
    zwlr_output_configuration_head_v1* native = nullptr;

    WlrOutputModeV1* mode = nullptr;

    struct {
        QSize size;
        int refresh = -1;
    } customMode;

    QPoint position;
    bool positionSet = false;
    WlrOutputHeadV1::Transform transform = WlrOutputHeadV1::Transform::Normal;
    bool transformSet = false;
    double scale = 1.;
    bool scaleSet = false;
    bool adapt_sync{false};
    bool adapt_sync_set{false};
};

class Q_DECL_HIDDEN WlrOutputConfigurationV1::Private
{
public:
    Private() = default;

    void setup(zwlr_output_configuration_v1* outputConfiguration);

    WaylandPointer<zwlr_output_configuration_v1, zwlr_output_configuration_v1_destroy>
        outputConfiguration;
    static const struct zwlr_output_configuration_v1_listener s_listener;
    EventQueue* queue = nullptr;

    void send();
    ConfigurationHead* getConfigurationHead(WlrOutputHeadV1* head);

    std::vector<std::unique_ptr<ConfigurationHead>> heads;
    WlrOutputConfigurationV1* q;

private:
    static void succeededCallback(void* data, zwlr_output_configuration_v1* config);
    static void failedCallback(void* data, zwlr_output_configuration_v1* config);
    static void cancelledCallback(void* data, zwlr_output_configuration_v1* config);
};

const zwlr_output_configuration_v1_listener WlrOutputConfigurationV1::Private::s_listener = {
    succeededCallback,
    failedCallback,
    cancelledCallback,
};

void WlrOutputConfigurationV1::Private::succeededCallback(void* data,
                                                          zwlr_output_configuration_v1* config)
{
    Q_UNUSED(config);
    auto priv = reinterpret_cast<WlrOutputConfigurationV1::Private*>(data);
    Q_EMIT priv->q->succeeded();
}

void WlrOutputConfigurationV1::Private::failedCallback(void* data,
                                                       zwlr_output_configuration_v1* config)
{
    Q_UNUSED(config);
    auto priv = reinterpret_cast<WlrOutputConfigurationV1::Private*>(data);
    Q_EMIT priv->q->failed();
}

void WlrOutputConfigurationV1::Private::cancelledCallback(void* data,
                                                          zwlr_output_configuration_v1* config)
{
    Q_UNUSED(config);
    auto priv = reinterpret_cast<WlrOutputConfigurationV1::Private*>(data);
    Q_EMIT priv->q->cancelled();
}

WlrOutputConfigurationV1::WlrOutputConfigurationV1(QObject* parent)
    : QObject(parent)
    , d(new Private)
{
    d->q = this;
}

WlrOutputConfigurationV1::~WlrOutputConfigurationV1()
{
    release();
}

void WlrOutputConfigurationV1::setup(zwlr_output_configuration_v1* outputConfiguration)
{
    Q_ASSERT(outputConfiguration);
    Q_ASSERT(!d->outputConfiguration);

    d->outputConfiguration.setup(outputConfiguration);
    d->setup(outputConfiguration);
}

void WlrOutputConfigurationV1::Private::setup(zwlr_output_configuration_v1* outputConfiguration)
{
    zwlr_output_configuration_v1_add_listener(outputConfiguration, &s_listener, this);
}

ConfigurationHead* WlrOutputConfigurationV1::Private::getConfigurationHead(WlrOutputHeadV1* head)
{
    for (auto& configurationHead : heads) {
        if (configurationHead->head == head) {
            return configurationHead.get();
        }
    }

    // Create a new configuration head struct and hand a reference back by calling this function
    // again.
    std::unique_ptr<ConfigurationHead> configurationHead(new ConfigurationHead);
    configurationHead->head = head;
    heads.push_back(std::move(configurationHead));

    return getConfigurationHead(head);
}

void WlrOutputConfigurationV1::Private::send()
{
    for (auto& head : heads) {
        if (!head->native) {
            continue;
        }

        if (head->mode) {
            zwlr_output_configuration_head_v1_set_mode(head->native, *head->mode);
        } else if (head->customMode.refresh >= 0 && head->customMode.size.isValid()) {
            zwlr_output_configuration_head_v1_set_custom_mode(head->native,
                                                              head->customMode.size.width(),
                                                              head->customMode.size.height(),
                                                              head->customMode.refresh);
        }

        if (head->positionSet) {
            zwlr_output_configuration_head_v1_set_position(
                head->native, head->position.x(), head->position.y());
        }

        if (head->transformSet) {
            auto toNative = [](WlrOutputHeadV1::Transform transform) {
                switch (transform) {
                case WlrOutputHeadV1::Transform::Normal:
                    return WL_OUTPUT_TRANSFORM_NORMAL;
                case WlrOutputHeadV1::Transform::Rotated90:
                    return WL_OUTPUT_TRANSFORM_90;
                case WlrOutputHeadV1::Transform::Rotated180:
                    return WL_OUTPUT_TRANSFORM_180;
                case WlrOutputHeadV1::Transform::Rotated270:
                    return WL_OUTPUT_TRANSFORM_270;
                case WlrOutputHeadV1::Transform::Flipped:
                    return WL_OUTPUT_TRANSFORM_FLIPPED;
                case WlrOutputHeadV1::Transform::Flipped90:
                    return WL_OUTPUT_TRANSFORM_FLIPPED_90;
                case WlrOutputHeadV1::Transform::Flipped180:
                    return WL_OUTPUT_TRANSFORM_FLIPPED_180;
                case WlrOutputHeadV1::Transform::Flipped270:
                    return WL_OUTPUT_TRANSFORM_FLIPPED_270;
                }
                abort();
            };
            zwlr_output_configuration_head_v1_set_transform(head->native,
                                                            toNative(head->transform));
        }

        if (head->scaleSet) {
            zwlr_output_configuration_head_v1_set_scale(head->native,
                                                        wl_fixed_from_double(head->scale));
        }

        if (head->adapt_sync_set) {
            zwlr_output_configuration_head_v1_set_adaptive_sync(
                head->native,
                head->adapt_sync ? ZWLR_OUTPUT_HEAD_V1_ADAPTIVE_SYNC_STATE_ENABLED
                                 : ZWLR_OUTPUT_HEAD_V1_ADAPTIVE_SYNC_STATE_DISABLED);
        }
    }
}

void WlrOutputConfigurationV1::release()
{
    d->outputConfiguration.release();
}

void WlrOutputConfigurationV1::setEventQueue(EventQueue* queue)
{
    d->queue = queue;
}

EventQueue* WlrOutputConfigurationV1::eventQueue()
{
    return d->queue;
}

WlrOutputConfigurationV1::operator zwlr_output_configuration_v1*()
{
    return d->outputConfiguration;
}

WlrOutputConfigurationV1::operator zwlr_output_configuration_v1*() const
{
    return d->outputConfiguration;
}

bool WlrOutputConfigurationV1::isValid() const
{
    return d->outputConfiguration.isValid();
}

void WlrOutputConfigurationV1::setEnabled(WlrOutputHeadV1* head, bool enable)
{
    auto configurationHead = d->getConfigurationHead(head);

    if (enable) {
        if (!configurationHead->native) {
            configurationHead->native
                = zwlr_output_configuration_v1_enable_head(d->outputConfiguration, *head);
        }
    } else {
        zwlr_output_configuration_v1_disable_head(d->outputConfiguration, *head);
    }
}

void WlrOutputConfigurationV1::setMode(WlrOutputHeadV1* head, WlrOutputModeV1* mode)
{
    d->getConfigurationHead(head)->mode = mode;
}

void WlrOutputConfigurationV1::setTransform(WlrOutputHeadV1* head,
                                            WlrOutputHeadV1::Transform transform)
{
    auto configurationHead = d->getConfigurationHead(head);

    configurationHead->transform = transform;
    configurationHead->transformSet = true;
}

void WlrOutputConfigurationV1::setPosition(WlrOutputHeadV1* head, QPoint const& pos)
{
    auto configurationHead = d->getConfigurationHead(head);

    configurationHead->position = pos;
    configurationHead->positionSet = true;
}

void WlrOutputConfigurationV1::setScale(WlrOutputHeadV1* head, double scale)
{
    auto configurationHead = d->getConfigurationHead(head);

    configurationHead->scale = scale;
    configurationHead->scaleSet = true;
}

void WlrOutputConfigurationV1::set_adaptive_sync(WlrOutputHeadV1* head, bool enable)
{
    auto configurationHead = d->getConfigurationHead(head);
    if (zwlr_output_configuration_head_v1_get_version(configurationHead->native)
        < ZWLR_OUTPUT_CONFIGURATION_HEAD_V1_SET_ADAPTIVE_SYNC_SINCE_VERSION) {
        return;
    }

    configurationHead->adapt_sync = enable;
    configurationHead->adapt_sync_set = true;
}

void WlrOutputConfigurationV1::test()
{
    d->send();
    zwlr_output_configuration_v1_test(d->outputConfiguration);
}

void WlrOutputConfigurationV1::apply()
{
    d->send();
    zwlr_output_configuration_v1_apply(d->outputConfiguration);
}

}
}
