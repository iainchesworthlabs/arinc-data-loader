// SPDX-License-Identifier: GPL-3.0-or-later
// The plain words for a licence's state, edition and reason. The command line prints them, and a program
// that reads them reads the same eight state names whatever product it is.

#include "adl/licence/licence.hpp"
#include "adl/licence/words.hpp"
#include "support.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string_view>
#include <vector>

using namespace adl::licence;
using namespace adl::test;

namespace
{

constexpr State all_states[] = {State::none,    State::valid,         State::expiring,      State::grace,
                                State::expired, State::not_yet_valid, State::clock_suspect, State::invalid};

constexpr Reason all_reasons[] = {Reason::ok,
                                  Reason::no_licence,
                                  Reason::expired,
                                  Reason::not_yet_valid,
                                  Reason::clock_suspect,
                                  Reason::malformed,
                                  Reason::newer_version,
                                  Reason::bad_signature,
                                  Reason::revoked,
                                  Reason::wrong_computer,
                                  Reason::wrong_product,
                                  Reason::not_in_licence};

} // namespace

TEST_CASE("words: the eight state names are the same in every product", "[licence][words]")
{
    // These are the names a script reads. They do not change.
    REQUIRE(state_name(State::none) == "none");
    REQUIRE(state_name(State::valid) == "valid");
    REQUIRE(state_name(State::expiring) == "expiring");
    REQUIRE(state_name(State::grace) == "grace");
    REQUIRE(state_name(State::expired) == "expired");
    REQUIRE(state_name(State::not_yet_valid) == "not_yet_valid");
    REQUIRE(state_name(State::clock_suspect) == "clock_suspect");
    REQUIRE(state_name(State::invalid) == "invalid");

    REQUIRE(state_word(State::none) == "none");
    REQUIRE(state_word(State::not_yet_valid) == "not yet valid");
    REQUIRE(state_word(State::clock_suspect) == "clock suspect");

    std::vector<std::string_view> names;
    std::vector<std::string_view> words;
    for (const State state : all_states)
    {
        names.push_back(state_name(state));
        words.push_back(state_word(state));
        REQUIRE(!state_name(state).empty());
        REQUIRE(!state_word(state).empty());
    }
    std::ranges::sort(names);
    std::ranges::sort(words);
    REQUIRE(std::ranges::adjacent_find(names) == names.end());
    REQUIRE(std::ranges::adjacent_find(words) == words.end());

    // A value that is not a state is still an answer, and not one a script mistakes for a good one.
    REQUIRE(state_name(static_cast<State>(99)) == "invalid");
    REQUIRE(state_word(static_cast<State>(99)) == "unknown");
}

TEST_CASE("words: every reason has its own plain sentence", "[licence][words]")
{
    std::vector<std::string_view> sentences;
    for (const Reason reason : all_reasons)
    {
        const std::string_view sentence = reason_sentence(reason);
        REQUIRE(!sentence.empty());
        // One lower-case clause: no full stop and no capital letter at the start.
        REQUIRE(sentence.back() != '.');
        REQUIRE(sentence.front() >= 'a');
        sentences.push_back(sentence);
    }
    std::ranges::sort(sentences);
    REQUIRE(std::ranges::adjacent_find(sentences) == sentences.end());

    REQUIRE(reason_sentence(Reason::ok) == "ok");
    REQUIRE(reason_sentence(Reason::revoked) == "the licence or its key was withdrawn");
    REQUIRE(reason_sentence(Reason::wrong_product) == "the licence is for another product");
    REQUIRE(reason_sentence(static_cast<Reason>(99)) == "unknown");
}

TEST_CASE("words: the edition a status amounts to", "[licence][words]")
{
    // No licence, or one that is not in force, is Community.
    REQUIRE(edition_name(no_licence()) == "community");
    REQUIRE(edition_name(evaluate("junk", on(at(2027, 1, 1)))) == "community");
    REQUIRE(edition_name(evaluate(signed_text(annual()), on(at(2030, 1, 1)))) == "community");

    // In its term, and in its grace period, the licence names the edition.
    REQUIRE(edition_name(evaluate(signed_text(annual()), on(at(2027, 1, 1)))) == "pro");
    REQUIRE(edition_name(evaluate(signed_text(annual()), on(at(2027, 12, 1)))) == "pro");
    REQUIRE(edition_name(evaluate(signed_text(full()), on(at(2027, 1, 1)))) == "enterprise");
}
