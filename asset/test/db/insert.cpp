#include "asset/asset-db.h"
#include <fty_common_db_connection.h>
#include <catch2/catch.hpp>
#include <fty_common_asset_types.h>
#include <test-db/sample-db.h>

TEST_CASE("Asset/insert")
{
    fty::SampleDb db("");

    fty::db::Connection conn;

    fty::asset::db::AssetElement el;
    el.name      = "device";
    el.status    = "active";
    el.priority  = 1;
    el.subtypeId = persist::subtype_to_subtypeid("ups");
    el.typeId    = persist::type_to_typeid("device");

    auto ret = fty::asset::db::insertIntoAssetElement(conn, el, true);
    if (!ret) {
        FAIL(ret.error());
    }
    REQUIRE(*ret > 0);
    el.id = *ret;

    SECTION("Update")
    {
        auto res = fty::asset::db::updateAssetElement(conn, el.id, 0, "active", 2, "tag");
        if (!res) {
            FAIL(res.error());
        }
        CHECK(*res > 0);
    }

    SECTION("Links")
    {
        fty::asset::db::AssetElement el2;
        el2.name      = "device2";
        el2.status    = "active";
        el2.priority  = 1;
        el2.subtypeId = persist::subtype_to_subtypeid("row");
        el2.typeId    = persist::type_to_typeid("device");

        auto lret = fty::asset::db::insertIntoAssetElement(conn, el2, true);
        if (!lret) {
            FAIL(lret.error());
        }
        CHECK(*lret > 0);
        if (lret) {
            el2.id = *lret;

            fty::asset::db::AssetLink link;
            link.dest = el2.id;
            link.src  = el.id;
            link.type = 1;

            auto res = fty::asset::db::insertIntoAssetLink(conn, link);
            if (!res) {
                FAIL(res.error());
            }
            CHECK(*res > 0);

            if (res) {
                auto res1 = fty::asset::db::selectAssetDeviceLinksSrc(el.id);
                if (!res1) {
                    FAIL(res1.error());
                }
                CHECK(res1->size() > 0);
                CHECK((*res1)[0] == el2.id);

                SECTION("selectAssetDeviceLinksTo")
                {
                    auto res2 = fty::asset::db::selectAssetDeviceLinksTo(el2.id, 1);
                    if (!res2) {
                        FAIL(res2.error());
                    }
                    REQUIRE(res2);
                    CHECK(res2->size() == 1);
                }

                auto res2 = fty::asset::db::deleteAssetLinksTo(conn, el2.id);
                if (!res2) {
                    FAIL(res2.error());
                }
                CHECK(*res2 > 0);
            }

            auto ret2 = fty::asset::db::deleteAssetElement(conn, el2.id);
            if (!ret2) {
                FAIL(ret2.error());
            }
            REQUIRE(*ret2 > 0);
        }
    }

    auto ret2 = fty::asset::db::deleteAssetElement(conn, el.id);
    if (!ret2) {
        FAIL(ret2.error());
    }
    REQUIRE(*ret2 > 0);
}

TEST_CASE("Asset/Wrong parent")
{
    fty::SampleDb db("");
    fty::db::Connection conn;

    fty::asset::db::AssetElement el;
    el.name      = "device";
    el.status    = "active";
    el.priority  = 1;
    el.subtypeId = persist::subtype_to_subtypeid("ups");
    el.typeId    = persist::type_to_typeid("device");
    el.parentId  = uint32_t(-1);

    auto ret = fty::asset::db::insertIntoAssetElement(conn, el, true);
    REQUIRE(!ret);
    CHECK(ret.error().find("a foreign key constraint fails") != std::string::npos);
}

TEST_CASE("Asset/Wrong type")
{
    fty::SampleDb db("");

    fty::db::Connection conn;

    fty::asset::db::AssetElement el;
    el.name      = "device";
    el.status    = "active";
    el.priority  = 1;
    el.subtypeId = persist::subtype_to_subtypeid("ups");

    auto ret = fty::asset::db::insertIntoAssetElement(conn, el, true);
    REQUIRE(!ret);
    CHECK("0 value of element_type_id is not allowed" == ret.error());
}

TEST_CASE("Asset/Wrong name1")
{
    fty::SampleDb db("");

    fty::db::Connection conn;

    fty::asset::db::AssetElement el;
    el.name      = "device_1";
    el.status    = "active";
    el.priority  = 1;
    el.typeId    = persist::type_to_typeid("device");
    el.subtypeId = persist::subtype_to_subtypeid("ups");

    auto ret = fty::asset::db::insertIntoAssetElement(conn, el, true);
    REQUIRE(!ret);
    CHECK("wrong element name" == ret.error());
}

TEST_CASE("Asset/Wrong name2")
{
    fty::SampleDb db("");

    fty::db::Connection conn;

    fty::asset::db::AssetElement el;
    el.name      = "device@";
    el.status    = "active";
    el.priority  = 1;
    el.typeId    = persist::type_to_typeid("device");
    el.subtypeId = persist::subtype_to_subtypeid("ups");

    auto ret = fty::asset::db::insertIntoAssetElement(conn, el, true);
    REQUIRE(!ret);
    CHECK("wrong element name" == ret.error());
}

TEST_CASE("Asset/Wrong name3")
{
    fty::SampleDb db("");

    fty::db::Connection conn;

    fty::asset::db::AssetElement el;
    el.name      = "device%";
    el.status    = "active";
    el.priority  = 1;
    el.typeId    = persist::type_to_typeid("device");
    el.subtypeId = persist::subtype_to_subtypeid("ups");

    auto ret = fty::asset::db::insertIntoAssetElement(conn, el, true);
    REQUIRE(!ret);
    CHECK("wrong element name" == ret.error());
}

TEST_CASE("Monitor")
{
    fty::SampleDb db("");

    fty::db::Connection conn;
    auto type = fty::asset::db::selectMonitorDeviceTypeId(conn, "ups");
    if (!type) {
        FAIL(type.error());
    }
    CHECK(*type > 0);

    auto mon = fty::asset::db::insertIntoMonitorDevice(conn, *type, "mon");
    if (!mon) {
        FAIL(mon.error());
    }
    REQUIRE(*mon > 0);

    fty::asset::db::AssetElement el;
    el.name      = "device";
    el.status    = "active";
    el.priority  = 1;
    el.subtypeId = persist::subtype_to_subtypeid("ups");
    el.typeId    = persist::type_to_typeid("device");

    auto ret = fty::asset::db::insertIntoAssetElement(conn, el, true);
    if (!ret) {
        FAIL(ret.error());
    }
    REQUIRE(*ret > 0);
    el.id = *ret;

    auto res =fty::asset::db::insertIntoMonitorAssetRelation(conn, *mon, el.id);
    if (!res) {
        FAIL(ret.error());
    }
    REQUIRE(*ret > 0);

    auto del = fty::asset::db::deleteMonitorAssetRelationByA(conn, el.id);
    if (!del) {
        FAIL(del.error());
    }
    REQUIRE(*del > 0);

    auto ret2 = fty::asset::db::deleteAssetElement(conn, el.id);
    if (!ret2) {
        FAIL(ret2.error());
    }
    REQUIRE(*ret2 > 0);
}

TEST_CASE("Asset/UTF-8")
{
    fty::SampleDb db("");

    fty::db::Connection conn;

    fty::asset::db::AssetElement el;
    el.name      = "á";
    el.status    = "active";
    el.priority  = 1;
    el.typeId    = persist::type_to_typeid("device");
    el.subtypeId = persist::subtype_to_subtypeid("ups");

    auto ret = fty::asset::db::insertIntoAssetElement(conn, el, true);
    REQUIRE(ret);

    auto check = fty::asset::db::selectAssetElementWebById(*ret);
    REQUIRE(check);
    CHECK(check->name == "á");

    auto ret2 = fty::asset::db::deleteAssetElement(conn, check->id);
    if (!ret2) {
        FAIL(ret2.error());
    }
    REQUIRE(*ret2 > 0);
}
