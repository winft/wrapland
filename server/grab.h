/*
    SPDX-FileCopyrightText: 2020 Carson Black <uhhadd@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include <QHash>
#include <QObject>

#include "surface.h"
#include <Wrapland/Server/wraplandserver_export.h>

#include <optional>

namespace Wrapland::Server
{

class Seat;

using OptionalSurface = std::optional<Surface*>;

class WRAPLANDSERVER_EXPORT Grab : public QObject
{
    Q_OBJECT

private:
    Seat* m_seat = nullptr;

    void setSeat(Seat* seat)
    {
        m_seat = seat;
    }
    friend class GrabManager;

public:
    Seat* seat() const
    {
        return m_seat;
    }
    Grab(QObject* parent);

    Q_SIGNAL void wantedGrabChanged(OptionalSurface surf);
};

class WRAPLANDSERVER_EXPORT GrabManager : public QObject
{
    Q_OBJECT

private:
    Seat* m_seat;
    Surface* m_currentGrab;

    using PriorityGrab = std::tuple<Grab*, int>;

    QList<PriorityGrab> m_grabs;
    QHash<Grab*, OptionalSurface> m_currentRequests;
    Surface* computeCurrentGrab();

public:
    GrabManager(Seat* parent);

    Surface* currentGrab();
    void registerGrab(Grab* grab, int priority);
};

} // Wrapland::Server
