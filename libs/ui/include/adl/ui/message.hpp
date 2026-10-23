// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// A phrase that is translated when it is read, not when it is made.
//
// The engine's thread makes text (a log line, how a job ended) and the window shows it for as long
// as it is on screen; the operator may change the language in between. A QString made with tr()
// at the moment of the event is in the language of that moment for good. A Message keeps the
// untranslated source, its count and its arguments, and text() translates them again each time it
// is asked, so the models only have to announce that their text changed (retranslate()) for the
// window to follow.
//
// A translated message is made inside a member function of a class that has tr() (a QObject, or
// Q_DECLARE_TR_FUNCTIONS), passing that class's own tr and the source wrapped in QT_TR_NOOP, which
// is what lupdate reads:
//
//     Message::translated(&EngineWorker::tr, QT_TR_NOOP("Reading the media set in %1..."),
//                         {Message::verbatim(path)});
//
// A count goes in as `count`, with `%n` and the "(s)" convention in the source (see the Qt
// documentation of plural forms). Arguments are other Messages, so a phrase can hold a translated
// phrase (a number of loads inside a sentence) as well as text that is never translated (a path, a
// file name, a hardware identifier, a word of the standard).

#include <QMetaType>
#include <QString>
#include <vector>

namespace adl::ui
{

class Message
{
public:
    // The tr() of the class the message is made in.
    using Translate = QString (*)(const char* source, const char* disambiguation, int count);

    Message() = default;

    // Text that is shown as it is, in every language.
    [[nodiscard]] static Message verbatim(QString text);

    // `source` translated by `translate`, then %1, %2... replaced by the arguments' text. `count`
    // (when not negative) picks the plural form and stands in for %n.
    [[nodiscard]] static Message translated(Translate translate, const char* source,
                                            std::vector<Message> arguments = {}, int count = -1);

    // The parts, one after the other.
    [[nodiscard]] static Message sequence(std::vector<Message> parts);

    // The text in the language installed now.
    [[nodiscard]] QString text() const;

    [[nodiscard]] bool isEmpty() const;

private:
    enum class Kind
    {
        empty,
        verbatim,
        translated,
        sequence
    };

    Kind kind_ = Kind::empty;
    QString verbatim_;
    Translate translate_ = nullptr;
    const char* source_ = nullptr;
    int count_ = -1;
    // The arguments of a translated message, or the parts of a sequence.
    std::vector<Message> parts_;
};

} // namespace adl::ui

Q_DECLARE_METATYPE(adl::ui::Message)
