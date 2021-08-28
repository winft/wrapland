/****************************************************************************
Copyright © 2020 Adrien Faveraux <ad1rie3@hotmail.fr>

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

#include <QObject>

#include <Wrapland/Server/wraplandserver_export.h>

#include <memory>

namespace Wrapland::Server
{
class Display;
class Client;
class Seat;
class Surface;

class WRAPLANDSERVER_EXPORT TextInputV2 : public QObject
{
    Q_OBJECT
public:
    enum class ContentHint : uint32_t {
        None = 0,
        AutoCompletion = 1 << 0,
        AutoCorrection = 1 << 1,
        AutoCapitalization = 1 << 2,
        LowerCase = 1 << 3,
        UpperCase = 1 << 4,
        TitleCase = 1 << 5,
        HiddenText = 1 << 6,
        SensitiveData = 1 << 7,
        Latin = 1 << 8,
        MultiLine = 1 << 9,
    };
    Q_DECLARE_FLAGS(ContentHints, ContentHint)

    enum class ContentPurpose : uint32_t {
        Normal,
        Alpha,
        Digits,
        Number,
        Phone,
        Url,
        Email,
        Name,
        Password,
        Date,
        Time,
        DateTime,
        Terminal,
    };

    Client* client() const;
    QByteArray preferredLanguage() const;
    QRect cursorRectangle() const;
    ContentPurpose contentPurpose() const;
    ContentHints contentHints() const;
    QByteArray surroundingText() const;
    qint32 surroundingTextCursorPosition() const;
    qint32 surroundingTextSelectionAnchor() const;
    QPointer<Surface> surface() const;
    bool isEnabled() const;

    void preEdit(const QByteArray& text, const QByteArray& commitText);
    void commit(const QByteArray& text);
    void setPreEditCursor(qint32 index);
    void deleteSurroundingText(quint32 beforeLength, quint32 afterLength);
    void setCursorPosition(qint32 index, qint32 anchor);
    void setTextDirection(Qt::LayoutDirection direction);

    void keysymPressed(quint32 keysym, Qt::KeyboardModifiers modifiers = Qt::NoModifier);
    void keysymReleased(quint32 keysym, Qt::KeyboardModifiers modifiers = Qt::NoModifier);
    void setInputPanelState(bool visible, const QRect& overlappedSurfaceArea);
    void setLanguage(const QByteArray& languageTag);

Q_SIGNALS:
    void requestShowInputPanel();
    void requestHideInputPanel();
    void requestReset();
    void preferredLanguageChanged(const QByteArray& language);
    void cursorRectangleChanged(const QRect& rect);
    void contentTypeChanged();
    void surroundingTextChanged();
    void enabledChanged();
    void resourceDestroyed();

private:
    explicit TextInputV2(Client* client, uint32_t version, uint32_t id);
    friend class TextInputManagerV2;
    friend class Seat;
    friend class text_input_pool;

    class Private;
    Private* d_ptr;
};

class WRAPLANDSERVER_EXPORT TextInputManagerV2 : public QObject
{
    Q_OBJECT
public:
    explicit TextInputManagerV2(Display* display, QObject* parent = nullptr);
    ~TextInputManagerV2() override;

private:
    class Private;
    std::unique_ptr<Private> d_ptr;
};

}

Q_DECLARE_METATYPE(Wrapland::Server::TextInputV2*)
Q_DECLARE_METATYPE(Wrapland::Server::TextInputV2::ContentHint)
Q_DECLARE_METATYPE(Wrapland::Server::TextInputV2::ContentHints)
Q_DECLARE_OPERATORS_FOR_FLAGS(Wrapland::Server::TextInputV2::ContentHints)
Q_DECLARE_METATYPE(Wrapland::Server::TextInputV2::ContentPurpose)
