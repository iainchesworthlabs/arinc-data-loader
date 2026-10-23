#pragma once

// The words for what the 615A library reports as enumerations - how an operation ended, a status
// the target gave, how a transfer ended, a notice - in the operator's language. The library names
// them in English (for logs and tests); the application says them through the translators, as
// Messages so a language switch reaches the ones already shown.

#include "adl/app/message.hpp"
#include "adl/arinc615a/data_loader.hpp"

#include <QCoreApplication>
#include <cstdint>
#include <string_view>

namespace adl::app
{

class Wording
{
    Q_DECLARE_TR_FUNCTIONS(Wording)

public:
    // How an operation ended.
    [[nodiscard]] static Message outcome(adl::arinc615a::OutcomeCode code);
    // A status the target gave for an operation, a load or a file.
    [[nodiscard]] static Message status(adl::arinc615a::StatusCode code);
    // How a file transfer ended.
    [[nodiscard]] static Message transfer(adl::arinc615a::TransferOutcome outcome);
    // A notice of the engine as a line for the operator: what happened, to which file, and the
    // number it carries said in the unit it is in (seconds, files, a transfer's outcome).
    [[nodiscard]] static Message notice(adl::arinc615a::Notice notice, std::string_view subject, std::uint32_t value);

private:
    // The notice's own sentence, before the file and the number.
    [[nodiscard]] static Message notice_phrase(adl::arinc615a::Notice notice);
};

} // namespace adl::app
