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

#include <catch2/catch.hpp>

#include "fty_asset_server.h"
#include "fty_asset_inventory.h"
#include "fty_asset_autoupdate.h"
#include "dns.h"

static bool verbose = false;

TEST_CASE("fty_asset_server_test")
{
    fty_asset_server_test(verbose);
}

TEST_CASE("fty_asset_inventory_test")
{
    fty_asset_inventory_test(verbose);
}

TEST_CASE("fty_asset_autoupdate_test")
{
    fty_asset_autoupdate_test(verbose);
}

TEST_CASE("dns")
{
    dns_test(verbose);
}
