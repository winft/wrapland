/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2021 Francesco Sorrentino <francesco.sorr@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#pragma once

#include <QMimeDatabase>
#include <QObject>
#include <QString>

namespace Wrapland::Client
{

template<typename PrivateDevice, typename WlOffer>
void offer_callback(void* data, WlOffer* dataOffer, const char* mimeType)
{
    auto d = reinterpret_cast<PrivateDevice*>(data);
    Q_ASSERT(d->dataOffer == dataOffer);
    auto mime = QString::fromUtf8(mimeType);

    QMimeDatabase db;
    const auto& m = db.mimeTypeForName(mimeType);
    if (m.isValid()) {
        d->mimeTypes << m;
        Q_EMIT d->q->mimeTypeOffered(m.name());
    }
}

}
