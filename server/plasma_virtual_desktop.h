/****************************************************************************
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
****************************************************************************/
#pragma once

// TODO(romangg): For registring uint32_t with the Qt meta object system. Remove when not needed
//                anymore.
#include "display.h"

#include <Wrapland/Server/wraplandserver_export.h>

#include <QObject>
#include <memory>

namespace Wrapland::Server
{

class Display;
class PlasmaVirtualDesktop;

class WRAPLANDSERVER_EXPORT PlasmaVirtualDesktopManager : public QObject
{
    Q_OBJECT
public:
    ~PlasmaVirtualDesktopManager() override;

    void setRows(uint32_t rows);

    PlasmaVirtualDesktop* desktop(const QString& id);
    QList<PlasmaVirtualDesktop*> desktops() const;

    PlasmaVirtualDesktop* createDesktop(const QString& id,
                                        uint32_t position = std::numeric_limits<uint32_t>::max());
    void removeDesktop(const QString& id);
    void sendDone();

Q_SIGNALS:
    void desktopActivated(const QString& id);
    void desktopCreateRequested(const QString& name, uint32_t position);
    void desktopRemoveRequested(const QString& id);

private:
    explicit PlasmaVirtualDesktopManager(Display* display, QObject* parent = nullptr);
    friend class Display;

    class Private;
    std::unique_ptr<Private> d_ptr;
};

class WRAPLANDSERVER_EXPORT PlasmaVirtualDesktop : public QObject
{
    Q_OBJECT
public:
    QString id() const;

    void setName(const QString& name);
    QString name() const;

    void setActive(bool active);
    bool active() const;

    void sendDone();

Q_SIGNALS:
    void activateRequested();

private:
    explicit PlasmaVirtualDesktop(PlasmaVirtualDesktopManager* parent);
    ~PlasmaVirtualDesktop() override;

    friend class PlasmaVirtualDesktopManager;

    class Private;
    const std::unique_ptr<Private> d_ptr;
};

}
