/********************************************************************
Copyright © 2014 Martin Gräßlin <mgraesslin@kde.org>
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
#include <QtTest>

#include "../../server/display.h"
#include "../../server/keyboard_pool.h"
#include "../../server/pointer_pool.h"
#include "../../server/seat.h"

using namespace Wrapland::Server;

class TestWaylandServerSeat : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void testCapabilities();
    void testName();
    void testPointerButton();
    void testPointerPos();
    void testDestroyThroughTerminate();
    void testRepeatInfo();
    void testMultiple();
};

constexpr auto socket_name{"kwin-wayland-server-seat-test-0"};

void TestWaylandServerSeat::testCapabilities()
{
    Display display;
    display.set_socket_name(socket_name);
    display.start();
    std::unique_ptr<Seat> seat{display.createSeat()};
    QVERIFY(!seat->hasKeyboard());
    QVERIFY(!seat->hasPointer());
    QVERIFY(!seat->hasTouch());

    seat->setHasKeyboard(true);
    QVERIFY(seat->hasKeyboard());
    seat->setHasKeyboard(false);
    QVERIFY(!seat->hasKeyboard());

    seat->setHasPointer(true);
    QVERIFY(seat->hasPointer());
    seat->setHasPointer(false);
    QVERIFY(!seat->hasPointer());

    seat->setHasTouch(true);
    QVERIFY(seat->hasTouch());
    seat->setHasTouch(false);
    QVERIFY(!seat->hasTouch());
}

void TestWaylandServerSeat::testName()
{
    Display display;
    display.set_socket_name(socket_name);
    display.start();
    std::unique_ptr<Seat> seat{display.createSeat()};
    QCOMPARE(seat->name().size(), 0);

    const std::string name = "foobar";
    seat->setName(name);
    QCOMPARE(seat->name(), name);
}

void TestWaylandServerSeat::testPointerButton()
{
    Display display;
    display.set_socket_name(socket_name);
    display.start();
    std::unique_ptr<Seat> seat{display.createSeat()};
    seat->setHasPointer(true);

    QVERIFY(seat->pointers().get_focus().devices.empty());

    // no button pressed yet, should be released and no serial
    QVERIFY(!seat->pointers().is_button_pressed(0));
    QVERIFY(!seat->pointers().is_button_pressed(1));
    QCOMPARE(seat->pointers().button_serial(0), quint32(0));
    QCOMPARE(seat->pointers().button_serial(1), quint32(0));

    // mark the button as pressed
    seat->pointers().button_pressed(0);
    QVERIFY(seat->pointers().is_button_pressed(0));
    QCOMPARE(seat->pointers().button_serial(0), display.serial());

    // other button should still be unpressed
    QVERIFY(!seat->pointers().is_button_pressed(1));
    QCOMPARE(seat->pointers().button_serial(1), quint32(0));

    // release it again
    seat->pointers().button_released(0);
    QVERIFY(!seat->pointers().is_button_pressed(0));
    QCOMPARE(seat->pointers().button_serial(0), display.serial());
}

void TestWaylandServerSeat::testPointerPos()
{
    Display display;
    display.set_socket_name(socket_name);
    display.start();

    std::unique_ptr<Seat> seat{display.createSeat()};
    QSignalSpy seatPosSpy(seat.get(), SIGNAL(pointerPosChanged(QPointF)));
    QVERIFY(seatPosSpy.isValid());
    seat->setHasPointer(true);

    QVERIFY(seat->pointers().get_focus().devices.empty());
    QCOMPARE(seat->pointers().get_position(), QPointF());

    seat->pointers().set_position(QPointF(10, 15));
    QCOMPARE(seat->pointers().get_position(), QPointF(10, 15));
    QCOMPARE(seatPosSpy.count(), 1);
    QCOMPARE(seatPosSpy.first().first().toPointF(), QPointF(10, 15));

    seat->pointers().set_position(QPointF(10, 15));
    QCOMPARE(seatPosSpy.count(), 1);

    seat->pointers().set_position(QPointF(5, 7));
    QCOMPARE(seat->pointers().get_position(), QPointF(5, 7));
    QCOMPARE(seatPosSpy.count(), 2);
    QCOMPARE(seatPosSpy.first().first().toPointF(), QPointF(10, 15));
    QCOMPARE(seatPosSpy.last().first().toPointF(), QPointF(5, 7));
}

void TestWaylandServerSeat::testDestroyThroughTerminate()
{
    Display display;
    display.set_socket_name(socket_name);
    display.start();

    std::unique_ptr<Seat> seat{display.createSeat()};
    QSignalSpy destroyedSpy(seat.get(), &QObject::destroyed);
    QVERIFY(destroyedSpy.isValid());
    display.terminate();
    QVERIFY(!destroyedSpy.wait(100));
    seat.reset();
    QCOMPARE(destroyedSpy.count(), 1);
}

void TestWaylandServerSeat::testRepeatInfo()
{
    Display display;
    display.set_socket_name(socket_name);
    display.start();

    std::unique_ptr<Seat> seat{display.createSeat()};
    seat->setHasKeyboard(true);
    auto& keyboards = seat->keyboards();

    QCOMPARE(keyboards.get_repeat_info().rate, 0);
    QCOMPARE(keyboards.get_repeat_info().delay, 0);
    keyboards.set_repeat_info(25, 660);
    QCOMPARE(keyboards.get_repeat_info().rate, 25);
    QCOMPARE(keyboards.get_repeat_info().delay, 660);

    // setting negative values should result in 0
    keyboards.set_repeat_info(-25, -660);
    QCOMPARE(keyboards.get_repeat_info().rate, 0);
    QCOMPARE(keyboards.get_repeat_info().delay, 0);
}

void TestWaylandServerSeat::testMultiple()
{
    Display display;
    display.set_socket_name(socket_name);
    display.start();
    QVERIFY(display.seats().empty());

    std::unique_ptr<Seat> seat1{display.createSeat()};
    QCOMPARE(display.seats().size(), 1);
    QCOMPARE(display.seats().at(0), seat1.get());

    std::unique_ptr<Seat> seat2{display.createSeat()};
    QCOMPARE(display.seats().size(), 2);
    QCOMPARE(display.seats().at(0), seat1.get());
    QCOMPARE(display.seats().at(1), seat2.get());

    std::unique_ptr<Seat> seat3{display.createSeat()};
    QCOMPARE(display.seats().size(), 3);
    QCOMPARE(display.seats().at(0), seat1.get());
    QCOMPARE(display.seats().at(1), seat2.get());
    QCOMPARE(display.seats().at(2), seat3.get());

    seat3.reset();
    QCOMPARE(display.seats().size(), 2);
    QCOMPARE(display.seats().at(0), seat1.get());
    QCOMPARE(display.seats().at(1), seat2.get());

    seat2.reset();
    QCOMPARE(display.seats().size(), 1);
    QCOMPARE(display.seats().at(0), seat1.get());

    seat1.reset();
    QCOMPARE(display.seats().size(), 0);
}

QTEST_GUILESS_MAIN(TestWaylandServerSeat)
#include "test_seat.moc"
