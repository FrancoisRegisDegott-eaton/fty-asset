#include <asset/asset-db2.h>
#include <catch2/catch.hpp>
#include <fty_common_asset_types.h>
#include <fty_common_db_connection.h>
#include <iostream>
#include <test-db/sample-db.h>

#define EXP_CHECK(expected)                                                                                                                \
    if (!expected) {                                                                                                                       \
        FAIL(expected.error());                                                                                                            \
    }

namespace asset = fty::db::asset;

static void testItemByName(fty::db::Connection& conn)
{
    {
        auto ret = asset::select::item(conn, "UpsWoDC");
        EXP_CHECK(ret);
        CHECK(ret->name == "UpsWoDC");
        CHECK(ret->status == "active");
        CHECK(ret->priority == 1);
        CHECK(ret->subtypeId == persist::UPS);
        CHECK(ret->typeId == persist::DEVICE);
    }
    {
        // invalid name
        auto ret = asset::select::item(conn, "UpsWoDC!@");
        REQUIRE(!ret);
        CHECK(ret.error() == "name 'UpsWoDC!@' is not valid");
    }
    {
        // not existsing element
        auto ret = asset::select::item(conn, "UpsWoDC1");
        REQUIRE(!ret);
        CHECK(ret.error() == "Element 'UpsWoDC1' not found.");
    }
    {
        // by ext name
        auto ret = asset::select::item(conn, "Ups Wo DC");
        EXP_CHECK(ret);
        CHECK(ret->name == "UpsWoDC");
    }
    {
        // by name, but should search by ext name
        auto ret = asset::select::item(conn, "UpsWoDC", true);
        REQUIRE(!ret);
        CHECK(ret.error() == "Element 'UpsWoDC' not found.");
    }
    {
        // by ext name
        auto ret = asset::select::item(conn, "Ups Wo DC", true);
        EXP_CHECK(ret);
        CHECK(ret->name == "UpsWoDC");
    }
}

static void testItemsList(fty::db::Connection& conn)
{
    std::vector<asset::AssetItem> items;

    auto func = [&](const fty::db::Row& row) {
        asset::AssetItem item;
        item.name      = row.get("name");
        item.id        = row.get<uint32_t>("id");
        item.typeId    = row.get<uint16_t>("typeId");
        item.subtypeId = row.get<uint16_t>("subTypeId");

        items.emplace_back(item);
    };

    {
        items.clear();
        auto ret = asset::select::items(conn, func);
        EXP_CHECK(ret);
        CHECK(items.size() == 3);
        CHECK(items[0].name == "datacenter");
        CHECK(items[0].typeId == persist::DATACENTER);
        CHECK(items[0].subtypeId == persist::N_A);
        CHECK(items[1].name == "device");
        CHECK(items[1].typeId == persist::DEVICE);
        CHECK(items[1].subtypeId == persist::UPS);
        CHECK(items[2].name == "UpsWoDC");
        CHECK(items[2].typeId == persist::DEVICE);
        CHECK(items[2].subtypeId == persist::UPS);
    }
    {
        items.clear();

        asset::select::Filter filter;
        filter.types.push_back(persist::DEVICE);

        auto ret = asset::select::items(conn, func, filter);
        EXP_CHECK(ret);

        CHECK(items.size() == 2);
        CHECK(items[0].name == "device");
        CHECK(items[1].name == "UpsWoDC");
    }
    {
        items.clear();

        asset::select::Filter filter;
        filter.types.push_back(persist::DEVICE);
        filter.subtypes.push_back(persist::UPS);

        auto ret = asset::select::items(conn, func, filter, {});
        EXP_CHECK(ret);

        CHECK(items.size() == 2);
        CHECK(items[0].name == "device");
        CHECK(items[1].name == "UpsWoDC");
    }
    {
        items.clear();

        asset::select::Filter filter;
        filter.status = "active";

        auto ret = asset::select::items(conn, func, filter);
        EXP_CHECK(ret);

        CHECK(items.size() == 3);
        CHECK(items[0].name == "datacenter");
        CHECK(items[1].name == "device");
        CHECK(items[2].name == "UpsWoDC");
    }
    {
        items.clear();

        asset::select::Filter filter;
        filter.status = "nonactive";

        auto ret = asset::select::items(conn, func, filter, {});
        EXP_CHECK(ret);

        CHECK(items.size() == 0);
    }
    {
        items.clear();

        asset::select::Order order;
        order.field = "name";
        order.dir   = asset::select::Order::Dir::Asc;

        auto ret = asset::select::items(conn, func, {}, order);
        EXP_CHECK(ret);

        CHECK(items.size() == 3);
        CHECK(items[0].name == "datacenter");
        CHECK(items[1].name == "device");
        CHECK(items[2].name == "UpsWoDC");
    }
    {
        items.clear();

        asset::select::Order order;
        order.field = "name";
        order.dir   = asset::select::Order::Dir::Desc;

        auto ret = asset::select::items(conn, func, {}, order);
        EXP_CHECK(ret);

        CHECK(items.size() == 3);
        CHECK(items[0].name == "UpsWoDC");
        CHECK(items[1].name == "device");
        CHECK(items[2].name == "datacenter");
    }
    {
        items.clear();

        asset::select::Order order;
        order.field = "wrong";

        auto ret = asset::select::items(conn, func, {}, order);
        REQUIRE(!ret);
        CHECK(ret.error().find("order field is invalid") != std::string::npos);
    }
}

static void testItemsList2(fty::db::Connection& conn)
{
    {
        auto ret = asset::select::items(conn);
        EXP_CHECK(ret);
        REQUIRE(ret->size() == 3);
        CHECK(ret->at(0).name == "datacenter");
        CHECK(ret->at(0).typeId == persist::DATACENTER);
        CHECK(ret->at(0).subtypeId == persist::N_A);
        CHECK(ret->at(1).name == "device");
        CHECK(ret->at(1).typeId == persist::DEVICE);
        CHECK(ret->at(1).subtypeId == persist::UPS);
        CHECK(ret->at(2).name == "UpsWoDC");
        CHECK(ret->at(2).typeId == persist::DEVICE);
        CHECK(ret->at(2).subtypeId == persist::UPS);
    }
    {
        asset::select::Filter filter;
        filter.types.push_back(persist::DEVICE);

        auto ret = asset::select::items(conn, filter);
        EXP_CHECK(ret);

        REQUIRE(ret->size() == 2);
        CHECK(ret->at(0).name == "device");
        CHECK(ret->at(1).name == "UpsWoDC");
    }
    {
        asset::select::Filter filter;
        filter.types.push_back(persist::DEVICE);
        filter.subtypes.push_back(persist::UPS);

        auto ret = asset::select::items(conn, filter);
        EXP_CHECK(ret);

        REQUIRE(ret->size() == 2);
        CHECK(ret->at(0).name == "device");
        CHECK(ret->at(1).name == "UpsWoDC");
    }
    {
        asset::select::Filter filter;
        filter.status = "active";

        auto ret = asset::select::items(conn, filter);
        EXP_CHECK(ret);

        REQUIRE(ret->size() == 3);
        CHECK(ret->at(0).name == "datacenter");
        CHECK(ret->at(1).name == "device");
        CHECK(ret->at(2).name == "UpsWoDC");
    }
    {
        asset::select::Filter filter;
        filter.status = "nonactive";

        auto ret = asset::select::items(conn, filter);
        EXP_CHECK(ret);
        CHECK(ret->size() == 0);
    }
    {
        asset::select::Order order;
        order.field = "name";
        order.dir   = asset::select::Order::Dir::Asc;

        auto ret = asset::select::items(conn, {}, order);
        EXP_CHECK(ret);

        REQUIRE(ret->size() == 3);
        CHECK(ret->at(0).name == "datacenter");
        CHECK(ret->at(1).name == "device");
        CHECK(ret->at(2).name == "UpsWoDC");
    }
    {
        asset::select::Order order;
        order.field = "name";
        order.dir   = asset::select::Order::Dir::Desc;

        auto ret = asset::select::items(conn, {}, order);
        EXP_CHECK(ret);

        REQUIRE(ret->size() == 3);
        CHECK(ret->at(0).name == "UpsWoDC");
        CHECK(ret->at(1).name == "device");
        CHECK(ret->at(2).name == "datacenter");
    }
    {
        asset::select::Order order;
        order.field = "wrong";

        auto ret = asset::select::items(conn, {}, order);
        REQUIRE(!ret);
        CHECK(ret.error().find("order field is invalid") != std::string::npos);
    }
}

static void testItemExtById(fty::db::Connection& conn, uint32_t id)
{
    {
        auto ret = asset::select::itemExt(conn, id);
        EXP_CHECK(ret);
        CHECK(ret->name == "UpsWoDC");
        CHECK(ret->status == "active");
        CHECK(ret->priority == 1);
        CHECK(ret->subtypeId == persist::UPS);
        CHECK(ret->typeId == persist::DEVICE);
    }
    {
        auto ret = asset::select::itemExt(conn, 999);
        REQUIRE(!ret);
        CHECK(ret.error() == "Element '999' not found.");
    }
}

static void testItemExtByName(fty::db::Connection& conn)
{
    {
        auto ret = asset::select::itemExt(conn, "UpsWoDC");
        EXP_CHECK(ret);
        CHECK(ret->name == "UpsWoDC");
        CHECK(ret->status == "active");
        CHECK(ret->priority == 1);
        CHECK(ret->subtypeId == persist::UPS);
        CHECK(ret->typeId == persist::DEVICE);
    }
    {
        auto ret = asset::select::itemExt(conn, "some name");
        REQUIRE(!ret);
        CHECK(ret.error() == "Element 'some name' not found.");
    }
    {
        auto ret = asset::select::itemExt(conn, "UpsWoDC!!@@");
        REQUIRE(!ret);
        CHECK(ret.error() == "name 'UpsWoDC!!@@' is not valid");
    }
}

static void testItemsByContainer(fty::db::Connection& conn, uint32_t cntId)
{
    std::vector<asset::AssetItem> items;

    auto func = [&](const fty::db::Row& row) {
        asset::AssetItem item;
        item.name      = row.get("name");
        item.id        = row.get<uint32_t>("id");
        item.typeId    = row.get<uint16_t>("typeId");
        item.subtypeId = row.get<uint16_t>("subTypeId");

        items.emplace_back(item);
    };

    {
        items.clear();
        auto ret = asset::select::itemsByContainer(conn, cntId, func);
        EXP_CHECK(ret);
        REQUIRE(items.size() == 1);
        CHECK(items.at(0).name == "device");
    }
    {
        items.clear();
        auto ret = asset::select::itemsByContainer(conn, 999, func);
        EXP_CHECK(ret);
        REQUIRE(items.size() == 0);
    }
    {
        items.clear();
        asset::select::Filter filter;
        filter.types.push_back(persist::DEVICE);
        auto ret = asset::select::itemsByContainer(conn, cntId, func, filter);
        EXP_CHECK(ret);
        REQUIRE(items.size() == 1);
        CHECK(items.at(0).name == "device");
    }
    {
        items.clear();

        asset::select::Filter filter;
        filter.types.push_back(persist::DEVICE);
        filter.subtypes.push_back(persist::UPS);
        filter.status = "active";

        auto ret = asset::select::itemsByContainer(conn, cntId, func, filter);
        EXP_CHECK(ret);
        REQUIRE(items.size() == 1);
        CHECK(items.at(0).name == "device");
    }
    {
        items.clear();

        asset::select::Filter filter;
        filter.types.push_back(persist::DEVICE);
        filter.subtypes.push_back(persist::UPS);
        filter.status = "active";

        asset::select::Order order;
        order.field = "name";

        auto ret = asset::select::itemsByContainer(conn, cntId, func, filter, order);
        EXP_CHECK(ret);
        REQUIRE(items.size() == 1);
        CHECK(items.at(0).name == "device");
    }
    {
        items.clear();

        asset::select::Order order;
        order.field = "name";

        auto ret = asset::select::itemsByContainer(conn, cntId, func, {}, order);
        EXP_CHECK(ret);
        REQUIRE(items.size() == 1);
        CHECK(items.at(0).name == "device");
    }
}

static void testItemsByContainer2(fty::db::Connection& conn, uint32_t cntId)
{
    {
        auto ret = asset::select::itemsByContainer(conn, cntId);
        EXP_CHECK(ret);
        REQUIRE(ret->size() == 1);
        CHECK(ret->at(0).name == "device");
    }
    {
        auto ret = asset::select::itemsByContainer(conn, 999);
        EXP_CHECK(ret);
        REQUIRE(ret->size() == 0);
    }
    {
        asset::select::Filter filter;
        filter.types.push_back(persist::DEVICE);

        auto ret = asset::select::itemsByContainer(conn, cntId, filter);
        EXP_CHECK(ret);
        REQUIRE(ret->size() == 1);
        CHECK(ret->at(0).name == "device");
    }
    {
        asset::select::Filter filter;
        filter.types.push_back(persist::DEVICE);
        filter.subtypes.push_back(persist::UPS);
        filter.status = "active";

        auto ret = asset::select::itemsByContainer(conn, cntId, filter);
        EXP_CHECK(ret);
        REQUIRE(ret->size() == 1);
        CHECK(ret->at(0).name == "device");
    }
    {
        asset::select::Filter filter;
        filter.types.push_back(persist::DEVICE);
        filter.subtypes.push_back(persist::UPS);
        filter.status = "active";

        asset::select::Order order;
        order.field = "name";

        auto ret = asset::select::itemsByContainer(conn, cntId, filter, order);
        EXP_CHECK(ret);
        REQUIRE(ret->size() == 1);
        CHECK(ret->at(0).name == "device");
    }
    {
        asset::select::Order order;
        order.field = "name";

        auto ret = asset::select::itemsByContainer(conn, cntId, {}, order);
        EXP_CHECK(ret);
        REQUIRE(ret->size() == 1);
        CHECK(ret->at(0).name == "device");
    }
}

static void testItemsWithoutContainer(fty::db::Connection& conn)
{
    std::vector<asset::AssetItem> items;

    auto func = [&](const fty::db::Row& row) {
        asset::AssetItem item;
        item.name      = row.get("name");
        item.id        = row.get<uint32_t>("id");
        item.typeId    = row.get<uint16_t>("typeId");
        item.subtypeId = row.get<uint16_t>("subTypeId");

        items.emplace_back(item);
    };

    {
        items.clear();
        auto ret = asset::select::itemsWithoutContainer(conn, func);
        EXP_CHECK(ret);
        REQUIRE(items.size() == 1);
        CHECK(items.at(0).name == "UpsWoDC");
    }
    {
        items.clear();
        auto ret = asset::select::itemsWithoutContainer(conn, func);
        EXP_CHECK(ret);
        REQUIRE(items.size() == 0);
    }
    {
        items.clear();
        asset::select::Filter filter;
        filter.types.push_back(persist::DEVICE);
        auto ret = asset::select::itemsWithoutContainer(conn, func, filter);
        EXP_CHECK(ret);
        REQUIRE(items.size() == 1);
        CHECK(items.at(0).name == "UpsWoDC");
    }
    {
        items.clear();

        asset::select::Filter filter;
        filter.types.push_back(persist::DEVICE);
        filter.subtypes.push_back(persist::UPS);
        filter.status = "active";

        auto ret = asset::select::itemsWithoutContainer(conn, func, filter);
        EXP_CHECK(ret);
        REQUIRE(items.size() == 1);
        CHECK(items.at(0).name == "UpsWoDC");
    }
    {
        items.clear();

        asset::select::Filter filter;
        filter.types.push_back(persist::DEVICE);
        filter.subtypes.push_back(persist::UPS);
        filter.status = "active";

        asset::select::Order order;
        order.field = "name";

        auto ret = asset::select::itemsWithoutContainer(conn, func, filter, order);
        EXP_CHECK(ret);
        REQUIRE(items.size() == 1);
        CHECK(items.at(0).name == "UpsWoDC");
    }
    {
        items.clear();

        asset::select::Order order;
        order.field = "name";

        auto ret = asset::select::itemsWithoutContainer(conn, func, {}, order);
        EXP_CHECK(ret);
        REQUIRE(items.size() == 1);
        CHECK(items.at(0).name == "UpsWoDC");
    }
}


TEST_CASE("DB: select")
{
    fty::SampleDb db(R"(
        items:
            - type     : Datacenter
              name     : datacenter
              ext-name : Data Center
              items:
                  - type     : Ups
                    name     : device
                    ext-name : Device name
            - type     : Ups
              name     : UpsWoDC
              ext-name : Ups Wo DC
    )");

    fty::db::Connection conn;

    testItemByName(conn);
    testItemsList(conn);
    testItemsList2(conn);
    testItemExtById(conn, db.idByName("UpsWoDC"));
    testItemExtByName(conn);
    testItemsByContainer(conn, db.idByName("datacenter"));
    testItemsByContainer2(conn, db.idByName("datacenter"));
    testItemsWithoutContainer(conn);
}
