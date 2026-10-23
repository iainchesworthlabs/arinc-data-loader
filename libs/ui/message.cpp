// SPDX-License-Identifier: GPL-3.0-or-later
#include "adl/ui/message.hpp"

#include <cstddef>
#include <utility>

namespace adl::ui
{

Message Message::verbatim(QString text)
{
    Message message;
    message.kind_ = Kind::verbatim;
    message.verbatim_ = std::move(text);
    return message;
}

Message Message::translated(Translate translate, const char* source, std::vector<Message> arguments, int count)
{
    Message message;
    message.kind_ = Kind::translated;
    message.translate_ = translate;
    message.source_ = source;
    message.count_ = count;
    message.parts_ = std::move(arguments);
    return message;
}

Message Message::sequence(std::vector<Message> parts)
{
    Message message;
    message.kind_ = Kind::sequence;
    message.parts_ = std::move(parts);
    return message;
}

bool Message::isEmpty() const
{
    return kind_ == Kind::empty || (kind_ == Kind::verbatim && verbatim_.isEmpty()) ||
           (kind_ == Kind::sequence && text().isEmpty());
}

QString Message::text() const
{
    switch (kind_)
    {
    case Kind::empty:
        return {};
    case Kind::verbatim:
        return verbatim_;
    case Kind::sequence:
    {
        QString joined;
        for (const Message& part : parts_)
        {
            joined += part.text();
        }
        return joined;
    }
    case Kind::translated:
        break;
    }

    const QString phrase = translate_(source_, nullptr, count_);
    if (parts_.empty())
    {
        return phrase;
    }

    // %1 to %9 are replaced in one pass over the translated phrase, so an argument that itself
    // holds a "%1" is left alone, and a translation may use the arguments in any order or drop one
    // (QString::arg() would hand the next argument to the next lowest placeholder instead).
    QString result;
    result.reserve(phrase.size());
    for (qsizetype i = 0; i < phrase.size(); ++i)
    {
        const QChar character = phrase.at(i);
        if (character == QLatin1Char('%') && i + 1 < phrase.size())
        {
            const int digit = phrase.at(i + 1).digitValue();
            if (digit >= 1 && static_cast<std::size_t>(digit) <= parts_.size())
            {
                result += parts_[static_cast<std::size_t>(digit) - 1].text();
                ++i;
                continue;
            }
        }
        result += character;
    }
    return result;
}

} // namespace adl::ui
