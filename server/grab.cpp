/*
    SPDX-FileCopyrightText: 2020 Carson Black <uhhadd@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "grab.h"
#include "seat.h"

namespace Wrapland::Server
{

Grab::Grab(QObject* parent)
    : QObject(parent)
{
}

GrabManager::GrabManager(Seat* parent)
    : QObject(parent)
    , m_seat(parent)
{
}

Surface* GrabManager::computeCurrentGrab()
{
    for (auto [grab, _] : m_grabs) {
        if (m_currentRequests.value(grab).has_value()) {
            return m_currentRequests.value(grab).value();
        }
    }

    return nullptr;
}

Surface* GrabManager::currentGrab()
{
    return m_currentGrab;
}

void GrabManager::registerGrab(Grab* grab, int priority)
{
    grab->setSeat(m_seat);
    m_grabs << PriorityGrab(grab, priority);

    // Sort grabs so that we iterate over the ones with higher priority before the
    // ones with lower priority.
    std::sort(m_grabs.begin(), m_grabs.end(), [=](PriorityGrab& lhs, PriorityGrab& rhs) {
        auto [_, lhsPriority] = lhs;
        auto [__, rhsPriority] = rhs;

        return lhsPriority > rhsPriority;
    });

    connect(grab, &Grab::wantedGrabChanged, [=](OptionalSurface surf) {
        m_currentRequests[grab] = surf;

        auto newGrab = computeCurrentGrab();
        if (newGrab != m_currentGrab) {
            m_currentGrab = newGrab;

            m_seat->setKeyboardGrab(surf);
            m_seat->setPointerGrab(surf);
            m_seat->setTouchGrab(surf);
        }
    });
}

} // Wrapland::Server
