#include <catch2/catch.hpp>
#include "asset/asset-helpers.h"
#include <test-db/sample-db.h>
#include <iostream>

TEST_CASE("Long names / simple")
{
    fty::SampleDb db(R"(
        items:
            - type     : Datacenter
              name     : datacenter
              ext-name : Data Center
        )");

    {
        auto res = fty::asset::normName("Long Long Long name Long name Long name device name");
        REQUIRE(res);
        CHECK(*res == "Long Long Long name Long name Long name device nam");
    }
}

TEST_CASE("Long names / already exists")
{
    fty::SampleDb db(R"(
        items:
            - type     : Datacenter
              name     : datacenter
              ext-name : Data Center
              items :
                  - type     : Feed
                    name     : feed
                  - type     : Server
                    name     : dev1
                    ext-name : Long Long Long name Long name Long name device nam
        )");

    {
        auto res = fty::asset::normName("Long Long Long name Long name Long name device name");
        REQUIRE(res);
        CHECK(*res == "Long Long Long name Long name Long name device n~1");
    }
}

TEST_CASE("Long names / already exists 2")
{
    fty::SampleDb db(R"(
        items:
            - type     : Datacenter
              name     : datacenter
              ext-name : Data Center
              items :
                  - type     : Feed
                    name     : feed
                  - type     : Server
                    name     : dev1
                    ext-name : Long Long Long name Long name Long name device nam
                  - type     : Server
                    name     : dev2
                    ext-name : Long Long Long name Long name Long name device n~1
        )");

    {
        auto res = fty::asset::normName("Long Long Long name Long name Long name device name");
        REQUIRE(res);
        CHECK(*res == "Long Long Long name Long name Long name device n~2");
    }
}

TEST_CASE("Long names / already exists 3")
{
    fty::SampleDb db(R"(
        items:
            - type     : Datacenter
              name     : datacenter
              ext-name : Data Center
              items :
                  - type     : Feed
                    name     : feed
                  - type     : Server
                    name     : dev1
                    ext-name : Long Long Long name Long name Long name device nam
                  - type     : Server
                    name     : dev2
                    ext-name : Long Long Long name Long name Long name device ~10
        )");

    {
        auto res = fty::asset::normName("Long Long Long name Long name Long name device name");
        REQUIRE(res);
        CHECK(*res == "Long Long Long name Long name Long name device ~11");
    }
}
