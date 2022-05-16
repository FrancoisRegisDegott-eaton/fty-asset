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

/*
@header
    fty_asset - Agent managing assets
@discuss
@end
*/

#include "fty_asset_library.h"

#include <fty_log.h>
#include <czmq.h>
#include <malamute.h>

#include "fty_asset_autoupdate.h"
#include "fty_asset_server.h"
#include "fty_asset_inventory.h"

#define DEFAULT_LOG_CONFIG "/etc/fty/ftylog.cfg"

static int s_autoupdate_timer (zloop_t * /*loop*/, int /*timer_id*/, void *output)
{
    zstr_send (output, "WAKEUP");
    return 0;
}

static int s_repeat_assets_timer (zloop_t * /*loop*/, int /*timer_id*/, void *output)
{
    zstr_send (output, "REPEAT_ALL");
    return 0;
}

int main (int argc, char *argv [])
{
    const char* endpoint = "ipc://@/malamute";
    ManageFtyLog::setInstanceFtylog("fty-asset", DEFAULT_LOG_CONFIG);
    bool verbose = false;
    int argn;

    for (argn = 1; argn < argc; argn++) {
        if (streq (argv [argn], "--help")
        ||  streq (argv [argn], "-h")) {
            puts ("fty-asset [options] ...");
            puts ("  --verbose / -v         verbose test output");
            puts ("  --help / -h            this information");
            return 0;
        }
        else
        if (streq (argv [argn], "--verbose")
        ||  streq (argv [argn], "-v"))
            verbose = true;
        else {
            printf ("Unknown option: %s\n", argv [argn]);
        }
    }

    log_info ("fty_asset - Agent managing assets");

    if (verbose) {
        ManageFtyLog::getInstanceFtylog()->setVerboseMode();
    }

    log_info("new asset_server");
    zactor_t *asset_server = zactor_new (fty_asset_server, static_cast<void*>( const_cast<char*>("asset-agent")));
    zstr_sendx (asset_server, "CONNECTSTREAM", endpoint, NULL);
    zsock_wait (asset_server);
    zstr_sendx (asset_server, "PRODUCER", "ASSETS", NULL);
    zsock_wait (asset_server);
    zstr_sendx (asset_server, "CONSUMER", "ASSETS", ".*", NULL);
    zsock_wait (asset_server);
    zstr_sendx (asset_server, "CONSUMER", "LICENSING-ANNOUNCEMENTS", ".*", NULL);
    zsock_wait (asset_server);
    zstr_sendx (asset_server, "CONNECTMAILBOX", endpoint, NULL);
    zsock_wait (asset_server);
    zstr_sendx (asset_server, "REPEAT_ALL", NULL);

    log_info("new autoupdate_server");
    zactor_t *autoupdate_server = zactor_new (fty_asset_autoupdate_server, static_cast<void*>( const_cast<char*>("asset-autoupdate")));
    zstr_sendx (autoupdate_server, "CONNECT", endpoint, NULL);
    zsock_wait (autoupdate_server);
    zstr_sendx (autoupdate_server, "PRODUCER", "ASSETS", NULL);
    zsock_wait (autoupdate_server);
    zstr_sendx (autoupdate_server, "ASSET_AGENT_NAME", "asset-agent", NULL);
    //no signal transmitted here!
    zstr_sendx (autoupdate_server, "WAKEUP", NULL);

    log_info("new inventory_server");
    zactor_t *inventory_server = zactor_new (fty_asset_inventory_server, static_cast<void*>( const_cast<char*>("asset-inventory")));
    zstr_sendx (inventory_server, "CONNECT", endpoint, NULL);
    zsock_wait (inventory_server);
    zstr_sendx (inventory_server, "CONSUMER", "ASSETS", ".*", NULL);

    // create regular event for agents
    log_info("new loop");
    zloop_t *loop = zloop_new();

    // timer: send WAKEUP msg to autoupdate_server
    size_t interval_s = 5 * 60;
    zloop_timer (loop, interval_s * 1000, 0, s_autoupdate_timer, autoupdate_server);
    log_info("[WAKEUP] timer interval_s: %zu", interval_s);

    // timer: send REPEAT_ALL msg to asset_server
    // set up how ofter assets should be repeated
    char *repeat_interval = getenv("BIOS_ASSETS_REPEAT");
    interval_s = repeat_interval ? static_cast<size_t>(std::stoi(repeat_interval)) : (60 * 60);
    zloop_timer (loop, interval_s * 1000, 0, s_repeat_assets_timer, asset_server);
    log_info("[REPEAT_ALL] timer interval_s: %zu", interval_s);

    // takes ownership of this thread and waits for interrupt
    log_info("fty-asset started");
    zloop_start (loop);
    zloop_destroy (&loop);

    zactor_destroy (&inventory_server);
    zactor_destroy (&autoupdate_server);
    zactor_destroy (&asset_server);

    log_info("fty-asset ended");
    return EXIT_SUCCESS;
}
