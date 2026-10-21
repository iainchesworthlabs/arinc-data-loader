#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <string>

#include "adl/arinc665/md5.hpp"
#include "adl/arinc665/sha1.hpp"
#include "bytes.hpp"

using adl::arinc665::Md5;
using adl::arinc665::Sha1;
using adl::test::bytes_of;
using adl::test::to_hex;

namespace {

std::string md5_hex(const std::string& text) {
    Md5 md5;
    md5.update(bytes_of(text));
    return to_hex(md5.digest());
}

std::string sha1_hex(const std::string& text) {
    Sha1 sha1;
    sha1.update(bytes_of(text));
    return to_hex(sha1.digest());
}

}  // namespace

// RFC 1321 appendix A.5.
TEST_CASE("MD5 matches the RFC 1321 test suite", "[digest]") {
    CHECK(md5_hex("") == "d41d8cd98f00b204e9800998ecf8427e");
    CHECK(md5_hex("a") == "0cc175b9c0f1b6a831c399e269772661");
    CHECK(md5_hex("abc") == "900150983cd24fb0d6963f7d28e17f72");
    CHECK(md5_hex("message digest") == "f96b697d7cb7938d525a2f31aaf161d0");
    CHECK(md5_hex("abcdefghijklmnopqrstuvwxyz") == "c3fcd3d76192e4007dfb496cca67e13b");
    CHECK(md5_hex("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789") ==
          "d174ab98d277d9f5a5611c2c9f419d9f");
    CHECK(md5_hex(
              "12345678901234567890123456789012345678901234567890123456789012345678901234567890") ==
          "57edf4a22be3c955ac49da2e2107b67a");
}

// FIPS 180-1 appendix A and B.
TEST_CASE("SHA-1 matches the FIPS 180 test vectors", "[digest]") {
    CHECK(sha1_hex("") == "da39a3ee5e6b4b0d3255bfef95601890afd80709");
    CHECK(sha1_hex("abc") == "a9993e364706816aba3e25717850c26c9cd0d89d");
    CHECK(sha1_hex("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq") ==
          "84983e441c3bd26ebaae4aa1f95129e5e54670f1");
}

TEST_CASE("a million 'a' hashes as the standards say", "[digest]") {
    const auto million = bytes_of(std::string(1'000'000, 'a'));

    Md5 md5;
    md5.update(million);
    CHECK(to_hex(md5.digest()) == "7707d6ae4e027c70eea2a935c2296f21");

    Sha1 sha1;
    sha1.update(million);
    CHECK(to_hex(sha1.digest()) == "34aa973cd4c4daa4f61eeb2bdbad27316534016f");
}

TEST_CASE("digests are right across every padding boundary", "[digest]") {
    // Lengths either side of the 56-byte point where the padding spills into a
    // second block, and of the block size itself.
    for (const std::size_t length : {54U, 55U, 56U, 57U, 63U, 64U, 65U, 119U, 120U, 128U}) {
        const auto data = bytes_of(std::string(length, 'x'));

        Md5 whole;
        whole.update(data);

        Md5 bytewise;
        for (const std::byte b : data) {
            bytewise.update(std::span{&b, 1});
        }
        CHECK(whole.digest() == bytewise.digest());

        Sha1 sha_whole;
        sha_whole.update(data);
        Sha1 sha_bytewise;
        for (const std::byte b : data) {
            sha_bytewise.update(std::span{&b, 1});
        }
        CHECK(sha_whole.digest() == sha_bytewise.digest());
    }
}

TEST_CASE("reading a digest does not disturb the running state", "[digest]") {
    Sha1 sha1;
    sha1.update(bytes_of("ab"));
    static_cast<void>(sha1.digest());
    sha1.update(bytes_of("c"));

    CHECK(to_hex(sha1.digest()) == "a9993e364706816aba3e25717850c26c9cd0d89d");
}

TEST_CASE("reset starts a digest again", "[digest]") {
    Md5 md5;
    md5.update(bytes_of("junk"));
    md5.reset();
    md5.update(bytes_of("abc"));

    CHECK(to_hex(md5.digest()) == "900150983cd24fb0d6963f7d28e17f72");
}
