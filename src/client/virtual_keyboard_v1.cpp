/*
    SPDX-FileCopyrightText: 2021 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#include "virtual_keyboard_v1_p.h"

#include "event_queue.h"
#include "seat.h"
#include "surface.h"

namespace Wrapland::Client
{

virtual_keyboard_manager_v1::virtual_keyboard_manager_v1(QObject* parent)
    : QObject(parent)
    , d_ptr(new Private)
{
}

virtual_keyboard_manager_v1::~virtual_keyboard_manager_v1() = default;

void virtual_keyboard_manager_v1::setup(zwp_virtual_keyboard_manager_v1* manager)
{
    Q_ASSERT(manager);
    Q_ASSERT(!d_ptr->manager_ptr);
    d_ptr->manager_ptr.setup(manager);
}

void virtual_keyboard_manager_v1::release()
{
    d_ptr->release();
}

void virtual_keyboard_manager_v1::setEventQueue(EventQueue* queue)
{
    d_ptr->queue = queue;
}

EventQueue* virtual_keyboard_manager_v1::event_queue()
{
    return d_ptr->queue;
}

virtual_keyboard_manager_v1::operator zwp_virtual_keyboard_manager_v1*()
{
    return *(d_ptr.get());
}

virtual_keyboard_manager_v1::operator zwp_virtual_keyboard_manager_v1*() const
{
    return *(d_ptr.get());
}

bool virtual_keyboard_manager_v1::isValid() const
{
    return d_ptr->isValid();
}

virtual_keyboard_v1* virtual_keyboard_manager_v1::create_virtual_keyboard(Seat* seat,
                                                                          QObject* parent)
{
    return d_ptr->create_virtual_keyboard(seat, parent);
}

void virtual_keyboard_manager_v1::Private::release()
{
    manager_ptr.release();
}

bool virtual_keyboard_manager_v1::Private::isValid()
{
    return manager_ptr.isValid();
}

virtual_keyboard_v1* virtual_keyboard_manager_v1::Private::create_virtual_keyboard(Seat* seat,
                                                                                   QObject* parent)
{
    Q_ASSERT(isValid());
    auto ti = new virtual_keyboard_v1(seat, parent);
    auto wlti = zwp_virtual_keyboard_manager_v1_create_virtual_keyboard(manager_ptr, *seat);
    if (queue) {
        queue->addProxy(wlti);
    }
    ti->setup(wlti);
    return ti;
}

virtual_keyboard_v1::Private::Private(Seat* seat, virtual_keyboard_v1* q)
    : seat{seat}
    , q_ptr{q}
{
}

void virtual_keyboard_v1::Private::setup(zwp_virtual_keyboard_v1* virtual_keyboard)
{
    Q_ASSERT(virtual_keyboard);
    Q_ASSERT(!virtual_keyboard_ptr);
    virtual_keyboard_ptr.setup(virtual_keyboard);
}

bool virtual_keyboard_v1::Private::isValid() const
{
    return virtual_keyboard_ptr.isValid();
}

virtual_keyboard_v1::virtual_keyboard_v1(Seat* seat, QObject* parent)
    : QObject(parent)
    , d_ptr(new Private(seat, this))
{
}

virtual_keyboard_v1::~virtual_keyboard_v1()
{
    release();
}

void virtual_keyboard_v1::setEventQueue(EventQueue* queue)
{
    d_ptr->queue = queue;
}

EventQueue* virtual_keyboard_v1::event_queue() const
{
    return d_ptr->queue;
}

bool virtual_keyboard_v1::isValid() const
{
    return d_ptr->isValid();
}

void virtual_keyboard_v1::keymap(std::string const& content)
{
    auto tmpf = std::tmpfile();

    std::fputs(content.data(), tmpf);
    std::rewind(tmpf);

    zwp_virtual_keyboard_v1_keymap(d_ptr->virtual_keyboard_ptr,
                                   WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1,
                                   fileno(tmpf),
                                   content.size());

    d_ptr->keymap = file_wrap(tmpf);
}

void virtual_keyboard_v1::key(std::chrono::milliseconds time, uint32_t key, key_state state)
{
    zwp_virtual_keyboard_v1_key(d_ptr->virtual_keyboard_ptr,
                                time.count(),
                                key,
                                state == key_state::pressed ? WL_KEYBOARD_KEY_STATE_PRESSED
                                                            : WL_KEYBOARD_KEY_STATE_RELEASED);
}
void virtual_keyboard_v1::modifiers(uint32_t depressed,
                                    uint32_t latched,
                                    uint32_t locked,
                                    uint32_t group)
{
    zwp_virtual_keyboard_v1_modifiers(
        d_ptr->virtual_keyboard_ptr, depressed, latched, locked, group);
}

void virtual_keyboard_v1::setup(zwp_virtual_keyboard_v1* virtual_keyboard)
{
    d_ptr->setup(virtual_keyboard);
}

void virtual_keyboard_v1::release()
{
    d_ptr->virtual_keyboard_ptr.release();
}

virtual_keyboard_v1::operator zwp_virtual_keyboard_v1*()
{
    return d_ptr->virtual_keyboard_ptr;
}

virtual_keyboard_v1::operator zwp_virtual_keyboard_v1*() const
{
    return d_ptr->virtual_keyboard_ptr;
}

}
