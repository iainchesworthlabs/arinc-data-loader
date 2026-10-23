// The Qt Linguist catalogues of the three applications and of the interface they share
// (libs/ui/translations, apps/app/translations, apps/media_gen/translations,
// apps/mock_target/translations), read as files rather than through Qt: these cases run in the
// plain unit-test program on every CI leg, Qt installed or not. What they hold the catalogues to is
// what a reader of a catalogue cannot see at a glance and what a hand edit or a machine pass gets
// wrong: a message left unfinished or vanished, an empty translation, a dropped placeholder or
// markup tag, a plural form missing, an identifier translated into something else, and one language
// holding a message the others do not.
//
// Whether the catalogues still match the sources is the `translations.current.*` tests' business
// (they run lupdate over a copy, cmake/CheckTranslations.cmake). Whether the translations are
// good is a speaker's: the seven non-English catalogues are machine-drafted first versions, which
// docs/app.md says plainly; this suite asserts that every message HAS a translation of the right
// shape, which is a smaller claim than that every translation is right.
//
// lupdate writes a fixed shape - one element per line, no attribute order to guess at - so the scan
// below is a plain string walk and needs no XML parser.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

namespace {

namespace fs = std::filesystem;

// The languages besides English, and how many plural forms each needs (what lupdate writes).
struct Language {
    std::string_view code;
    std::size_t forms;
};
constexpr std::array<Language, 7> kLanguages{
    {{"de", 2}, {"es", 2}, {"fr", 2}, {"he", 2}, {"ja", 1}, {"yi", 2}, {"zh", 1}}};

// One family of catalogues: <directory>/<base>_<code>.ts for each language, and <base>_en.ts
// for English, which holds the plural messages and nothing else.
struct Family {
    std::string_view name;
    std::string_view directory;
    std::string_view base;
    // The least messages the family has; a file that parsed to nothing passes every other rule.
    std::size_t at_least;
};

const std::array<Family, 4> kFamilies{{
    {"ui", ADL_UI_TRANSLATIONS_DIR, "adl_ui", 8},
    {"app", ADL_APP_TRANSLATIONS_DIR, "adl_app", 300},
    {"media_gen", ADL_MEDIA_GEN_TRANSLATIONS_DIR, "adl_media_gen", 120},
    {"mock_target", ADL_MOCK_TARGET_TRANSLATIONS_DIR, "adl_mock_target", 90},
}};

// Words that name a standard, a protocol, a file or an identifier. They read the same in every
// language, and a translation that has lost one has renamed something rather than translated it.
constexpr std::array<std::string_view, 12> kKeptTerms{
    "ARINC", "THW_ID_POS", "FILES.LUM", "DNLD_DATA", "IPv4", "TFTP",
    "UDP",   "CRC",        "Qt",        "LGPL",      "OFL",  "Archivo"};

struct Message {
    std::string context;
    std::string source;
    std::string comment;
    // Whatever stood inside the <translation ...> tag, to see whether the message is marked.
    std::string attributes;
    std::string translation;
    // `<message numerus="yes">`: the translation holds one <numerusform> per plural form.
    bool numerus = false;
    std::vector<std::string> forms;
};

[[nodiscard]] std::string read_file(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    INFO("catalogue " << path.string());
    REQUIRE(in.is_open());
    return std::string{std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
}

[[nodiscard]] fs::path catalogue_path(const Family& family, std::string_view code) {
    return fs::path{std::string{family.directory}} /
           (std::string{family.base} + "_" + std::string{code} + ".ts");
}

void append_utf8(std::string& out, char32_t code) {
    if (code < 0x80) {
        out += static_cast<char>(code);
    } else if (code < 0x800) {
        out += static_cast<char>(0xC0 | (code >> 6));
        out += static_cast<char>(0x80 | (code & 0x3F));
    } else if (code < 0x10000) {
        out += static_cast<char>(0xE0 | (code >> 12));
        out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (code & 0x3F));
    } else {
        out += static_cast<char>(0xF0 | (code >> 18));
        out += static_cast<char>(0x80 | ((code >> 12) & 0x3F));
        out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (code & 0x3F));
    }
}

// The five predefined entities, and numeric references, as lupdate writes them.
[[nodiscard]] std::string unescape(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] != '&') {
            out += text[i];
            continue;
        }
        const auto end = text.find(';', i);
        if (end == std::string_view::npos) {
            out += text[i];
            continue;
        }
        const std::string_view entity = text.substr(i + 1, end - i - 1);
        if (entity == "amp") {
            out += '&';
        } else if (entity == "lt") {
            out += '<';
        } else if (entity == "gt") {
            out += '>';
        } else if (entity == "quot") {
            out += '"';
        } else if (entity == "apos") {
            out += '\'';
        } else if (!entity.empty() && entity[0] == '#') {
            const bool hex = entity.size() > 1 && entity[1] == 'x';
            const unsigned long code =
                std::stoul(std::string{entity.substr(hex ? 2 : 1)}, nullptr, hex ? 16 : 10);
            append_utf8(out, static_cast<char32_t>(code));
        } else {
            out.append(text.substr(i, end - i + 1));
        }
        i = end;
    }
    return out;
}

// The text between `open` and the first `close` after it, searching from `from` and no further
// than `until`; npos in `next` when there is none.
[[nodiscard]] std::string element(const std::string& text, std::size_t from, std::size_t until,
                                  std::string_view open, std::string_view close,
                                  std::size_t& next) {
    next = std::string::npos;
    const auto start = text.find(open, from);
    if (start == std::string::npos || start >= until) {
        return {};
    }
    const auto body = start + open.size();
    const auto end = text.find(close, body);
    if (end == std::string::npos || end > until) {
        return {};
    }
    next = end + close.size();
    return text.substr(body, end - body);
}

[[nodiscard]] std::vector<Message> parse(const std::string& text) {
    std::vector<Message> messages;
    std::string context;
    std::size_t at = 0;
    while (true) {
        const auto name_at = text.find("<name>", at);
        const auto message_at = text.find("<message", at);
        if (message_at == std::string::npos) {
            break;
        }
        // A <context> opens with its <name>; every <message> after it belongs to it until the next.
        if (name_at != std::string::npos && name_at < message_at) {
            std::size_t next = 0;
            context = unescape(element(text, name_at, message_at, "<name>", "</name>", next));
            at = next;
            continue;
        }
        const auto message_end = text.find("</message>", message_at);
        REQUIRE(message_end != std::string::npos);
        const auto tag_end = text.find('>', message_at);
        Message message;
        message.context = context;
        message.numerus = text.substr(message_at, tag_end - message_at).find("numerus=\"yes\"") !=
                          std::string::npos;
        std::size_t next = 0;
        message.source =
            unescape(element(text, message_at, message_end, "<source>", "</source>", next));
        message.comment =
            unescape(element(text, message_at, message_end, "<comment>", "</comment>", next));

        const auto translation_at = text.find("<translation", message_at);
        REQUIRE(translation_at != std::string::npos);
        REQUIRE(translation_at < message_end);
        const auto translation_tag_end = text.find('>', translation_at);
        message.attributes =
            text.substr(translation_at + 12, translation_tag_end - translation_at - 12);
        // An empty translation is written <translation type="unfinished"></translation> or
        // <translation type="unfinished"/>; the body is empty either way.
        const bool self_closing = text[translation_tag_end - 1] == '/';
        const auto translation_end =
            self_closing ? translation_tag_end : text.find("</translation>", translation_tag_end);
        REQUIRE(translation_end != std::string::npos);
        const std::string body =
            self_closing
                ? std::string{}
                : text.substr(translation_tag_end + 1, translation_end - translation_tag_end - 1);
        if (message.numerus) {
            std::size_t form_at = 0;
            while (true) {
                const auto form_start = body.find("<numerusform", form_at);
                if (form_start == std::string::npos) {
                    break;
                }
                const auto form_tag_end = body.find('>', form_start);
                const auto form_end = body.find("</numerusform>", form_tag_end);
                const bool empty_form = body[form_tag_end - 1] == '/';
                message.forms.push_back(
                    empty_form
                        ? std::string{}
                        : unescape(body.substr(form_tag_end + 1, form_end - form_tag_end - 1)));
                form_at = empty_form ? form_tag_end : form_end;
            }
        } else {
            message.translation = unescape(body);
        }
        messages.push_back(std::move(message));
        at = message_end + 10;
    }
    return messages;
}

[[nodiscard]] bool marked(const Message& message, std::string_view mark) {
    return message.attributes.find(mark) != std::string::npos;
}

[[nodiscard]] bool blank(std::string_view text) {
    return std::ranges::all_of(text, [](char c) { return c == ' ' || c == '\t' || c == '\n'; });
}

// The placeholders (%1 to %9 and %n) in the text, once each, in order of first appearance.
[[nodiscard]] std::set<std::string> placeholders(std::string_view text) {
    std::set<std::string> found;
    for (std::size_t i = 0; i + 1 < text.size(); ++i) {
        if (text[i] == '%' && ((text[i + 1] >= '1' && text[i + 1] <= '9') || text[i + 1] == 'n')) {
            found.insert(std::string{text.substr(i, 2)});
        }
    }
    return found;
}

// The markup tags (<a href="...">, </a>) in the text.
[[nodiscard]] std::vector<std::string> tags(std::string_view text) {
    std::vector<std::string> found;
    for (std::size_t i = text.find('<'); i != std::string_view::npos; i = text.find('<', i + 1)) {
        const auto end = text.find('>', i);
        if (end == std::string_view::npos) {
            break;
        }
        found.emplace_back(text.substr(i, end - i + 1));
    }
    return found;
}

[[nodiscard]] bool carries_term(std::string_view text, std::string_view term) {
    for (auto at = text.find(term); at != std::string_view::npos; at = text.find(term, at + 1)) {
        const bool before_ok =
            at == 0 ||
            !(std::isalnum(static_cast<unsigned char>(text[at - 1])) || text[at - 1] == '_');
        const std::size_t after = at + term.size();
        const bool after_ok =
            after >= text.size() ||
            !(std::isalnum(static_cast<unsigned char>(text[after])) || text[after] == '_');
        if (before_ok && after_ok) {
            return true;
        }
    }
    return false;
}

[[maybe_unused]] std::string quoted(std::string_view text) {
    return "\"" + std::string{text.substr(0, 80)} + (text.size() > 80 ? "...\"" : "\"");
}

// The texts a message must be checked as: the one translation, or each plural form.
[[nodiscard]] std::vector<std::string> bodies(const Message& message) {
    return message.numerus ? message.forms : std::vector<std::string>{message.translation};
}

// ---- the rules ---------------------------------------------------------------------------------

void check_finished(const Family& family, const Language& language) {
    const auto messages = parse(read_file(catalogue_path(family, language.code)));
    INFO(family.name << "_" << language.code << ".ts parsed to " << messages.size() << " messages");
    CHECK(messages.size() >= family.at_least);
    for (const Message& message : messages) {
        INFO(language.code << " / " << message.context << ": " << quoted(message.source));
        CHECK_FALSE(marked(message, "unfinished"));
        CHECK_FALSE(marked(message, "vanished"));
        CHECK_FALSE(marked(message, "obsolete"));
        const auto texts = bodies(message);
        CHECK_FALSE(texts.empty());
        for (const auto& text : texts) {
            CHECK_FALSE(blank(text));
        }
    }
}

void check_shape(const Family& family, const Language& language) {
    for (const Message& message : parse(read_file(catalogue_path(family, language.code)))) {
        INFO(language.code << " / " << message.context << ": " << quoted(message.source));
        const auto texts = bodies(message);
        // A count in the source is a plural message, and every plural message has a count.
        const bool has_count = placeholders(message.source).contains("%n");
        CHECK(has_count == message.numerus);
        if (message.numerus) {
            CHECK(message.forms.size() == language.forms);
        }
        for (const auto& text : texts) {
            INFO("translation: " << quoted(text));
            // The same placeholders as the source: none dropped, none invented.
            CHECK(placeholders(text) == placeholders(message.source));
            // The same markup, tag for tag (the text between the tags is the translation).
            CHECK(tags(text) == tags(message.source));
            if (message.numerus) {
                // The English "(s)" is a hack for the source only.
                CHECK(text.find("(s)") == std::string::npos);
                CHECK(text.find("(S)") == std::string::npos);
            }
            for (const auto term : kKeptTerms) {
                if (carries_term(message.source, term)) {
                    INFO("kept term " << term);
                    CHECK(carries_term(text, term));
                }
            }
        }
    }
}

// A long sentence carried over unchanged is a message nobody translated.
void check_not_copied(const Family& family, const Language& language) {
    for (const Message& message : parse(read_file(catalogue_path(family, language.code)))) {
        // A sentence: four words or more, one of which starts in lower case. A name, a label in
        // capitals or a format of placeholders ("ARINC Data Loader %1", "%1 (%2, ID 0x%3)") may
        // stand as it is.
        const auto words = static_cast<std::size_t>(std::ranges::count(message.source, ' ') + 1);
        bool lower_case_word = false;
        for (std::size_t i = 0; i < message.source.size(); ++i) {
            const bool word_start = i == 0 || message.source[i - 1] == ' ';
            lower_case_word =
                lower_case_word ||
                (word_start && std::islower(static_cast<unsigned char>(message.source[i])));
        }
        if (words < 4 || !lower_case_word) {
            continue;
        }
        INFO(language.code << " / " << message.context << ": " << quoted(message.source));
        for (const auto& text : bodies(message)) {
            CHECK(text != message.source);
        }
    }
}

using Key = std::tuple<std::string, std::string, std::string>;  // context, source, comment

[[nodiscard]] std::set<Key> keys(const Family& family, std::string_view code,
                                 bool numerus_only = false) {
    std::set<Key> found;
    for (const Message& message : parse(read_file(catalogue_path(family, code)))) {
        if (!numerus_only || message.numerus) {
            found.emplace(message.context, message.source, message.comment);
        }
    }
    return found;
}

}  // namespace

TEST_CASE("every message of every catalogue is finished, translated and not left vanished",
          "[translations]") {
    for (const Family& family : kFamilies) {
        for (const Language& language : kLanguages) {
            check_finished(family, language);
        }
    }
}

TEST_CASE("placeholders, plural forms, markup and identifiers survive in every catalogue",
          "[translations]") {
    for (const Family& family : kFamilies) {
        for (const Language& language : kLanguages) {
            check_shape(family, language);
        }
    }
}

TEST_CASE("no sentence is carried over from English unchanged", "[translations]") {
    for (const Family& family : kFamilies) {
        for (const Language& language : kLanguages) {
            check_not_copied(family, language);
        }
    }
}

TEST_CASE("the seven languages of a family hold the same messages", "[translations]") {
    // Every catalogue is generated from one extraction, so a difference is a hand edit or a file
    // that missed the last lupdate rather than anything about the language.
    for (const Family& family : kFamilies) {
        const auto first = keys(family, kLanguages.front().code);
        REQUIRE_FALSE(first.empty());
        for (const Language& language : kLanguages) {
            const auto here = keys(family, language.code);
            for (const auto& [context, source, comment] : first) {
                INFO(family.name << ": " << kLanguages.front().code << " has a message "
                                 << language.code << " does not: " << context << " / "
                                 << quoted(source));
                CHECK(here.contains({context, source, comment}));
            }
            for (const auto& [context, source, comment] : here) {
                INFO(family.name << ": " << language.code << " has a message "
                                 << kLanguages.front().code << " does not: " << context << " / "
                                 << quoted(source));
                CHECK(first.contains({context, source, comment}));
            }
        }
    }
}

// English has no catalogue of its own for ordinary strings (the source is English), but a plural
// message needs one: Qt takes the form a count asks for from a catalogue, and the source holds only
// the "(s)" hack. The English file is that catalogue - the plural messages of the family, two
// forms each, written by the people who wrote the source - and nothing else.
TEST_CASE("the English plural catalogues hold every plural message, finished", "[translations]") {
    for (const Family& family : kFamilies) {
        const auto english = parse(read_file(catalogue_path(family, "en")));
        std::set<Key> in_english;
        for (const Message& message : english) {
            INFO(family.name << " en / " << message.context << ": " << quoted(message.source));
            CHECK(message.numerus);
            CHECK_FALSE(marked(message, "unfinished"));
            CHECK_FALSE(marked(message, "vanished"));
            CHECK_FALSE(marked(message, "obsolete"));
            // English has two forms: one, and every other count including 0.
            CHECK(message.forms.size() == 2);
            for (const auto& form : message.forms) {
                CHECK_FALSE(blank(form));
                CHECK(placeholders(form) == placeholders(message.source));
                CHECK(form.find("(s)") == std::string::npos);
            }
            in_english.emplace(message.context, message.source, message.comment);
        }
        // The same plural messages as the other languages hold, so a `%n` message added to a
        // window is added to English's file as well.
        const auto elsewhere = keys(family, kLanguages.front().code, true);
        CHECK(in_english == elsewhere);
    }
}

TEST_CASE("a catalogue is read the way the rules assume", "[translations]") {
    const std::string text = R"(<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.1" language="fr_FR">
<context>
    <name>Demo</name>
    <message>
        <location filename="../a.qml" line="3"/>
        <source>Open &quot;%1&quot;</source>
        <translation type="unfinished"></translation>
    </message>
    <message numerus="yes">
        <location line="+4"/>
        <source>%n file(s) in %1</source>
        <translation>
            <numerusform>%n fichier dans %1</numerusform>
            <numerusform>%n fichiers dans %1</numerusform>
        </translation>
    </message>
</context>
</TS>
)";
    const auto messages = parse(text);
    REQUIRE(messages.size() == 2);
    CHECK(messages[0].context == "Demo");
    CHECK(messages[0].source == "Open \"%1\"");
    CHECK(marked(messages[0], "unfinished"));
    CHECK(blank(messages[0].translation));
    CHECK(messages[1].numerus);
    REQUIRE(messages[1].forms.size() == 2);
    CHECK(messages[1].forms[1] == "%n fichiers dans %1");
    CHECK(placeholders(messages[1].source) == std::set<std::string>{"%1", "%n"});
    CHECK(tags("<a href=\"x\">go</a>") == std::vector<std::string>{"<a href=\"x\">", "</a>"});
    CHECK(carries_term("le THW_ID_POS est", "THW_ID_POS"));
    CHECK_FALSE(carries_term("le THW_ID_POSX est", "THW_ID_POS"));
}
