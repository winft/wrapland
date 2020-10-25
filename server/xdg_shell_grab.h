#pragma once

#include <QList>
#include <QObject>
#include <Wrapland/Server/wraplandserver_export.h>

#include "grab.h"

namespace Wrapland::Server
{

class XdgShellPopup;

class WRAPLANDSERVER_EXPORT XdgPopupGrab : public Grab
{
    Q_OBJECT

private:
    QList<XdgShellPopup*> m_grabStack;

protected:
    void popupDestroyed();

public:
    XdgPopupGrab(QObject* parent = nullptr);
    ~XdgPopupGrab();
    XdgShellPopup* toplevelPopup();

    void grabPopup(XdgShellPopup* popup);
    void ungrabPopup(XdgShellPopup* popup);
    void handleGrabChanged();
};

}
