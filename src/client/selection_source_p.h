/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2021 Francesco Sorrentino <francesco.sorr@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#pragma once

#include <QObject>

namespace Wrapland::Client
{

template<typename PrivateSource, typename WlSource>
void send_callback(void* data, WlSource* dataSource, const char* mimeType, int32_t fd)
{
    auto d = reinterpret_cast<PrivateSource*>(data);
    Q_ASSERT(d->source == dataSource);
    Q_EMIT d->q->sendDataRequested(QString::fromUtf8(mimeType), fd);
}

template<typename PrivateSource, typename WlSource>
void cancelled_callback(void* data, WlSource* dataSource)
{
    auto d = reinterpret_cast<PrivateSource*>(data);
    Q_ASSERT(d->source == dataSource);
    Q_EMIT d->q->cancelled();
}

}
