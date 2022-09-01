#include "asset/asset-manager.h"
#include <catch2/catch.hpp>
#include <test-db/sample-db.h>

TEST_CASE("Import asset")
{
    fty::SampleDb db(R"(
        items:
          - type     : Datacenter
            name     : datacenter
            ext-name : Washington DC
    )");

    static std::string data = R"(name,type,sub_type,location,status,priority,asset_tag,power_source.1,power_plug_src.1,power_input.1,description,ip.1,company,site_name,region,country,address,contact_name,contact_email,contact_phone,u_size,manufacturer,model,serial_no,runtime,installation_date,maintenance_date,maintenance_due,location_u_pos,location_w_pos,end_warranty_date,hostname.1,http_link.1,accordion_is_open,asset_order,logical_asset,max_power,phases.output,port,group.1,id
Room1,room,,Washington DC,active,P1,,,,,,,,,,,,,,,,,,,,,,,,,,,,true,1,,,,,,
Row1,row,,Room1,active,P1,,,,,,,,,,,,,,,,,,,,,,,,,,,,,1,,,,,,
Rack1,rack,,Row1,active,P1,,,,,,,,,,,,,,,42,,,,,,,,,,,,,,1,,,,,,
NewsFeed,device,feed,Washington DC,active,P1,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,1,,,)";

    auto ret = fty::asset::AssetManager::importCsv(data, "dummy", false);
    if (!ret) {
        FAIL(ret.error());
    }
    REQUIRE(ret);

    for (auto iter = ret->rbegin(); iter != ret->rend(); ++iter) {
        if (!iter->second) {
            FAIL(iter->second.error());
        }

        auto el = fty::asset::db::selectAssetElementWebById(*(iter->second));
        std::cerr << *(iter->second) << " " << el->name << std::endl;
        if (auto res = fty::asset::AssetManager::deleteAsset(*el, false); !res) {
            FAIL(res.error());
        }
    }
}

TEST_CASE("Import asset2")
{
    fty::SampleDb db(R"(
        items:
          - type     : Datacenter
            name     : dc
            ext-name : dc
            items:
              - type : Feed
                name : feed
    )");

    // clang-format off
    static std::string data = R"(name,type,sub_type,location,status,priority,asset_tag,power_source.1,power_plug_src.1,power_input.1,description,ip.1,company,site_name,region,country,address,contact_name,contact_email,contact_phone,u_size,manufacturer,model,serial_no,runtime,installation_date,maintenance_date,maintenance_due,location_u_pos,location_w_pos,end_warranty_date,hostname.1,http_link.1,endpoint.1.nut_powercom.secw_credential_id,endpoint.1.nut_snmp.secw_credential_id,endpoint.1.protocol,fast_track,location_type,max_power,upsconf_block,id
UPS-DUMMY,device,ups,dc,active,P3,,feed,,56,,127.0.0.1,,,,,,,,,6,,,,,,,,16,horizontal,,,,,d233a73f-5c15-4a91-b685-27141b91c93e,nut_snmp,,datacenter,,|driver=dummy-ups|port=myups.seq|,)";
    // clang-format on

    auto ret = fty::asset::AssetManager::importCsv(data, "dummy", false);
    if (!ret) {
        FAIL(ret.error());
    }
    REQUIRE(ret);
}

TEST_CASE("Import asset ISO UTF-8")
{
    fty::SampleDb db(R"(
        items:
          - type     : Datacenter
            name     : datacenter
            ext-name : theDC
    )");

    #define ISOUTF8_feedName "NewFeed-你好-à°é"
    std::string data =
        "name,type,sub_type,location,status,priority,asset_tag,power_source.1,power_plug_src.1,power_input.1,description,ip.1,company,site_name,region,country,address,contact_name,contact_email,contact_phone,u_size,manufacturer,model,serial_no,runtime,installation_date,maintenance_date,maintenance_due,location_u_pos,location_w_pos,end_warranty_date,hostname.1,http_link.1,accordion_is_open,asset_order,logical_asset,max_power,phases.output,port,group.1,id\n"
        ISOUTF8_feedName ",device,feed,theDC,active,P1,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,1,,,";

    std::cout << "data: " << data << std::endl;

    auto ret = fty::asset::AssetManager::importCsv(data, "dummy", false);
    if (!ret) {
        FAIL(ret.error());
    }
    REQUIRE(ret);

    for (auto iter = ret->rbegin(); iter != ret->rend(); ++iter) {
        if (!iter->second) {
            FAIL(iter->second.error());
        }

        auto el = fty::asset::db::selectAssetElementWebById(*(iter->second));
        std::cerr << "id: " << *(iter->second) << ", iName: " << el->name << std::endl;

        auto p = fty::asset::db::idToNameExtName(el->id);
        REQUIRE(p);
        REQUIRE(p->second == ISOUTF8_feedName);

        if (auto res = fty::asset::AssetManager::deleteAsset(*el, false); !res) {
            FAIL(res.error());
        }
    }
}
