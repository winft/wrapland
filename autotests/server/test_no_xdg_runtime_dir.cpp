/********************************************************************
Copyright 2016  Martin Gräßlin <mgraesslin@kde.org>

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

class NoXdgRuntimeDirTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void testCreate();
};

void NoXdgRuntimeDirTest::initTestCase()
{
    qunsetenv("XDG_RUNTIME_DIR");
}

constexpr auto socket_name{"wrapland-test-no-xdg-runtime-dir-0"};

void NoXdgRuntimeDirTest::testCreate()
{
    // this test verifies that not having an XDG_RUNTIME_DIR is handled gracefully
    // the server cannot start, but should not crash
    Wrapland::Server::Display display;

    display.set_socket_name(socket_name);
    QVERIFY(!display.running());
    display.start();
    QVERIFY(!display.running());

    // call into dispatchEvents should not crash
    display.dispatchEvents();
}

QTEST_GUILESS_MAIN(NoXdgRuntimeDirTest)
#include "test_no_xdg_runtime_dir.moc"
