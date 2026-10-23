#include "adl/arinc615a/tftp.hpp"

#include "adl/arinc615a/limits.hpp"
#include "wire.hpp"

#include <algorithm>
#include <charconv>
#include <cstring>

namespace adl::arinc615a
{

namespace
{

using detail::Reader;
using detail::Writer;

constexpr std::string_view octet = "octet";
constexpr std::string_view checksum_prefix = "checksum_";

[[nodiscard]] char lower(char c) noexcept
{
    return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
}

[[nodiscard]] bool equal_ignoring_case(std::string_view a, std::string_view b) noexcept
{
    return a.size() == b.size() && std::ranges::equal(a, b, [](char x, char y) { return lower(x) == lower(y); });
}

[[nodiscard]] bool starts_with_ignoring_case(std::string_view text, std::string_view prefix) noexcept
{
    return text.size() >= prefix.size() && equal_ignoring_case(text.substr(0, prefix.size()), prefix);
}

// A whole decimal number: digits and nothing else.
template <typename T>
[[nodiscard]] std::optional<T> parse_decimal(std::string_view text) noexcept
{
    T value{};
    const auto [end, status] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (status != std::errc{} || end != text.data() + text.size() || text.empty() || text.front() == '+')
    {
        return std::nullopt;
    }
    return value;
}

[[nodiscard]] std::optional<unsigned> hex_digit(char c) noexcept
{
    if (c >= '0' && c <= '9')
    {
        return static_cast<unsigned>(c - '0');
    }
    if (c >= 'A' && c <= 'F')
    {
        return static_cast<unsigned>(c - 'A' + 10);
    }
    if (c >= 'a' && c <= 'f')
    {
        return static_cast<unsigned>(c - 'a' + 10);
    }
    return std::nullopt;
}

// One option, read from bytes the decoder has checked.
TftpOption read_option(std::span<const std::byte>& rest) noexcept
{
    Reader reader{rest};
    const std::string_view name = reader.c_string();
    const std::string_view value = reader.c_string();
    rest = rest.subspan(reader.position());
    return {name, value};
}

// Reads and checks the name and value pairs that fill the rest of a datagram.
OptionList read_options(Reader& reader) noexcept
{
    const auto start = reader.rest();
    const std::size_t first = reader.position();
    std::size_t count = 0;
    while (reader.ok() && reader.remaining() != 0)
    {
        const std::string_view name = reader.c_string();
        static_cast<void>(reader.c_string());
        if (reader.ok() && name.empty())
        {
            reader.fail(Error::bad_option);
        }
        ++count;
    }
    if (!reader.ok())
    {
        return {};
    }
    return OptionList::from_wire(start.first(reader.position() - first), count, &read_option);
}

void write_options(Writer& writer, const OptionList& options) noexcept
{
    for (const TftpOption option : options)
    {
        if (option.name.empty())
        {
            writer.fail(Error::bad_option);
        }
        writer.c_string(option.name);
        writer.c_string(option.value);
    }
}

void write_request(Writer& writer, const TftpRequest& packet) noexcept
{
    writer.u16(static_cast<std::uint16_t>(packet.kind == TftpRequest::Kind::read ? Opcode::read_request
                                                                                 : Opcode::write_request));
    if (packet.file_name.empty())
    {
        writer.fail(Error::invalid_value);
    }
    writer.c_string(packet.file_name);
    writer.c_string(octet);
    write_options(writer, packet.options);
}

void write_data(Writer& writer, const TftpData& packet) noexcept
{
    if (packet.data.size() > tftp_max_block_size)
    {
        writer.fail(Error::field_too_large);
    }
    writer.u16(static_cast<std::uint16_t>(Opcode::data));
    writer.u16(packet.block);
    writer.bytes(packet.data);
}

void write_ack(Writer& writer, const TftpAck& packet) noexcept
{
    writer.u16(static_cast<std::uint16_t>(Opcode::ack));
    writer.u16(packet.block);
}

void write_error(Writer& writer, const TftpError& packet) noexcept
{
    writer.u16(static_cast<std::uint16_t>(Opcode::error));
    writer.u16(static_cast<std::uint16_t>(packet.code));
    writer.c_string(packet.message);
}

void write_option_ack(Writer& writer, const TftpOptionAck& packet) noexcept
{
    if (packet.options.empty())
    {
        writer.fail(Error::bad_option);
    }
    writer.u16(static_cast<std::uint16_t>(Opcode::option_ack));
    write_options(writer, packet.options);
}

template <typename Packet, typename Write>
Result<std::size_t> encode_with(const Packet& packet, std::span<std::byte> out, Write write) noexcept
{
    Writer writer{out};
    write(writer, packet);
    return writer.finish();
}

template <typename Packet, typename Write>
Result<std::size_t> measure_with(const Packet& packet, Write write) noexcept
{
    Writer writer{std::span<std::byte>{}};
    write(writer, packet);
    return writer.measure();
}

// "WAIT:" or "ABORT:" and a number, as the text of an error packet.
Result<std::size_t> encode_message(std::string_view text, std::span<std::byte> out) noexcept
{
    return encode_error(TftpError{TftpErrorCode::not_defined, text}, out);
}

} // namespace

// ---- Packets ---------------------------------------------------------------------------

Result<Opcode> peek_opcode(std::span<const std::byte> bytes) noexcept
{
    Reader reader{bytes};
    const std::uint16_t value = reader.u16();
    if (!reader.ok())
    {
        return std::unexpected{reader.error()};
    }
    if (value < static_cast<std::uint16_t>(Opcode::read_request) ||
        value > static_cast<std::uint16_t>(Opcode::option_ack))
    {
        return std::unexpected{Error::unknown_opcode};
    }
    return static_cast<Opcode>(value);
}

Result<TftpPacket> decode_packet(std::span<const std::byte> bytes) noexcept
{
    const auto opcode = peek_opcode(bytes);
    if (!opcode)
    {
        return std::unexpected{opcode.error()};
    }
    Reader reader{bytes};
    static_cast<void>(reader.u16());

    TftpPacket packet;
    switch (*opcode)
    {
    case Opcode::read_request:
    case Opcode::write_request:
    {
        TftpRequest request;
        request.kind = *opcode == Opcode::read_request ? TftpRequest::Kind::read : TftpRequest::Kind::write;
        request.file_name = reader.c_string();
        const std::string_view mode = reader.c_string();
        if (reader.ok() && request.file_name.empty())
        {
            reader.fail(Error::invalid_value);
        }
        if (reader.ok() && !equal_ignoring_case(mode, octet))
        {
            reader.fail(Error::unsupported_mode);
        }
        request.options = read_options(reader);
        packet = request;
        break;
    }
    case Opcode::data:
    {
        TftpData data;
        data.block = reader.u16();
        data.data = reader.bytes(reader.remaining());
        if (reader.ok() && data.data.size() > tftp_max_block_size)
        {
            reader.fail(Error::bad_length);
        }
        packet = data;
        break;
    }
    case Opcode::ack:
    {
        TftpAck ack;
        ack.block = reader.u16();
        reader.expect_end();
        packet = ack;
        break;
    }
    case Opcode::error:
    {
        TftpError error;
        error.code = static_cast<TftpErrorCode>(reader.u16());
        error.message = reader.c_string();
        reader.expect_end();
        packet = error;
        break;
    }
    case Opcode::option_ack:
    {
        TftpOptionAck oack;
        oack.options = read_options(reader);
        if (reader.ok() && oack.options.empty())
        {
            reader.fail(Error::bad_option);
        }
        packet = oack;
        break;
    }
    }
    if (!reader.ok())
    {
        return std::unexpected{reader.error()};
    }
    return packet;
}

Result<std::size_t> encode_request(const TftpRequest& packet, std::span<std::byte> out) noexcept
{
    return encode_with(packet, out, write_request);
}
Result<std::size_t> encode_data(const TftpData& packet, std::span<std::byte> out) noexcept
{
    return encode_with(packet, out, write_data);
}
Result<std::size_t> encode_ack(const TftpAck& packet, std::span<std::byte> out) noexcept
{
    return encode_with(packet, out, write_ack);
}
Result<std::size_t> encode_error(const TftpError& packet, std::span<std::byte> out) noexcept
{
    return encode_with(packet, out, write_error);
}
Result<std::size_t> encode_option_ack(const TftpOptionAck& packet, std::span<std::byte> out) noexcept
{
    return encode_with(packet, out, write_option_ack);
}

Result<std::size_t> encode_packet(const TftpPacket& packet, std::span<std::byte> out) noexcept
{
    if (const auto* p = std::get_if<TftpRequest>(&packet))
    {
        return encode_request(*p, out);
    }
    if (const auto* p = std::get_if<TftpData>(&packet))
    {
        return encode_data(*p, out);
    }
    if (const auto* p = std::get_if<TftpAck>(&packet))
    {
        return encode_ack(*p, out);
    }
    if (const auto* p = std::get_if<TftpError>(&packet))
    {
        return encode_error(*p, out);
    }
    if (const auto* p = std::get_if<TftpOptionAck>(&packet))
    {
        return encode_option_ack(*p, out);
    }
    return std::unexpected{Error::invalid_value};
}

Result<std::size_t> encoded_size(const TftpPacket& packet) noexcept
{
    if (const auto* p = std::get_if<TftpRequest>(&packet))
    {
        return measure_with(*p, write_request);
    }
    if (const auto* p = std::get_if<TftpData>(&packet))
    {
        return measure_with(*p, write_data);
    }
    if (const auto* p = std::get_if<TftpAck>(&packet))
    {
        return measure_with(*p, write_ack);
    }
    if (const auto* p = std::get_if<TftpError>(&packet))
    {
        return measure_with(*p, write_error);
    }
    if (const auto* p = std::get_if<TftpOptionAck>(&packet))
    {
        return measure_with(*p, write_option_ack);
    }
    return std::unexpected{Error::invalid_value};
}

// ---- Options ---------------------------------------------------------------------------

std::optional<std::string_view> find_option(const OptionList& options, std::string_view name) noexcept
{
    for (const TftpOption option : options)
    {
        if (equal_ignoring_case(option.name, name))
        {
            return option.value;
        }
    }
    return std::nullopt;
}

namespace
{

// Hex characters of an even count to bytes; nothing if any character is not hex.
std::optional<ChecksumOption> read_checksum(adl::arinc665::CheckValueType type, std::string_view text) noexcept
{
    const std::size_t length = adl::arinc665::check_value_length(type);
    if (length == 0 || text.size() != 2 * length)
    {
        return std::nullopt;
    }
    ChecksumOption checksum;
    checksum.type = type;
    checksum.length = static_cast<std::uint8_t>(length);
    for (std::size_t i = 0; i < length; ++i)
    {
        const auto high = hex_digit(text[2 * i]);
        const auto low = hex_digit(text[2 * i + 1]);
        if (!high || !low)
        {
            return std::nullopt;
        }
        checksum.value[i] = static_cast<std::byte>((*high << 4) | *low);
    }
    return checksum;
}

std::optional<adl::arinc665::CheckValueType> known_check_type(unsigned number) noexcept
{
    using adl::arinc665::CheckValueType;
    if (number >= static_cast<unsigned>(CheckValueType::crc8) && number <= static_cast<unsigned>(CheckValueType::sha1))
    {
        return static_cast<CheckValueType>(number);
    }
    return std::nullopt;
}

template <typename T>
bool set_once(std::optional<T>& slot, std::optional<T> value) noexcept
{
    if (slot || !value)
    {
        return false;
    }
    slot = value;
    return true;
}

} // namespace

Result<TransferOptions> read_transfer_options(const OptionList& options) noexcept
{
    TransferOptions result;
    for (const TftpOption option : options)
    {
        bool good = true;
        if (equal_ignoring_case(option.name, "blksize"))
        {
            const auto value = parse_decimal<std::uint32_t>(option.value);
            good = value && *value >= tftp_min_block_size && *value <= tftp_max_block_size &&
                   set_once(result.block_size, std::optional<std::uint16_t>(static_cast<std::uint16_t>(*value)));
        }
        else if (equal_ignoring_case(option.name, "tsize"))
        {
            good = set_once(result.transfer_size, parse_decimal<std::uint64_t>(option.value));
        }
        else if (equal_ignoring_case(option.name, "timeout"))
        {
            const auto value = parse_decimal<std::uint32_t>(option.value);
            good = value && *value >= 1 && *value <= 255 &&
                   set_once(result.timeout, std::optional<std::uint8_t>(static_cast<std::uint8_t>(*value)));
        }
        else if (equal_ignoring_case(option.name, "port"))
        {
            const auto value = parse_decimal<std::uint32_t>(option.value);
            good = value && *value >= 1 && *value <= 0xFFFFU &&
                   set_once(result.port, std::optional<std::uint16_t>(static_cast<std::uint16_t>(*value)));
        }
        else if (starts_with_ignoring_case(option.name, checksum_prefix))
        {
            // An algorithm this library does not know is an option it does not know.
            const auto number = parse_decimal<std::uint32_t>(option.name.substr(checksum_prefix.size()));
            const auto type = number ? known_check_type(*number) : std::nullopt;
            if (!number)
            {
                good = false;
            }
            else if (type)
            {
                good = set_once(result.checksum, read_checksum(*type, option.value));
            }
        }
        if (!good)
        {
            return std::unexpected{Error::bad_option};
        }
    }
    return result;
}

Result<void> OptionBuilder::add(std::string_view name, std::string_view value) noexcept
{
    if (name.empty() || !detail::is_text(name) || !detail::is_text(value))
    {
        return std::unexpected{Error::bad_option};
    }
    const std::size_t needed = name.size() + value.size() + 2;
    if (needed > capacity - size_)
    {
        return std::unexpected{Error::buffer_too_small};
    }
    char* at = text_.data() + size_;
    std::memcpy(at, name.data(), name.size());
    at[name.size()] = '\0';
    std::memcpy(at + name.size() + 1, value.data(), value.size());
    at[name.size() + 1 + value.size()] = '\0';
    size_ += needed;
    ++count_;
    return {};
}

namespace
{

template <typename T>
Result<void> add_number(OptionBuilder& builder, std::string_view name, T value) noexcept
{
    char digits[24];
    const auto [end, status] = std::to_chars(digits, digits + sizeof digits, value);
    static_cast<void>(status);
    return builder.add(name, std::string_view{digits, static_cast<std::size_t>(end - digits)});
}

} // namespace

Result<void> OptionBuilder::add_block_size(std::uint16_t size) noexcept
{
    if (size < tftp_min_block_size || size > tftp_max_block_size)
    {
        return std::unexpected{Error::bad_option};
    }
    return add_number(*this, "blksize", size);
}

Result<void> OptionBuilder::add_transfer_size(std::uint64_t size) noexcept
{
    return add_number(*this, "tsize", size);
}

Result<void> OptionBuilder::add_timeout(std::uint8_t seconds) noexcept
{
    if (seconds == 0)
    {
        return std::unexpected{Error::bad_option};
    }
    return add_number(*this, "timeout", static_cast<unsigned>(seconds));
}

Result<void> OptionBuilder::add_port(std::uint16_t port) noexcept
{
    if (port == 0)
    {
        return std::unexpected{Error::bad_option};
    }
    return add_number(*this, "port", port);
}

Result<void> OptionBuilder::add_checksum(const ChecksumOption& checksum) noexcept
{
    const auto type = known_check_type(static_cast<unsigned>(checksum.type));
    if (!type || checksum.length != adl::arinc665::check_value_length(*type))
    {
        return std::unexpected{Error::bad_option};
    }
    char name[16] = "checksum_";
    name[9] = static_cast<char>('0' + static_cast<unsigned>(*type));
    name[10] = '\0';
    char value[2 * 20 + 1];
    constexpr std::string_view digits = "0123456789ABCDEF";
    for (std::size_t i = 0; i < checksum.length; ++i)
    {
        const unsigned byte = std::to_integer<unsigned>(checksum.value[i]);
        value[2 * i] = digits[byte >> 4];
        value[2 * i + 1] = digits[byte & 0xFU];
    }
    return add(std::string_view{name, 10}, std::string_view{value, std::size_t{2} * checksum.length});
}

OptionList OptionBuilder::list() const noexcept
{
    return OptionList::from_wire(std::as_bytes(std::span<const char>{text_.data(), size_}), count_, &read_option);
}

// ---- WAIT and ABORT --------------------------------------------------------------------

Result<std::size_t> encode_wait(const WaitMessage& message, std::span<std::byte> out) noexcept
{
    char text[16] = "WAIT:";
    const auto [end, status] = std::to_chars(text + 5, text + sizeof text, message.seconds);
    static_cast<void>(status);
    return encode_message(std::string_view{text, static_cast<std::size_t>(end - text)}, out);
}

Result<std::size_t> encode_abort(const AbortMessage& message, std::span<std::byte> out) noexcept
{
    char text[16] = "ABORT:";
    constexpr std::string_view digits = "0123456789ABCDEF";
    const auto code = static_cast<std::uint16_t>(message.status);
    for (std::size_t i = 0; i < 4; ++i)
    {
        text[6 + i] = digits[(code >> (12 - 4 * i)) & 0xFU];
    }
    return encode_message(std::string_view{text, 10}, out);
}

Result<ErrorMeaning> interpret_error(const TftpError& error) noexcept
{
    if (error.code != TftpErrorCode::not_defined)
    {
        return error;
    }
    constexpr std::string_view wait = "WAIT:";
    constexpr std::string_view abort = "ABORT:";
    if (error.message.starts_with(wait))
    {
        const std::string_view digits = error.message.substr(wait.size());
        const auto seconds = digits.size() <= 5 ? parse_decimal<std::uint32_t>(digits) : std::nullopt;
        if (!seconds || *seconds > max_wait_seconds)
        {
            return std::unexpected{Error::invalid_value};
        }
        return WaitMessage{static_cast<std::uint16_t>(*seconds)};
    }
    if (error.message.starts_with(abort))
    {
        const std::string_view digits = error.message.substr(abort.size());
        if (digits.size() != 4)
        {
            return std::unexpected{Error::invalid_value};
        }
        unsigned code = 0;
        for (const char c : digits)
        {
            const auto digit = hex_digit(c);
            if (!digit)
            {
                return std::unexpected{Error::invalid_value};
            }
            code = (code << 4) | *digit;
        }
        // 6.4.10 writes the abort status as four hexadecimal characters and
        // the code is carried as it stands. StatusCode's underlying type is
        // fixed at std::uint16_t, so every value the four characters can spell
        // is a value of the type; the codec's tests hold that down.
        return AbortMessage{static_cast<StatusCode>(code)};
    }
    return error;
}

} // namespace adl::arinc615a
