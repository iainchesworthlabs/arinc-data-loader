#include "adl/arinc615a/find.hpp"

#include "adl/arinc615a/limits.hpp"
#include "wire.hpp"

#include <array>

namespace adl::arinc615a
{

namespace
{

using detail::Reader;
using detail::Writer;

// A FIND string may be empty, may not hold a NUL, and has a limit on its length.
void write_field(Writer& writer, std::string_view text, std::size_t limit) noexcept
{
    if (text.size() > limit)
    {
        writer.fail(Error::field_too_large);
    }
    writer.c_string(text);
}

void write_answer(Writer& writer, const FindAnswer& answer) noexcept
{
    writer.u16(static_cast<std::uint16_t>(FindOpcode::answer));
    write_field(writer, answer.target_hardware_id, find_max_hardware_id);
    write_field(writer, answer.target_type_name, find_max_type_name);
    write_field(writer, answer.target_position, find_max_position);
    write_field(writer, answer.literal_name, find_max_literal_name);
    write_field(writer, answer.manufacturer_code, find_max_manufacturer_code);
    writer.u8(find_packet_terminator);
}

std::string_view read_field(Reader& reader, std::size_t limit) noexcept
{
    const std::string_view text = reader.c_string();
    if (reader.ok() && text.size() > limit)
    {
        reader.fail(Error::bad_length);
        return {};
    }
    return text;
}

} // namespace

Result<std::size_t> encode_find_request(std::span<std::byte> out) noexcept
{
    Writer writer{out};
    writer.u16(static_cast<std::uint16_t>(FindOpcode::request));
    writer.u8(0);
    writer.u8(find_packet_terminator);
    return writer.finish();
}

Result<std::size_t> encode_find_answer(const FindAnswer& answer, std::span<std::byte> out) noexcept
{
    Writer writer{out};
    write_answer(writer, answer);
    return writer.finish();
}

Result<std::size_t> encoded_size(const FindAnswer& answer) noexcept
{
    Writer writer{std::span<std::byte>{}};
    write_answer(writer, answer);
    return writer.measure();
}

Result<FindPacket> decode_find(std::span<const std::byte> bytes) noexcept
{
    Reader reader{bytes};
    const std::uint16_t opcode = reader.u16();
    FindPacket packet;
    if (reader.ok() && opcode == static_cast<std::uint16_t>(FindOpcode::request))
    {
        // The data list of a request is the empty string: one NUL.
        if (!reader.c_string().empty() && reader.ok())
        {
            reader.fail(Error::invalid_value);
        }
        packet = FindRequest{};
    }
    else if (reader.ok() && opcode == static_cast<std::uint16_t>(FindOpcode::answer))
    {
        FindAnswer answer;
        answer.target_hardware_id = read_field(reader, find_max_hardware_id);
        answer.target_type_name = read_field(reader, find_max_type_name);
        answer.target_position = read_field(reader, find_max_position);
        answer.literal_name = read_field(reader, find_max_literal_name);
        answer.manufacturer_code = read_field(reader, find_max_manufacturer_code);
        packet = answer;
    }
    else if (reader.ok())
    {
        reader.fail(Error::unknown_opcode);
    }
    if (reader.ok() && reader.u8() != find_packet_terminator && reader.ok())
    {
        reader.fail(Error::invalid_value);
    }
    reader.expect_end();
    if (!reader.ok())
    {
        return std::unexpected{reader.error()};
    }
    return packet;
}

} // namespace adl::arinc615a
