/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2021 Francesco Sorrentino <francesco.sorr@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#pragma once

#include <QObject>

namespace Wrapland::Client
{

template<typename PrivateDevice, typename WlDevice, typename WlOffer>
void data_offer_callback(void* data, WlDevice* device, WlOffer* id)
{
    auto priv = reinterpret_cast<PrivateDevice*>(data);
    Q_ASSERT(priv->device == device);

    Q_ASSERT(!priv->lastOffer);
    using Offer = typename std::remove_pointer_t<decltype(priv->q)>::offer_type;
    priv->lastOffer = new Offer(priv->q, id);
    Q_ASSERT(priv->lastOffer->isValid());
}

template<typename PrivateDevice, typename WlDevice, typename WlOffer>
void selection_callback(void* data, WlDevice* device, WlOffer* id)
{
    auto priv = reinterpret_cast<PrivateDevice*>(data);
    Q_ASSERT(priv->device == device);

    if (!id) {
        priv->selectionOffer.reset();
        Q_EMIT priv->q->selectionOffered(nullptr);
        return;
    }
    Q_ASSERT(*priv->lastOffer == id);
    priv->selectionOffer.reset(priv->lastOffer);
    priv->lastOffer = nullptr;
    Q_EMIT priv->q->selectionOffered(priv->selectionOffer.get());
}

}
