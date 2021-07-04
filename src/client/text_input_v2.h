/*
    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2021 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only
*/
#pragma once

#include <QObject>

#include <Wrapland/Client/wraplandclient_export.h>
#include <memory>

struct zwp_text_input_manager_v2;
struct zwp_text_input_v2;

namespace Wrapland::Client
{
class EventQueue;
class Seat;
class Surface;
class TextInputV2;

/**
 * @short Wrapper for the zwp_text_input_manager_v2 interface.
 *
 * This class provides a convenient wrapper for the zwp_text_input_manager_v2 interface.
 *
 * To use this class one needs to interact with the Registry. There are two
 * possible ways to create the TextInputManagerV2 interface:
 * @code
 * auto c = registry->createTextInputManagerV2(name, version);
 * @endcode
 *
 * This creates the TextInputManagerV2 and sets it up directly. As an alternative this
 * can also be done in a more low level way:
 * @code
 * auto c = new TextInputManagerV2;
 * c->setup(registry->bindTextInputManagerV2(name, version));
 * @endcode
 *
 * The TextInputManagerV2 can be used as a drop-in replacement for any zwp_text_input_manager_v2
 * pointer as it provides matching cast operators.
 *
 * @see Registry
 * @since 0.523.0
 **/
class WRAPLANDCLIENT_EXPORT TextInputManagerV2 : public QObject
{
    Q_OBJECT
public:
    explicit TextInputManagerV2(QObject* parent = nullptr);
    virtual ~TextInputManagerV2();

    /**
     * Setup this TextInputManagerV2 to manage the @p manager.
     * When using Registry::createTextInputManagerV2 there is no need to call this
     * method.
     **/
    void setup(zwp_text_input_manager_v2* manager);
    /**
     * @returns @c true if managing a resource.
     **/
    bool isValid() const;
    /**
     * Releases the interface.
     * After the interface has been released the TextInputManagerV2 instance is no
     * longer valid and can be setup with another interface.
     **/
    void release();

    /**
     * Sets the @p queue to use for creating objects with this TextInputManagerV2.
     **/
    void setEventQueue(EventQueue* queue);
    /**
     * @returns The event queue to use for creating objects with this TextInputManagerV2.
     **/
    EventQueue* eventQueue();

    /**
     * Creates a TextInput for the @p seat.
     *
     * @param seat The Seat to create the TextInput for
     * @param parent The parent to use for the TextInput
     **/
    TextInputV2* createTextInput(Seat* seat, QObject* parent = nullptr);

    /**
     * @returns @c null if not for a zwp_text_input_manager_v2
     **/
    operator zwp_text_input_manager_v2*();
    /**
     * @returns @c null if not for a zwp_text_input_manager_v2
     **/
    operator zwp_text_input_manager_v2*() const;

Q_SIGNALS:
    /**
     * The corresponding global for this interface on the Registry got removed.
     *
     * This signal gets only emitted if the TextInputManagerV2 got created by
     * Registry::createTextInputManagerV2
     **/
    void removed();

private:
    class Private;

    std::unique_ptr<Private> d_ptr;
};

/**
 * @short Wrapper for the zwp_text_input_v2 interface.
 *
 * This class is a convenient wrapper for the zwp_text_input_v2 interface.
 * To create a TextInputV2 call TextInputManagerV2::createTextInput.
 *
 * The main purpose of this class is to setup the next frame which
 * should be rendered. Therefore it provides methods to add damage
 * and to attach a new Buffer and to finalize the frame by calling
 * commit.
 *
 * @see TextInputManagerV2
 * @since 0.523.0
 **/
class WRAPLANDCLIENT_EXPORT TextInputV2 : public QObject
{
    Q_OBJECT
public:
    virtual ~TextInputV2();

    /**
     * Setup this TextInputV2 to manage the @p text_input.
     * When using TextInputManagerUnstableV2::createTextInput there is no need to call
     * this method.
     **/
    void setup(zwp_text_input_v2* text_input);
    /**
     * Releases the zwp_text_input_v2 interface.
     * After the interface has been released the TextInputV2 instance is no
     * longer valid and can be setup with another zwp_text_input_v2 interface.
     **/
    void release();
    /**
     * @returns @c true if managing a resource.
     **/
    bool isValid() const;

    operator zwp_text_input_v2*();
    operator zwp_text_input_v2*() const;

    /**
     * @returns The Surface which has the text input focus on this TextInputV2.
     * @see entered
     * @see left
     **/
    Surface* enteredSurface() const;

    void setEventQueue(EventQueue* queue);
    EventQueue* eventQueue() const;

    /**
     * @returns whether the input panel (virtual keyboard) is currently visible on the screen
     * @see inputPanelStateChanged
     **/
    bool isInputPanelVisible() const;

    /**
     * Enable text input in a @p surface (usually when a text entry inside of it has focus).
     *
     * This can be called before or after a surface gets text (or keyboard) focus via the
     * enter event. Text input to a surface is only active when it has the current
     * text (or keyboard) focus and is enabled.
     * @see deactivate
     **/
    void enable(Surface* surface);

    /**
     * Disable text input in a @p surface (typically when there is no focus on any
     * text entry inside the surface).
     * @see enable
     **/
    void disable(Surface* surface);

    /**
     * Requests input panels (virtual keyboard) to show.
     * @see hideInputPanel
     **/
    void showInputPanel();

    /**
     * Requests input panels (virtual keyboard) to hide.
     * @see showInputPanel
     **/
    void hideInputPanel();

    /**
     * Should be called by an editor widget when the input state should be
     * reset, for example after the text was changed outside of the normal
     * input method flow.
     **/
    void reset();

    /**
     * Sets the plain surrounding text around the input position.
     *
     * @param text The text surrounding the cursor position
     * @param cursor Index in the text describing the cursor position
     * @param anchor Index of the selection anchor, if no selection same as cursor
     **/
    void setSurroundingText(const QString& text, quint32 cursor, quint32 anchor);

    /**
     * The possible states for a keyEvent.
     * @see keyEvent
     **/
    enum class KeyState {
        Pressed,
        Released,
    };

    /**
     * ContentHint allows to modify the behavior of the text input.
     **/
    enum class ContentHint : uint32_t {
        /**
         * no special behaviour
         */
        None = 0,
        /**
         * suggest word completions
         */
        AutoCompletion = 1 << 0,
        /**
         * suggest word corrections
         */
        AutoCorrection = 1 << 1,
        /**
         * switch to uppercase letters at the start of a sentence
         */
        AutoCapitalization = 1 << 2,
        /**
         * prefer lowercase letters
         */
        LowerCase = 1 << 3,
        /**
         * prefer uppercase letters
         */
        UpperCase = 1 << 4,
        /**
         * prefer casing for titles and headings (can be language dependent)
         */
        TitleCase = 1 << 5,
        /**
         * characters should be hidden
         */
        HiddenText = 1 << 6,
        /**
         * typed text should not be stored
         */
        SensitiveData = 1 << 7,
        /**
         * just latin characters should be entered
         */
        Latin = 1 << 8,
        /**
         * the text input is multi line
         */
        MultiLine = 1 << 9,
    };
    Q_DECLARE_FLAGS(ContentHints, ContentHint)

    /**
     * The ContentPurpose allows to specify the primary purpose of a text input.
     *
     * This allows an input method to show special purpose input panels with
     * extra characters or to disallow some characters.
     */
    enum class ContentPurpose : uint32_t {
        /**
         * default input, allowing all characters
         */
        Normal,
        /**
         * allow only alphabetic characters
         **/
        Alpha,
        /**
         * allow only digits
         */
        Digits,
        /**
         * input a number (including decimal separator and sign)
         */
        Number,
        /**
         * input a phone number
         */
        Phone,
        /**
         * input an URL
         */
        Url,
        /**
         * input an email address
         **/
        Email,
        /**
         * input a name of a person
         */
        Name,
        /**
         * input a password
         */
        Password,
        /**
         * input a date
         */
        Date,
        /**
         * input a time
         */
        Time,
        /**
         * input a date and time
         */
        DateTime,
        /**
         * input for a terminal
         */
        Terminal,
    };
    /**
     * Sets the content @p purpose and content @p hints.
     * While the @p purpose is the basic purpose of an input field, the @p hints flags allow
     * to modify some of the behavior.
     **/
    void setContentType(ContentHints hints, ContentPurpose purpose);

    /**
     * Sets the cursor outline @p rect in surface local coordinates.
     *
     * Allows the compositor to e.g. put a window with word suggestions
     * near the cursor.
     **/
    void setCursorRectangle(const QRect& rect);

    /**
     * Sets a specific @p language.
     *
     * This allows for example a virtual keyboard to show a language specific layout.
     * The @p language argument is a RFC-3066 format language tag.
     **/
    void setPreferredLanguage(const QString& language);

    /**
     * The text direction of input text.
     *
     * It is mainly needed for showing input cursor on correct side of the
     * editor when there is no input yet done and making sure neutral
     * direction text is laid out properly.
     * @see textDirectionChnaged
     **/
    Qt::LayoutDirection textDirection() const;

    /**
     * The language of the input text.
     *
     * As long as the server has not emitted the language, the code will be empty.
     *
     * @returns a RFC-3066 format language tag in utf-8.
     * @see languageChanged
     **/
    QByteArray language() const;

    /**
     * The cursor position inside the {@link composingText} (as byte offset) relative
     * to the start of the {@link composingText}.
     * If index is a negative number no cursor is shown.
     * @see composingText
     * @see composingTextChanged
     **/
    qint32 composingTextCursorPosition() const;

    /**
     * The currently being composed text around the {@link composingTextCursorPosition}.
     * @see composingTextCursorPosition
     * @see composingTextChanged
     **/
    QByteArray composingText() const;

    /**
     * The fallback text can be used to replace the {@link composingText} in some cases
     * (for example when losing focus).
     *
     * @see composingText
     * @see composingTextChanged
     **/
    QByteArray composingFallbackText() const;

    /**
     * The commit text to be inserted.
     *
     * The commit text might be empty if only text should be deleted or the cursor be moved.
     * @see cursorPosition
     * @see anchorPosition
     * @see deleteSurroundingText
     * @see committed
     **/
    QByteArray commitText() const;

    /**
     * The cursor position in bytes at which the {@link commitText} should be inserted.
     * @see committed
     **/
    qint32 cursorPosition() const;

    /**
     * The text between anchorPosition and {@link cursorPosition} should be selected.
     * @see cursorPosition
     * @see committed
     **/
    qint32 anchorPosition() const;

    /**
     * Holds the length before and after the cursor position to be deleted.
     **/
    struct DeleteSurroundingText {
        quint32 beforeLength;
        quint32 afterLength;
    };
    /**
     * @returns The length in bytes which should be deleted around the cursor position
     * @see committed
     **/
    DeleteSurroundingText deleteSurroundingText() const;

Q_SIGNALS:
    /**
     * Emitted whenever a Surface is focused on this TextInputV2.
     * @see enteredSurface
     * @see left
     **/
    void entered();
    /**
     * Emitted whenever a Surface loses the focus on this TextInputV2.
     * @see enteredSurface
     * @see entered
     **/
    void left();
    /**
     * Emitted whenever the state of the input panel (virtual keyboard changes).
     * @see isInputPanelVisible
     **/
    void inputPanelStateChanged();
    /**
     * Emitted whenver the text direction changes.
     * @see textDirection
     **/
    void textDirectionChanged();
    /**
     * Emitted whenever the language changes.
     * @see language
     **/
    void languageChanged();

    /**
     * Emitted when a key event was sent.
     * Key events are not used for normal text input operations, but for specific key symbols
     * which are not composable through text.
     *
     * @param xkbKeySym The XKB key symbol, not a key code
     * @param state Whether the event represents a press or release event
     * @param modifiers The hold modifiers on this event
     * @param time Timestamp of this event
     **/
    void keyEvent(quint32 xkbKeySym,
                  Wrapland::Client::TextInputV2::KeyState state,
                  Qt::KeyboardModifiers modifiers,
                  quint32 time);

    /**
     * Emitted whenever the composing text and related states changed.
     * @see composingText
     * @see composingTextCursorPosition
     * @see composingFallbackText
     **/
    void composingTextChanged();

    /**
     * Emitted when the currently composing text got committed.
     * The {@link commitText} should get inserted at the {@link cursorPosition} and
     * the text around {@link deleteSurroundingText} should be deleted.
     *
     * @see commitText
     * @see cursorPosition
     * @see anchorPosition
     * @see deleteSurroundingText
     **/
    void committed();

private:
    explicit TextInputV2(Seat* seat, QObject* parent = nullptr);
    friend class TextInputManagerV2;

    class Private;
    std::unique_ptr<Private> d_ptr;
};

}

Q_DECLARE_METATYPE(Wrapland::Client::TextInputV2::KeyState)
Q_DECLARE_METATYPE(Wrapland::Client::TextInputV2::ContentHint)
Q_DECLARE_METATYPE(Wrapland::Client::TextInputV2::ContentPurpose)
Q_DECLARE_METATYPE(Wrapland::Client::TextInputV2::ContentHints)
Q_DECLARE_OPERATORS_FOR_FLAGS(Wrapland::Client::TextInputV2::ContentHints)
