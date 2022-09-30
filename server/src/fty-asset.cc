/*  =========================================================================
    fty_asset - Agent managing assets

    Copyright (C) 2014 - 2020 Eaton

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

// won't compile main() for unit testing
#ifndef _UNIT_TESTS_COMPILATION_

/*
@header
    fty_asset - Agent managing assets
@discuss
@end
*/

#include "fty_asset_server.h"
#include "fty_asset_autoupdate.h"
#include "fty_asset_inventory.h"

#include <fty_log.h>
#include <fty_proto.h>
#include <fty_common.h>
#include <fty_common_mlm.h>
#include <czmq.h>
#include <malamute.h>

#define WAKEUP "WAKEUP"
#define REPEAT_ALL "REPEAT_ALL"

static void s_usage(const char* pname)
{
    printf ("%s [options] ...\n", pname);
    printf ("  --verbose / -v   verbose output\n");
    printf ("  --help / -h      this information\n");
}

static int s_wakeup_timer (zloop_t * /*loop*/, int /*timer_id*/, void *output)
{
    zstr_send (output, WAKEUP);
    return 0;
}

static int s_repeat_all_timer (zloop_t * /*loop*/, int /*timer_id*/, void *output)
{
    zstr_send (output, REPEAT_ALL);
    return 0;
}

int main (int argc, char *argv [])
{
    bool verbose = false;

    for (int argn = 1; argn < argc; argn++) {
        const char* arg = argv [argn];
        if (streq (arg, "--help") || streq (arg, "-h")) {
            s_usage(argv[0]);
            return EXIT_SUCCESS;
        }
        else if (streq (arg, "--verbose") || streq (arg, "-v")) {
            verbose = true;
        }
        else {
            fprintf (stderr, "Unknown option (%s)\n", arg);
            s_usage(argv[0]);
            return EXIT_FAILURE;
        }
    }

    ManageFtyLog::setInstanceFtylog("fty-asset", FTY_COMMON_LOGGING_DEFAULT_CFG);

    log_info("fty-asset starting...");

    if (verbose) {
        ManageFtyLog::getInstanceFtylog()->setVerboseMode();
    }

    log_info("new asset_server");
    zactor_t *asset_server = zactor_new (fty_asset_server, static_cast<void*>( const_cast<char*>("asset-agent")));
    if (!asset_server) {
        log_error("asset_server new failed");
        return EXIT_FAILURE;
    }
    zstr_sendx (asset_server, "CONNECTSTREAM", MLM_ENDPOINT, NULL);
    zsock_wait (asset_server);
    zstr_sendx (asset_server, "PRODUCER", FTY_PROTO_STREAM_ASSETS, NULL);
    zsock_wait (asset_server);
    zstr_sendx (asset_server, "CONSUMER", FTY_PROTO_STREAM_ASSETS, ".*", NULL);
    zsock_wait (asset_server);
    zstr_sendx (asset_server, "CONSUMER", FTY_PROTO_STREAM_LICENSING_ANNOUNCEMENTS, ".*", NULL);
    zsock_wait (asset_server);
    zstr_sendx (asset_server, "CONNECTMAILBOX", MLM_ENDPOINT, NULL);
    zsock_wait (asset_server);
    zstr_sendx (asset_server, REPEAT_ALL, NULL);

    log_info("new autoupdate_server");
    zactor_t *autoupdate_server = zactor_new (fty_asset_autoupdate_server, static_cast<void*>( const_cast<char*>("asset-autoupdate")));
    if (!autoupdate_server) {
        log_error("autoupdate_server new failed");
        zactor_destroy(&asset_server);
        return EXIT_FAILURE;
    }
    zstr_sendx (autoupdate_server, "CONNECT", MLM_ENDPOINT, NULL);
    zsock_wait (autoupdate_server);
    zstr_sendx (autoupdate_server, "PRODUCER", FTY_PROTO_STREAM_ASSETS, NULL);
    zsock_wait (autoupdate_server);
    zstr_sendx (autoupdate_server, "ASSET_AGENT_NAME", AGENT_FTY_ASSET, NULL);
    //no signal transmitted here!
    zstr_sendx (autoupdate_server, WAKEUP, NULL);

    log_info("new inventory_server");
    zactor_t *inventory_server = zactor_new (fty_asset_inventory_server, static_cast<void*>( const_cast<char*>("asset-inventory")));
    if (!inventory_server) {
        log_error("inventory_server new failed");
        zactor_destroy(&autoupdate_server);
        zactor_destroy(&asset_server);
        return EXIT_FAILURE;
    }
    zstr_sendx (inventory_server, "CONNECT", MLM_ENDPOINT, NULL);
    zsock_wait (inventory_server);
    zstr_sendx (inventory_server, "CONSUMER", FTY_PROTO_STREAM_ASSETS, ".*", NULL);

    // create regular event for agents
    log_info("new main_loop");
    zloop_t *main_loop = zloop_new();
    if (!main_loop) {
        log_error("main_loop new failed");
        zactor_destroy(&inventory_server);
        zactor_destroy(&autoupdate_server);
        zactor_destroy(&asset_server);
        return EXIT_FAILURE;
    }

    // timer: send WAKEUP msg to autoupdate_server
    {
        size_t interval_s = 5 * 60;
        zloop_timer (main_loop, interval_s * 1000, 0, s_wakeup_timer, autoupdate_server);
        log_info("[WAKEUP] timer interval_s: %zu", interval_s);
    }

    // timer: send REPEAT_ALL msg to asset_server
    // set up how ofter assets should be repeated
    {
        size_t interval_s = 60 * 60;
        try {
            const char *interval = getenv("BIOS_ASSETS_REPEAT");
            if (interval) {
                interval_s = static_cast<size_t>(std::stoi(interval));
            }
        }
        catch (const std::exception& e) {
            log_warning("env. BIOS_ASSETS_REPEAT is malformed (%s)", e.what());
        }
        zloop_timer (main_loop, interval_s * 1000, 0, s_repeat_all_timer, asset_server);
        log_info("[REPEAT_ALL] timer interval_s: %zu", interval_s);
    }

    log_info("fty-asset started");

    // takes ownership of this thread and waits for interrupt
    zloop_start (main_loop);
    zloop_destroy (&main_loop);

    zactor_destroy (&inventory_server);
    zactor_destroy (&autoupdate_server);
    zactor_destroy (&asset_server);

    log_info("fty-asset ended");

    return EXIT_SUCCESS;
}

#endif //_UNIT_TESTS_COMPILATION_
