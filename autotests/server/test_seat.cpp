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
#include "../../server/pointer.h"
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

static const QString s_socketName = QStringLiteral("kwin-wayland-server-seat-test-0");

void TestWaylandServerSeat::testCapabilities()
{
    D_isplay display;
    display.setSocketName(s_socketName);
    display.start();
    std::unique_ptr<Seat> seat{display.createSeat()};
    QVERIFY(!seat->hasKeyboard());
    QVERIFY(!seat->hasPointer());
    QVERIFY(!seat->hasTouch());

    QSignalSpy keyboardSpy(seat.get(), SIGNAL(hasKeyboardChanged(bool)));
    QVERIFY(keyboardSpy.isValid());
    seat->setHasKeyboard(true);
    QCOMPARE(keyboardSpy.count(), 1);
    QVERIFY(keyboardSpy.last().first().toBool());
    QVERIFY(seat->hasKeyboard());
    seat->setHasKeyboard(false);
    QCOMPARE(keyboardSpy.count(), 2);
    QVERIFY(!keyboardSpy.last().first().toBool());
    QVERIFY(!seat->hasKeyboard());
    seat->setHasKeyboard(false);
    QCOMPARE(keyboardSpy.count(), 2);

    QSignalSpy pointerSpy(seat.get(), SIGNAL(hasPointerChanged(bool)));
    QVERIFY(pointerSpy.isValid());
    seat->setHasPointer(true);
    QCOMPARE(pointerSpy.count(), 1);
    QVERIFY(pointerSpy.last().first().toBool());
    QVERIFY(seat->hasPointer());
    seat->setHasPointer(false);
    QCOMPARE(pointerSpy.count(), 2);
    QVERIFY(!pointerSpy.last().first().toBool());
    QVERIFY(!seat->hasPointer());
    seat->setHasPointer(false);
    QCOMPARE(pointerSpy.count(), 2);

    QSignalSpy touchSpy(seat.get(), SIGNAL(hasTouchChanged(bool)));
    QVERIFY(touchSpy.isValid());
    seat->setHasTouch(true);
    QCOMPARE(touchSpy.count(), 1);
    QVERIFY(touchSpy.last().first().toBool());
    QVERIFY(seat->hasTouch());
    seat->setHasTouch(false);
    QCOMPARE(touchSpy.count(), 2);
    QVERIFY(!touchSpy.last().first().toBool());
    QVERIFY(!seat->hasTouch());
    seat->setHasTouch(false);
    QCOMPARE(touchSpy.count(), 2);
}

void TestWaylandServerSeat::testName()
{
    D_isplay display;
    display.setSocketName(s_socketName);
    display.start();
    std::unique_ptr<Seat> seat{display.createSeat()};
    QCOMPARE(seat->name().size(), 0);

    const std::string name = "foobar";
    seat->setName(name);
    QCOMPARE(seat->name(), name);
}

void TestWaylandServerSeat::testPointerButton()
{
    D_isplay display;
    display.setSocketName(s_socketName);
    display.start();
    std::unique_ptr<Seat> seat{display.createSeat()};
    auto pointer = seat->focusedPointer();
    QVERIFY(!pointer);

    // no button pressed yet, should be released and no serial
    QVERIFY(!seat->isPointerButtonPressed(0));
    QVERIFY(!seat->isPointerButtonPressed(1));
    QCOMPARE(seat->pointerButtonSerial(0), quint32(0));
    QCOMPARE(seat->pointerButtonSerial(1), quint32(0));

    // mark the button as pressed
    seat->pointerButtonPressed(0);
    QVERIFY(seat->isPointerButtonPressed(0));
    QCOMPARE(seat->pointerButtonSerial(0), display.serial());

    // other button should still be unpressed
    QVERIFY(!seat->isPointerButtonPressed(1));
    QCOMPARE(seat->pointerButtonSerial(1), quint32(0));

    // release it again
    seat->pointerButtonReleased(0);
    QVERIFY(!seat->isPointerButtonPressed(0));
    QCOMPARE(seat->pointerButtonSerial(0), display.serial());
}

void TestWaylandServerSeat::testPointerPos()
{
    D_isplay display;
    display.setSocketName(s_socketName);
    display.start();

    std::unique_ptr<Seat> seat{display.createSeat()};
    QSignalSpy seatPosSpy(seat.get(), SIGNAL(pointerPosChanged(QPointF)));
    QVERIFY(seatPosSpy.isValid());
    auto pointer = seat->focusedPointer();
    QVERIFY(!pointer);

    QCOMPARE(seat->pointerPos(), QPointF());

    seat->setPointerPos(QPointF(10, 15));
    QCOMPARE(seat->pointerPos(), QPointF(10, 15));
    QCOMPARE(seatPosSpy.count(), 1);
    QCOMPARE(seatPosSpy.first().first().toPointF(), QPointF(10, 15));

    seat->setPointerPos(QPointF(10, 15));
    QCOMPARE(seatPosSpy.count(), 1);

    seat->setPointerPos(QPointF(5, 7));
    QCOMPARE(seat->pointerPos(), QPointF(5, 7));
    QCOMPARE(seatPosSpy.count(), 2);
    QCOMPARE(seatPosSpy.first().first().toPointF(), QPointF(10, 15));
    QCOMPARE(seatPosSpy.last().first().toPointF(), QPointF(5, 7));
}

void TestWaylandServerSeat::testDestroyThroughTerminate()
{
    D_isplay display;
    display.setSocketName(s_socketName);
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
    D_isplay display;
    display.setSocketName(s_socketName);
    display.start();

    std::unique_ptr<Seat> seat{display.createSeat()};
    QCOMPARE(seat->keyRepeatRate(), 0);
    QCOMPARE(seat->keyRepeatDelay(), 0);
    seat->setKeyRepeatInfo(25, 660);
    QCOMPARE(seat->keyRepeatRate(), 25);
    QCOMPARE(seat->keyRepeatDelay(), 660);
    // setting negative values should result in 0
    seat->setKeyRepeatInfo(-25, -660);
    QCOMPARE(seat->keyRepeatRate(), 0);
    QCOMPARE(seat->keyRepeatDelay(), 0);
}

void TestWaylandServerSeat::testMultiple()
{
    D_isplay display;
    display.setSocketName(s_socketName);
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
