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
#include "datasource.h"
#include "selection_source_p.h"
#include "wayland_pointer_p.h"
// Qt
#include <QMimeType>
// Wayland
#include <wayland-client-protocol.h>

namespace Wrapland
{
namespace Client
{

class Q_DECL_HIDDEN DataSource::Private
{
public:
    explicit Private(DataSource* q);
    void setup(wl_data_source* s);

    WaylandPointer<wl_data_source, wl_data_source_destroy> source;
    DataDeviceManager::DnDAction selectedAction = DataDeviceManager::DnDAction::None;

    void setAction(DataDeviceManager::DnDAction action);
    static void targetCallback(void* data, wl_data_source* dataSource, char const* mimeType);
    static void dndDropPerformedCallback(void* data, wl_data_source* wl_data_source);
    static void dndFinishedCallback(void* data, wl_data_source* wl_data_source);
    static void actionCallback(void* data, wl_data_source* wl_data_source, uint32_t dnd_action);

    static const struct wl_data_source_listener s_listener;

    DataSource* q;
};

const wl_data_source_listener DataSource::Private::s_listener = {
    targetCallback,
    send_callback<Private>,
    cancelled_callback<Private>,
    dndDropPerformedCallback,
    dndFinishedCallback,
    actionCallback,
};

DataSource::Private::Private(DataSource* q)
    : q(q)
{
}

void DataSource::Private::targetCallback(void* data,
                                         wl_data_source* dataSource,
                                         char const* mimeType)
{
    auto d = reinterpret_cast<DataSource::Private*>(data);
    Q_ASSERT(d->source == dataSource);
    emit d->q->targetAccepts(QString::fromUtf8(mimeType));
}

void DataSource::Private::dndDropPerformedCallback(void* data, wl_data_source* wl_data_source)
{
    Q_UNUSED(wl_data_source)
    auto d = reinterpret_cast<DataSource::Private*>(data);
    emit d->q->dragAndDropPerformed();
}

void DataSource::Private::dndFinishedCallback(void* data, wl_data_source* wl_data_source)
{
    Q_UNUSED(wl_data_source)
    auto d = reinterpret_cast<DataSource::Private*>(data);
    emit d->q->dragAndDropFinished();
}

void DataSource::Private::actionCallback(void* data,
                                         wl_data_source* wl_data_source,
                                         uint32_t dnd_action)
{
    Q_UNUSED(wl_data_source)
    auto d = reinterpret_cast<Private*>(data);
    switch (dnd_action) {
    case WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY:
        d->setAction(DataDeviceManager::DnDAction::Copy);
        break;
    case WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE:
        d->setAction(DataDeviceManager::DnDAction::Move);
        break;
    case WL_DATA_DEVICE_MANAGER_DND_ACTION_ASK:
        d->setAction(DataDeviceManager::DnDAction::Ask);
        break;
    case WL_DATA_DEVICE_MANAGER_DND_ACTION_NONE:
        d->setAction(DataDeviceManager::DnDAction::None);
        break;
    default:
        Q_UNREACHABLE();
    }
}

void DataSource::Private::setAction(DataDeviceManager::DnDAction action)
{
    if (action == selectedAction) {
        return;
    }
    selectedAction = action;
    emit q->selectedDragAndDropActionChanged();
}

void DataSource::Private::setup(wl_data_source* s)
{
    Q_ASSERT(!source.isValid());
    Q_ASSERT(s);
    source.setup(s);
    wl_data_source_add_listener(s, &s_listener, this);
}

DataSource::DataSource(QObject* parent)
    : QObject(parent)
    , d(new Private(this))
{
}

DataSource::~DataSource()
{
    release();
}

void DataSource::release()
{
    d->source.release();
}

bool DataSource::isValid() const
{
    return d->source.isValid();
}

void DataSource::setup(wl_data_source* dataSource)
{
    d->setup(dataSource);
}

void DataSource::offer(QString const& mimeType)
{
    wl_data_source_offer(d->source, mimeType.toUtf8().constData());
}

void DataSource::offer(QMimeType const& mimeType)
{
    if (!mimeType.isValid()) {
        return;
    }
    offer(mimeType.name());
}

DataSource::operator wl_data_source*() const
{
    return d->source;
}

DataSource::operator wl_data_source*()
{
    return d->source;
}

void DataSource::setDragAndDropActions(DataDeviceManager::DnDActions actions)
{
    uint32_t wlActions = WL_DATA_DEVICE_MANAGER_DND_ACTION_NONE;
    if (actions.testFlag(DataDeviceManager::DnDAction::Copy)) {
        wlActions |= WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY;
    }
    if (actions.testFlag(DataDeviceManager::DnDAction::Move)) {
        wlActions |= WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE;
    }
    if (actions.testFlag(DataDeviceManager::DnDAction::Ask)) {
        wlActions |= WL_DATA_DEVICE_MANAGER_DND_ACTION_ASK;
    }
    wl_data_source_set_actions(d->source, wlActions);
}

DataDeviceManager::DnDAction DataSource::selectedDragAndDropAction() const
{
    return d->selectedAction;
}

}
}
