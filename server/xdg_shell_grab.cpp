#include "xdg_shell_grab.h"
#include "xdg_shell_popup.h"
#include "xdg_shell_surface.h"
#include "xdg_shell_surface_p.h"

#include <wayland-server.h>
#include <wayland-xdg-shell-server-protocol.h>

namespace Wrapland::Server
{

XdgPopupGrab::XdgPopupGrab(QObject* parent)
    : Grab(parent)
{
}
XdgPopupGrab::~XdgPopupGrab()
{
}

XdgShellPopup* XdgPopupGrab::toplevelPopup()
{
    return m_grabStack.last();
}

void XdgPopupGrab::handleGrabChanged()
{
    auto surf = m_grabStack.last();
    if (surf) {
        Q_EMIT wantedGrabChanged(OptionalSurface(surf->surface()->surface()));
        return;
    }
    Q_EMIT wantedGrabChanged(OptionalSurface());
}

void XdgPopupGrab::popupDestroyed()
{
    // Use a reinterpret_cast instead of a qobject_cast because sender() is
    // currently self-destructing. In this case it's safe because we only
    // care about the numeric value of the pointer and the associated type
    // and not any of the functionality of the type.
    m_grabStack.removeAll(reinterpret_cast<XdgShellPopup*>(sender()));
    handleGrabChanged();
}

void XdgPopupGrab::grabPopup(XdgShellPopup* popup)
{
    if (m_grabStack.length() == 0) {
        // TODO: check parent
        m_grabStack << popup;
        handleGrabChanged();
        connect(popup, &QObject::destroyed, this, &XdgPopupGrab::popupDestroyed);
    } else {
        if (m_grabStack.last()->surface() == popup->parentSurface()) {
            m_grabStack << popup;
            handleGrabChanged();
            connect(popup, &QObject::destroyed, this, &XdgPopupGrab::popupDestroyed);
        } else {
            popup->surface()->d_ptr->postError(XDG_POPUP_ERROR_INVALID_GRAB,
                                               "cannot grab; parent popup does not have grab");
            return;
        }
    }
}

void XdgPopupGrab::ungrabPopup(XdgShellPopup* popup)
{
    if (!(m_grabStack.length() > 0)) {
        qCritical() << "Tried to ungrab a popup with an empty grab stack";
    }
    if (!(m_grabStack.last() == popup)) {
        qCritical() << "Tried to ungrab a popup that wasn't the topmost popup";
    }
    disconnect(popup, &QObject::destroyed, this, &XdgPopupGrab::popupDestroyed);
    m_grabStack.removeAll(popup);
}

}
