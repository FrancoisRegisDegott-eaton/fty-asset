/*  =========================================================================
    fty_asset_selftest.c - run selftests

    Runs all selftests.

    -------------------------------------------------------------------------
    Copyright (C) 2016 - 2020 Eaton

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


    =========================================================================
*/
#include <catch2/catch.hpp>
#include <string.h>

#include "fty_asset_server.h"
#include "fty_asset_autoupdate.h"
#include "fty_asset_inventory.h"
#include "topology_processor.h"
#include "total_power.h"
#include "dns.h"

//  -------------------------------------------------------------------------
//  Run all tests.
//

TEST_CASE("all tests")
{
    struct test_t {
        const char *testname;
        void (*fn) (bool); // function to run the test (or NULL)
    };

    test_t all_tests [] = {
        { "topology_processor_test", topology_processor_test },
        { "total_power_test", total_power_test },
        { "dns_test", dns_test },
        { "fty_asset_server_test", fty_asset_server_test },
        { "fty_asset_autoupdate_test", fty_asset_autoupdate_test },
        { "fty_asset_inventory_test", fty_asset_inventory_test },
        { NULL, NULL }
    };

    bool verbose = true;

    printf ("Running fty-asset-server selftests...\n");

    for (test_t *test = all_tests; test->testname; test++)
    {
        if (test->fn) {
            printf ("Running %s ...\n", test->testname);
            test->fn(verbose);
            printf ("%s done\n", test->testname);
        }
    }

    printf ("Running fty-asset-server selftests done\n");
}
