#include "adl/app/address.hpp"
#include "adl/app/events.hpp"
#include "test_classes.hpp"

#include <QTest>

using adl::app::format_ipv4;
using adl::app::parse_ipv4;

void AddressTest::parses_four_octets()
{
    const auto octets = parse_ipv4(QStringLiteral("10.0.0.2"));
    QVERIFY(octets.has_value());
    QCOMPARE((*octets)[0], 10);
    QCOMPARE((*octets)[3], 2);
    QVERIFY(parse_ipv4(QStringLiteral("255.255.255.255")).has_value());
    QVERIFY(parse_ipv4(QStringLiteral("0.0.0.0")).has_value());
}

void AddressTest::refuses_everything_else()
{
    const QStringList bad{{},
                          QStringLiteral("10.0.0"),
                          QStringLiteral("10.0.0.2.3"),
                          QStringLiteral("10.0.0."),
                          QStringLiteral(".0.0.2"),
                          QStringLiteral("256.0.0.1"),
                          QStringLiteral("1000.0.0.1"),
                          QStringLiteral("-1.0.0.1"),
                          QStringLiteral("+1.0.0.1"),
                          QStringLiteral("0x0A.0.0.1"),
                          QStringLiteral("a.b.c.d"),
                          QStringLiteral(" 10.0.0.2"),
                          QStringLiteral("10.0.0.2 "),
                          QStringLiteral("10..0.2"),
                          QStringLiteral("10.0.0.2\n"),
                          QStringLiteral("::1")};
    for (const QString& text : bad)
    {
        QVERIFY2(!parse_ipv4(text).has_value(), qPrintable(text));
    }
}

void AddressTest::formats_what_it_parses()
{
    for (const char* text : {"10.0.0.2", "192.168.1.255", "0.0.0.0", "255.255.255.255"})
    {
        const auto octets = parse_ipv4(QString::fromLatin1(text));
        QVERIFY(octets.has_value());
        QCOMPARE(format_ipv4(*octets), QString::fromLatin1(text));
    }
}

void AddressTest::target_name_is_hardware_id_and_position()
{
    adl::app::FoundTarget target;
    target.hardware_id = QStringLiteral("HNPFMS");
    target.position = QStringLiteral("L");
    QCOMPARE(target.thw_id_pos(), QStringLiteral("HNPFMS_L"));
    target.position.clear();
    QCOMPARE(target.thw_id_pos(), QStringLiteral("HNPFMS"));
}
