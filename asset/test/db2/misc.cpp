#include <asset/asset-db2.h>
#include <catch2/catch.hpp>
#include <fty_common_db_connection.h>
#include <test-db/sample-db.h>
#include <iostream>

#define EXP_CHECK(expected)                                                                                                                \
    if (!expected) {                                                                                                                       \
        FAIL(expected.error());                                                                                                            \
    }

TEST_CASE("DB: misc")
{
    fty::SampleDb db(R"(
        items:
          - type     : Ups
            name     : device
            ext-name : Device name
    )");

    fty::db::Connection conn;

    uint32_t devId = db.idByName("device");

    // nameToAssetId
    {
        auto ret = fty::db::asset::nameToAssetId(conn, "device");
        EXP_CHECK(ret);
        CHECK(*ret == devId);
    }
    {
        auto ret = fty::db::asset::nameToAssetId(conn, "device!@");
        REQUIRE(!ret);
        CHECK(ret.error() == "'device!@' name is not valid");
    }
    {
        auto ret = fty::db::asset::nameToAssetId(conn, "device1");
        REQUIRE(!ret);
        CHECK(ret.error() == "Element 'device1' not found.");
    }

    // idToNameExtName
    {
        auto ret = fty::db::asset::idToNameExtName(conn, devId);
        EXP_CHECK(ret);
        CHECK(ret->name == "device");
        CHECK(ret->extName == "Device name");
    }
    {
        auto ret = fty::db::asset::idToNameExtName(conn, 999);
        REQUIRE(!ret);
        CHECK(ret.error() == "Element '999' not found.");
    }

    // extNameToAssetName
    {
        auto ret = fty::db::asset::extNameToAssetName(conn, "Device name");
        EXP_CHECK(ret);
        CHECK(*ret == "device");
    }
    {
        auto ret = fty::db::asset::extNameToAssetName(conn, "Device1 name");
        REQUIRE(!ret);
        CHECK(ret.error() == "Element 'Device1 name' not found.");
    }

    // nameToExtName
    {
        auto ret = fty::db::asset::nameToExtName(conn, "device");
        EXP_CHECK(ret);
        CHECK(*ret == "Device name");
    }
    {
        auto ret = fty::db::asset::nameToExtName(conn, "device!@");
        REQUIRE(!ret);
        CHECK(ret.error() == "'device!@' name is not valid");
    }
    {
        auto ret = fty::db::asset::nameToExtName(conn, "device1");
        REQUIRE(!ret);
        CHECK(ret.error() == "Element 'device1' not found.");
    }

    // countKeytag
    {
        auto ret = fty::db::asset::countKeytag(conn, "name", "Device name");
        EXP_CHECK(ret);
        CHECK(*ret == 1);
    }
    {
        auto ret = fty::db::asset::countKeytag(conn, "name", "Device name1");
        EXP_CHECK(ret);
        CHECK(*ret == 0);
    }
    {
        auto ret = fty::db::asset::countKeytag(conn, "name1", "Device name");
        EXP_CHECK(ret);
        CHECK(*ret == 0);
    }
    {
        auto ret = fty::db::asset::countKeytag(conn, "name", "Device name", devId);
        EXP_CHECK(ret);
        CHECK(*ret == 1);
    }
    {
        auto ret = fty::db::asset::countKeytag(conn, "name", "Device name1", devId);
        EXP_CHECK(ret);
        CHECK(*ret == 0);
    }
    {
        auto ret = fty::db::asset::countKeytag(conn, "name1", "Device name", devId);
        EXP_CHECK(ret);
        CHECK(*ret == 0);
    }
    {
        // TODO: should return 'not found'
        auto ret = fty::db::asset::countKeytag(conn, "name", "Device name", 999);
        EXP_CHECK(ret);
        CHECK(*ret == 0);
        //CHECK(ret.error() == "Element '999' not found.");
    }
}
