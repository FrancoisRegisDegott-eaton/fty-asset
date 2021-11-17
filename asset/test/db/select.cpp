#include "asset/asset-db.h"
#include "asset/asset-db2.h"
#include <catch2/catch.hpp>
#include <fty_common_asset_types.h>
#include <fty_common_db_connection.h>
#include <iostream>
#include <test-db/sample-db.h>

TEST_CASE("Select asset")
{
    fty::SampleDb db(R"(
        items:
          - type     : Ups
            name     : device
            ext-name : Device name
    )");

    auto devId = db.idByName("device");

    fty::db::Connection conn;

    fty::asset::db::AssetElement gr;
    gr.name     = "MyGroup";
    gr.status   = "active";
    gr.priority = 1;
    gr.typeId   = persist::type_to_typeid("group");

    {
        auto ret = fty::asset::db::insertIntoAssetElement(conn, gr, true);
        if (!ret) {
            FAIL(ret.error());
        }
        REQUIRE(*ret > 0);
        gr.id = *ret;
    }

    {
        auto ret = fty::asset::db::insertElementIntoGroups(conn, {gr.id}, devId);
        if (!ret) {
            FAIL(ret.error());
        }
        REQUIRE(*ret > 0);
    }


    // selectAssetElementByName
    {
        auto res = fty::asset::db::selectAssetElementByName("device");
        if (!res) {
            FAIL(res.error());
        }
        REQUIRE(res);
        CHECK(res->id == devId);
        CHECK(res->name == "device");
        CHECK(res->status == "active");
        CHECK(res->priority == 1);
        CHECK(res->subtypeId == persist::subtype_to_subtypeid("ups"));
        CHECK(res->typeId == persist::type_to_typeid("device"));
    }

    // selectAssetElementByName/wrong
    {
        auto res = fty::asset::db::selectAssetElementByName("mydevice");
        CHECK(!res);
        CHECK(res.error() == "Element 'mydevice' not found.");
    }

    // selectAssetElementWebById
    {
        auto res = fty::asset::db::selectAssetElementWebById(devId);
        if (!res) {
            FAIL(res.error());
        }
        REQUIRE(res);
        CHECK(res->id == devId);
        CHECK(res->name == "device");
        CHECK(res->status == "active");
        CHECK(res->priority == 1);
        CHECK(res->subtypeId == persist::subtype_to_subtypeid("ups"));
        CHECK(res->typeId == persist::type_to_typeid("device"));
    }

    // selectAssetElementWebById/wrong
    {
        auto res = fty::asset::db::selectAssetElementWebById(uint32_t(-1));
        CHECK(!res);
        CHECK(res.error() == fmt::format("Element '{}' not found.", uint32_t(-1)));
    }

    // selectExtAttributes
    {
        auto res = fty::asset::db::selectExtAttributes(devId);
        if (!res) {
            FAIL(res.error());
        }
        REQUIRE(res);
        CHECK(res->size() == 1);
        CHECK((*res)["name"].value == "Device name");
        CHECK((*res)["name"].readOnly == true);
    }

    // selectAssetElementGroups
    {
        auto res = fty::asset::db::selectAssetElementGroups(devId);
        if (!res) {
            FAIL(res.error());
        }
        REQUIRE(res);
        CHECK(res->size() == 1);
        CHECK((*res)[gr.id] == "MyGroup");
    }

    // selectAssetElementGroups/wrong
    {
        auto res = fty::asset::db::selectAssetElementGroups(uint32_t(-1));
        if (!res) {
            FAIL(res.error());
        }
        REQUIRE(res);
        CHECK(res->size() == 0);
    }

    // selectAssetElementSuperParent
    {
        auto res = fty::asset::db::selectAssetElementSuperParent(devId, [](const auto& /*row*/) {
        });
        if (!res) {
            FAIL(res.error());
        }
        REQUIRE(res);
    }

    // countKeytag
    {
        auto res = fty::asset::db::countKeytag("name", "Device name");
        if (!res) {
            FAIL(res.error());
        }
        REQUIRE(res);
        CHECK(*res == 1);
    }

    // selectShortElements
    {
        auto res = fty::asset::db::selectShortElements(persist::type_to_typeid("device"), persist::subtype_to_subtypeid("ups"));
        if (!res) {
            FAIL(res.error());
        }
        REQUIRE(res);
        CHECK(res->size() == 1);
    }

    // Clean up
    {
        auto res = fty::asset::db::deleteAssetGroupLinks(conn, gr.id);
        if (!res) {
            FAIL(res.error());
        }
        CHECK(res);
        CHECK(*res > 0);
    }

    {
        auto res = fty::asset::db::deleteAssetElement(conn, gr.id);
        if (!res) {
            FAIL(res.error());
        }
        REQUIRE(*res > 0);
    }
}

TEST_CASE("Select asset with filters")
{
    std::string ipAddr = "10.130.33.34";

    fty::SampleDb db(R"(
        items:
          - type     : Ups
            name     : device
            ext-name : Device name
            attrs    :
              ip.1: 10.130.33.34
    )");

    auto devId = db.idByName("device");

    REQUIRE(devId);
    CHECK(devId == 1);

    // selectAssetElementWebById with filter
    {
        auto res = fty::asset::db::selectExtAttributes({{"keytag", "ip.1"}, {"value", ipAddr}});;
        if (!res) {
            FAIL(res.error());
        }

        REQUIRE(res);
        CHECK(res->size() == 1);
        CHECK((*res)["ip.1"].value == ipAddr);
    }

    // selectAssetElementWebById without filter
    {
        auto res = fty::asset::db::selectExtAttributes({});
        if (!res) {
            FAIL(res.error());
        }

        REQUIRE(res);
        CHECK(res->size() == 0);
    }

    // selectAssetElementWebById with wrong filter
    {
        auto res = fty::asset::db::selectExtAttributes({{"keytag", "ip.1"}, {"value", "127.0.0.1"}});
        if (!res) {
            FAIL(res.error());
        }

        REQUIRE(res);
        CHECK(res->size() == 0);
    }
}
