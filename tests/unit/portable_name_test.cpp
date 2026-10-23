#include "adl/arinc665/portable_name.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <string_view>

using namespace adl::arinc665;

TEST_CASE("an ordinary file name is portable", "[portable-name]")
{
    for (const std::string_view name :
         {"A", "APP.LUP", "HNPFMS_L.LCS", "lower.case", "a.b.c", "with space.txt", "x-y_z~1", "a,b", "CONFIG.LDR",
          ".hidden", "COMPORT.BIN", "CONSOLE", "AUXILIARY.TXT", "COM10", "COM0", "LPT0.TXT", "NULL", "PRNT", "x"})
    {
        CAPTURE(name);
        CHECK(is_portable_file_name(name));
    }
}

// What a target can name a file when the operator's download directory is on any host: the
// answer is the same on every platform, because the tree may be copied to another.
TEST_CASE("hostile file names are refused on every platform", "[portable-name]")
{
    const std::string long_but_fine(255, 'a');
    struct Case
    {
        std::string_view name;
        const char* why;
    };
    const Case hostile[] = {
        {"", "empty"},
        {".", "the directory itself"},
        {"..", "the parent directory"},
        {"a/b", "a path separator"},
        {"a\\b", "a Windows path separator"},
        {"/etc/passwd", "an absolute path"},
        {"..\\x", "a Windows parent path"},
        {"C:", "a drive"},
        {"C:x", "a drive-relative name"},
        {"a:b", "an NTFS alternate data stream"},
        {"a:b:$DATA", "an NTFS alternate data stream with its type"},
        {"file.txt::$DATA", "the default data stream"},
        {"NUL", "a Windows device"},
        {"nul", "a Windows device in any case"},
        {"Nul.txt", "a device with an extension"},
        {"NUL.tar.gz", "a device with several extensions"},
        {"CON", "the console"},
        {"con", "the console in any case"},
        {"PRN", "the printer"},
        {"AUX", "the auxiliary device"},
        {"COM1", "a serial port"},
        {"com9", "a serial port in any case"},
        {"COM5.log", "a serial port with an extension"},
        {"LPT1", "a parallel port"},
        {"lpt9.txt", "a parallel port with an extension"},
        {"CONIN$", "the console input"},
        {"conout$", "the console output"},
        {"CONIN$.txt", "the console input with an extension"},
        {"trailing.", "a trailing dot, which Windows drops"},
        {"trailing ", "a trailing space, which Windows drops"},
        {"trailing. ", "a trailing dot and space"},
        {"a*b", "a wildcard"},
        {"a?b", "a wildcard"},
        {"a\"b", "a quote"},
        {"a<b", "a redirection"},
        {"a>b", "a redirection"},
        {"a|b", "a pipe"},
        {"tab\there", "a control character"},
        {"new\nline", "a control character"},
        {"bell\a", "a control character"},
        {std::string_view{"nul\0byte", 8}, "an embedded NUL"},
        {"del\x7f", "DEL"},
        {"caf\xC3\xA9", "a character outside printable ASCII"},
        {"\xEF\xBC\x8F", "a full-width solidus, which some hosts fold to a separator"},
    };
    for (const Case& entry : hostile)
    {
        CAPTURE(entry.why, std::string{entry.name});
        CHECK_FALSE(is_portable_file_name(entry.name));
    }
    CHECK(is_portable_file_name(long_but_fine));
}

TEST_CASE("device names are told with or without an extension", "[portable-name]")
{
    for (const std::string_view name : {"CON", "con", "PRN", "AUX", "NUL", "COM1", "COM9", "LPT1", "LPT9", "CONIN$",
                                        "CONOUT$", "nul.txt", "Com3.bin", "aux.", "conin$.x"})
    {
        CAPTURE(name);
        CHECK(is_device_name(name));
    }
    for (const std::string_view name :
         {"", "COM", "COM0", "COM10", "LPT0", "CONS", "NULL", "AUXX", "xCON", "CONIN", "CONIN$$", "a.NUL", "x.con"})
    {
        CAPTURE(name);
        CHECK_FALSE(is_device_name(name));
    }
}
