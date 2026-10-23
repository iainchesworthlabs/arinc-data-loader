#include "adl/app/notice_text.hpp"

#include "adl/app/wording.hpp"

namespace adl::app
{

QString notice_text(adl::arinc615a::Notice notice, std::string_view subject, std::uint32_t value)
{
    return Wording::notice(notice, subject, value).text();
}

} // namespace adl::app
