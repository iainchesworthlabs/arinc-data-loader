// SPDX-License-Identifier: GPL-3.0-or-later
#include "adl/app/download_directory.hpp"

#include "adl/app/text.hpp"
#include "adl/loader_core/download_directory.hpp"

#include <QCoreApplication>

namespace adl::app
{

namespace
{

using adl::loader_core::DirectoryError;
using adl::platform::ErrorCode;

// What is said of a directory that cannot be made. The platform's error code is its own word and
// is not translated.
class DirectoryProblem
{
    Q_DECLARE_TR_FUNCTIONS(DirectoryProblem)

public:
    static Message not_a_directory(std::string_view path)
    {
        return Message::translated(&DirectoryProblem::tr, QT_TR_NOOP("%1 is not a folder"),
                                   {Message::verbatim(to_qstring(path))});
    }

    static Message cannot_look_at(std::string_view path, ErrorCode code)
    {
        return Message::translated(&DirectoryProblem::tr, QT_TR_NOOP("cannot look at %1 (%2)"),
                                   {Message::verbatim(to_qstring(path)), error_code(code)});
    }

    static Message cannot_create(std::string_view path, ErrorCode code)
    {
        return Message::translated(&DirectoryProblem::tr, QT_TR_NOOP("cannot create %1 (%2)"),
                                   {Message::verbatim(to_qstring(path)), error_code(code)});
    }

    static Message too_many(std::string_view thw_id_pos, std::string_view directory)
    {
        return Message::translated(
            &DirectoryProblem::tr, QT_TR_NOOP("there are already %1 download folders for %2 in %3"),
            {Message::verbatim(QString::number(adl::loader_core::max_download_directories)),
             Message::verbatim(to_qstring(thw_id_pos)), Message::verbatim(to_qstring(directory))});
    }

    static Message error_code(ErrorCode code)
    {
        return Message::verbatim(QString::fromLatin1(adl::platform::to_string(code)));
    }
};

Message message_of(const DirectoryError& error)
{
    switch (error.kind)
    {
    case DirectoryError::Kind::not_a_directory:
        return DirectoryProblem::not_a_directory(error.path);
    case DirectoryError::Kind::cannot_look_at:
        return DirectoryProblem::cannot_look_at(error.path, error.code);
    case DirectoryError::Kind::cannot_create:
        return DirectoryProblem::cannot_create(error.path, error.code);
    case DirectoryError::Kind::too_many:
        return DirectoryProblem::too_many(error.thw_id_pos, error.path);
    }
    return {};
}

} // namespace

std::expected<void, Message> make_directory_chain(adl::platform::FileSystem& files, std::string_view path)
{
    const auto made = adl::loader_core::make_directory_chain(files, path);
    if (!made)
    {
        return std::unexpected(message_of(made.error()));
    }
    return {};
}

std::expected<std::string, Message> make_download_directory(adl::platform::FileSystem& files,
                                                            std::string_view directory, std::string_view thw_id_pos)
{
    auto made = adl::loader_core::make_download_directory(files, directory, thw_id_pos);
    if (!made)
    {
        return std::unexpected(message_of(made.error()));
    }
    return std::move(*made);
}

} // namespace adl::app
