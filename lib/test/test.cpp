/*  ========================================================================
    Copyright (C) 2020 Eaton
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
    ========================================================================
*/

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

#include "fty_asset_dto.h"

using namespace fty;

TEST_CASE("Create fty::Asset object")
{
    REQUIRE_NOTHROW([&]()
    { 
        Asset asset;
        
    }());
    
}

TEST_CASE("Equality check - good case")
{
    Asset asset;
    asset.setInternalName("dc-0");
    asset.setAssetStatus(AssetStatus::Nonactive);
    asset.setAssetType(TYPE_DEVICE);
    asset.setAssetSubtype(SUB_UPS);
    asset.setParentIname("abc123");
    asset.setExtEntry("testKey", "testValue");
    asset.setPriority(4);

    Asset asset2;
    asset2 = asset;
    
    REQUIRE(asset == asset2);
    
}

TEST_CASE("Equality check - bad case")
{
    Asset asset;
    asset.setInternalName("dc-0");
    asset.setAssetStatus(AssetStatus::Nonactive);
    asset.setAssetType(TYPE_DEVICE);
    asset.setAssetSubtype(SUB_UPS);
    asset.setParentIname("abc123");
    asset.setExtEntry("testKey", "testValue");
    asset.setPriority(4);

    Asset asset2;
    asset2 = asset;

    asset2.setParentIname("wrong-name");
    
    REQUIRE(asset != asset2);
    
}

TEST_CASE("Priority set")
{
    FullAsset asset;
    CHECK_NOTHROW(asset.setPriority(1));
    CHECK(asset.getPriority() == 1);
    CHECK_NOTHROW(asset.setPriority("1"));
    CHECK(asset.getPriority() == 1);
    CHECK_NOTHROW(asset.setPriority("P1"));
    CHECK(asset.getPriority() == 1);
}
