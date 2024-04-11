/****************************************************************************
Copyright 2020  Roman Gilg <subdiff@gmail.com>

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
****************************************************************************/
#include "presentation_time.h"
#include "event_queue.h"
#include "output.h"
#include "surface.h"
#include "wayland_pointer_p.h"

#include <wayland-presentation-time-client-protocol.h>

namespace Wrapland
{
namespace Client
{

class Q_DECL_HIDDEN PresentationManager::Private
{
public:
    Private(PresentationManager* q);

    void setup(wp_presentation* presentation);

    clockid_t clockId = 0;

    WaylandPointer<wp_presentation, wp_presentation_destroy> presentationManager;
    EventQueue* queue = nullptr;

    PresentationManager* q;

private:
    static void clockIdCallback(void* data, wp_presentation* presentation, uint32_t clk_id);

    static wp_presentation_listener const s_listener;
};

wp_presentation_listener const PresentationManager::Private::s_listener = {
    clockIdCallback,
};

void PresentationManager::Private::clockIdCallback(void* data,
                                                   wp_presentation* presentation,
                                                   uint32_t clk_id)
{
    auto p = reinterpret_cast<PresentationManager::Private*>(data);
    Q_ASSERT(p->presentationManager == presentation);
    p->clockId = clk_id;
    Q_EMIT p->q->clockIdChanged();
}

PresentationManager::Private::Private(PresentationManager* q)
    : q(q)
{
}

void PresentationManager::Private::setup(wp_presentation* presentation)
{
    Q_ASSERT(presentation);
    Q_ASSERT(!presentationManager);
    presentationManager.setup(presentation);
    wp_presentation_add_listener(presentationManager, &s_listener, this);
}

PresentationManager::PresentationManager(QObject* parent)
    : QObject(parent)
    , d(new Private(this))
{
}

PresentationManager::~PresentationManager()
{
    release();
}

void PresentationManager::setup(wp_presentation* presentation)
{
    d->setup(presentation);
}

void PresentationManager::release()
{
    d->presentationManager.release();
}

PresentationManager::operator wp_presentation*()
{
    return d->presentationManager;
}

PresentationManager::operator wp_presentation*() const
{
    return d->presentationManager;
}

bool PresentationManager::isValid() const
{
    return d->presentationManager.isValid();
}

void PresentationManager::setEventQueue(EventQueue* queue)
{
    d->queue = queue;
}

EventQueue* PresentationManager::eventQueue()
{
    return d->queue;
}

clockid_t PresentationManager::clockId() const
{
    return d->clockId;
}

PresentationFeedback* PresentationManager::createFeedback(Surface* surface, QObject* parent)
{
    Q_ASSERT(isValid());
    auto feedback = new PresentationFeedback(parent);
    auto proxy = wp_presentation_feedback(d->presentationManager, *surface);
    if (d->queue) {
        d->queue->addProxy(proxy);
    }
    feedback->setup(proxy);
    return feedback;
}

class PresentationFeedback::Private
{
public:
    Private(PresentationFeedback* q);

    Output* syncOutput = nullptr;

    uint32_t tvSecHi;
    uint32_t tvSecLo;
    uint32_t tvNsec;
    uint32_t refresh;
    uint32_t seqHi;
    uint32_t seqLo;
    Kinds flags;

    void setup(struct wp_presentation_feedback* feedback);

    WaylandPointer<struct wp_presentation_feedback, wp_presentation_feedback_destroy> feedback;

private:
    static void
    syncOutputCallback(void* data, struct wp_presentation_feedback* wlFeedback, wl_output* output);
    static void presentedCallback(void* data,
                                  struct wp_presentation_feedback* wlFeedback,
                                  uint32_t tv_sec_hi,
                                  uint32_t tv_sec_lo,
                                  uint32_t tv_nsec,
                                  uint32_t refresh,
                                  uint32_t seq_hi,
                                  uint32_t seq_lo,
                                  uint32_t flags);
    static void discardedCallback(void* data, struct wp_presentation_feedback* wlFeedback);

    PresentationFeedback* q;

    static wp_presentation_feedback_listener const s_listener;
};

wp_presentation_feedback_listener const PresentationFeedback::Private::s_listener = {
    syncOutputCallback,
    presentedCallback,
    discardedCallback,
};

void PresentationFeedback::Private::syncOutputCallback(void* data,
                                                       struct wp_presentation_feedback* wlFeedback,
                                                       wl_output* output)
{
    auto priv = reinterpret_cast<PresentationFeedback::Private*>(data);
    Q_ASSERT(priv->feedback == wlFeedback);
    priv->syncOutput = Output::get(output);
}

PresentationFeedback::Kinds toKinds(uint32_t flags)
{
    using Kind = PresentationFeedback::Kind;
    PresentationFeedback::Kinds ret = {};
    if (flags & 0x1) {
        ret |= Kind::Vsync;
    }
    if (flags & 0x2) {
        ret |= Kind::HwClock;
    }
    if (flags & 0x4) {
        ret |= Kind::HwCompletion;
    }
    if (flags & 0x8) {
        ret |= Kind::ZeroCopy;
    }
    return ret;
}

void PresentationFeedback::Private::presentedCallback(void* data,
                                                      struct wp_presentation_feedback* wlFeedback,
                                                      uint32_t tv_sec_hi,
                                                      uint32_t tv_sec_lo,
                                                      uint32_t tv_nsec,
                                                      uint32_t refresh,
                                                      uint32_t seq_hi,
                                                      uint32_t seq_lo,
                                                      uint32_t flags)
{
    auto p = reinterpret_cast<PresentationFeedback::Private*>(data);
    Q_ASSERT(p->feedback == wlFeedback);
    p->tvSecHi = tv_sec_hi;
    p->tvSecLo = tv_sec_lo;
    p->tvNsec = tv_nsec;
    p->refresh = refresh;
    p->seqHi = seq_hi;
    p->seqLo = seq_lo;
    p->flags = toKinds(flags);

    Q_EMIT p->q->presented();
}

void PresentationFeedback::Private::discardedCallback(void* data,
                                                      struct wp_presentation_feedback* wlFeedback)
{
    auto p = reinterpret_cast<PresentationFeedback::Private*>(data);
    Q_ASSERT(p->feedback == wlFeedback);

    Q_EMIT p->q->discarded();
}

PresentationFeedback::Private::Private(PresentationFeedback* q)
    : q(q)
{
}

void PresentationFeedback::Private::setup(struct wp_presentation_feedback* feedback)
{
    Q_ASSERT(feedback);
    Q_ASSERT(!this->feedback);
    this->feedback.setup(feedback);
    wp_presentation_feedback_add_listener(feedback, &s_listener, this);
}

PresentationFeedback::PresentationFeedback(QObject* parent)
    : QObject(parent)
    , d(new Private(this))
{
}

PresentationFeedback::~PresentationFeedback() = default;

void PresentationFeedback::setup(struct wp_presentation_feedback* feedback)
{
    d->setup(feedback);
}

bool PresentationFeedback::isValid() const
{
    return d->feedback.isValid();
}

Output* PresentationFeedback::syncOutput() const
{
    return d->syncOutput;
}

uint32_t PresentationFeedback::tvSecHi() const
{
    return d->tvSecHi;
}

uint32_t PresentationFeedback::tvSecLo() const
{
    return d->tvSecLo;
}

uint32_t PresentationFeedback::tvNsec() const
{
    return d->tvNsec;
}

uint32_t PresentationFeedback::refresh() const
{
    return d->refresh;
}

uint32_t PresentationFeedback::seqHi() const
{
    return d->seqHi;
}

uint32_t PresentationFeedback::seqLo() const
{
    return d->seqLo;
}

PresentationFeedback::Kinds PresentationFeedback::flags() const
{
    return d->flags;
}

}
}
