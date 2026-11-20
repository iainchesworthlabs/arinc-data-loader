// SPDX-License-Identifier: GPL-3.0-or-later
// What the operator's settings become in the engine's configuration. No network.

#include "adl/mock/settings.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>

using namespace adl::mock;
using adl::arinc615a::Address;
using adl::arinc615a::StatusCode;

TEST_CASE("the default settings make the default engine configuration", "[mock][settings]")
{
    const Config config = make_config(TargetSettings{});
    const Config defaults;
    CHECK(config.thw == "HNPFMS_L");
    CHECK(config.thw == defaults.thw);
    CHECK(config.find_identity.hardware_id == "HNPFMS");
    CHECK(config.find_identity.type_name == "FMS");
    CHECK(config.find_identity.position == "L");
    CHECK(config.find_identity.literal_name == "FMS LEFT");
    CHECK(config.find_identity.manufacturer_code == "HNP");
    CHECK(config.tftp_port == 59);
    CHECK(config.find_port == 1001);
    CHECK(config.bind_address == defaults.bind_address);
    CHECK(config.think_time == std::chrono::seconds(1));
    CHECK(config.status_period == std::chrono::seconds(4));
    CHECK(config.wait_first_seconds == 0);
    CHECK_FALSE(config.wait_always);
    CHECK_FALSE(config.refusal.has_value());
    CHECK(config.drop_status_every == 0);
    CHECK_FALSE(config.corrupt_served);
    CHECK_FALSE(config.wrong_checksum);
    CHECK_FALSE(config.abort_after_statuses.has_value());
    CHECK_FALSE(config.silent);
    CHECK_FALSE(config.garbage_init);
}

TEST_CASE("the identity names the protocol files and answers FIND", "[mock][settings]")
{
    TargetSettings settings;
    settings.hardware_id = "ACMEFMC";
    settings.type_name = "FMC";
    settings.position = "R";
    settings.literal_name = "FMC RIGHT";
    settings.manufacturer_code = "ACM";
    const Config config = make_config(settings);
    CHECK(config.thw == "ACMEFMC_R");
    CHECK(config.find_identity.hardware_id == "ACMEFMC");
    CHECK(config.find_identity.type_name == "FMC");
    CHECK(config.find_identity.position == "R");
    CHECK(config.find_identity.literal_name == "FMC RIGHT");
    CHECK(config.find_identity.manufacturer_code == "ACM");
}

TEST_CASE("a target with no position has no underscore in its name", "[mock][settings]")
{
    TargetSettings settings;
    settings.position.clear();
    CHECK(make_config(settings).thw == "HNPFMS");
}

TEST_CASE("the ports and the interface carry over", "[mock][settings]")
{
    TargetSettings settings;
    settings.tftp_port = 30959;
    settings.find_port = 30961;
    settings.loopback_only = true;
    const Config config = make_config(settings);
    CHECK(config.tftp_port == 30959);
    CHECK(config.find_port == 30961);
    CHECK(config.bind_address == Address::loopback());
}

TEST_CASE("the pace carries over", "[mock][settings]")
{
    TargetSettings settings;
    settings.think_time_ms = 250;
    settings.status_period_sec = 1;
    const Config config = make_config(settings);
    CHECK(config.think_time == std::chrono::milliseconds(250));
    CHECK(config.status_period == std::chrono::seconds(1));
}

TEST_CASE("the injections carry over", "[mock][settings]")
{
    TargetSettings settings;
    settings.wait_first_seconds = 7;
    settings.wait_every_request = true;
    settings.drop_status_every = 3;
    settings.corrupt_served_file = true;
    settings.wrong_checksum = true;
    settings.abort_after_statuses = 2;
    settings.silent = true;
    const Config config = make_config(settings);
    CHECK(config.wait_first_seconds == 7);
    CHECK(config.wait_always);
    CHECK(config.drop_status_every == 3);
    CHECK(config.corrupt_served);
    CHECK(config.wrong_checksum);
    REQUIRE(config.abort_after_statuses.has_value());
    CHECK(*config.abort_after_statuses == 2);
    CHECK(config.silent);
}

TEST_CASE("a figure that makes no sense is held to what the engine can use", "[mock][settings]")
{
    TargetSettings settings;
    settings.wait_first_seconds = 100000; // a WAIT message carries 16 bits
    settings.drop_status_every = -4;
    settings.abort_after_statuses = -1;
    Config config = make_config(settings);
    CHECK(config.wait_first_seconds == 65535);
    CHECK(config.drop_status_every == 0);
    CHECK_FALSE(config.abort_after_statuses.has_value());

    settings.wait_first_seconds = -5;
    settings.abort_after_statuses = 0; // no count: not "abort after the zeroth"
    config = make_config(settings);
    CHECK(config.wait_first_seconds == 0);
    CHECK_FALSE(config.abort_after_statuses.has_value());
}

TEST_CASE("each operation can be the one that is refused, with 0x1000", "[mock][settings]")
{
    for (const OperationKind kind : {OperationKind::information, OperationKind::upload, OperationKind::download_media,
                                     OperationKind::download_operator})
    {
        TargetSettings settings;
        settings.refuse_operation = kind;
        const Config config = make_config(settings);
        REQUIRE(config.refusal.has_value());
        CHECK(config.refusal->kind == kind);
        CHECK(config.refusal->code == StatusCode::operation_denied);
        CHECK_FALSE(config.refusal->text.empty());
    }
}

TEST_CASE("a refusal can carry 0x1002, and an initialization file can be made invalid", "[mock][settings]")
{
    TargetSettings settings;
    settings.refuse_operation = OperationKind::upload;
    settings.refuse_code = StatusCode::operation_not_supported;
    settings.invalid_init_file = true;
    const Config config = make_config(settings);
    REQUIRE(config.refusal.has_value());
    CHECK(config.refusal->code == StatusCode::operation_not_supported);
    CHECK(config.garbage_init);
}
