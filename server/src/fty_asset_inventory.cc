/*  =========================================================================
    fty_asset_inventory - Inventory server: process inventory ASSET messages and update extended attributes

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
    fty_asset_inventory - Inventory server: process inventory ASSET messages and update extended attributes
@discuss
@end
*/

#include "fty_asset_inventory.h"
#include "asset/dbhelpers.h"

#include <fty_log.h>
#include <fty_proto.h>
#include <malamute.h>

#include <unordered_map>
#include <string>
#include <vector>
#include <iostream>

void
fty_asset_inventory_server (zsock_t *pipe, void *args)
{
    assert(args);

    mlm_client_t *client = mlm_client_new ();
    if (!client) {
        log_error("mlm_client_new failed");
        return;
    }

    zpoller_t *poller = zpoller_new (pipe, mlm_client_msgpipe (client), NULL);
    if (!poller) {
        log_error("zpoller_new failed");
        mlm_client_destroy (&client);
        return;
    }

    zsock_signal (pipe, 0);

    char *actor_name = strdup (static_cast<const char*>(args));
    log_info ("%s:\tStarted", actor_name);

    bool test = false;
    std::unordered_map<std::string, std::string> ext_map_cache;

    while (!zsys_interrupted)
    {
        void *which = zpoller_wait (poller, -1);
        if (which == NULL) {
            if (zpoller_terminated(poller) || zsys_interrupted) {
                break;
            }
        }
        else if (which == pipe) {
            zmsg_t *msg = zmsg_recv (pipe);
            char *cmd = zmsg_popstr (msg);
            log_debug ("%s:\tActor command=%s", actor_name, cmd);

            if (streq (cmd, "$TERM")) {
                log_info ("%s:\tGot $TERM", actor_name);
                zstr_free (&cmd);
                zmsg_destroy (&msg);
                break;
            }
            else
            if (streq (cmd, "CONNECT")) {
                char* endpoint = zmsg_popstr (msg);
                int rv = mlm_client_connect (client, endpoint, 1000, actor_name);
                if (rv == -1) {
                    log_error ("%s:\tCan't connect to malamute endpoint '%s'", actor_name, endpoint);
                }
                zstr_free (&endpoint);
                zsock_signal (pipe, 0);
            }
            else
            if (streq (cmd, "PRODUCER")) {
                char* stream = zmsg_popstr (msg);
                int rv = mlm_client_set_producer (client, stream);
                if (rv == -1) {
                    log_error ("%s:\tCan't set producer on stream '%s'", actor_name, stream);
                }
                zstr_free (&stream);
                zsock_signal (pipe, 0);
            }
            else
            if (streq (cmd, "CONSUMER")) {
                char* stream = zmsg_popstr (msg);
                char* pattern = zmsg_popstr (msg);
                test = streq (stream, "ASSETS-TEST");
                int rv = mlm_client_set_consumer (client, stream, pattern);
                if (rv == -1) {
                    log_error ("%s:\tCan't set consumer on stream '%s', '%s'", actor_name, stream, pattern);
                }
                zstr_free (&pattern);
                zstr_free (&stream);
                zsock_signal (pipe, 0);
            }
            else {
                log_info ("%s:\tUnhandled command %s", actor_name, cmd);
            }
            zstr_free (&cmd);
            zmsg_destroy (&msg);
        }
        else if (which == mlm_client_msgpipe (client)) {
            zmsg_t *msg = mlm_client_recv (client);
            fty_proto_t *proto = fty_proto_decode (&msg);
            zmsg_destroy (&msg);

            if (proto) {
                std::string device_name(fty_proto_name (proto));
                const char *operation = fty_proto_operation(proto);

                if (streq (operation, "inventory")) {
                    zhash_t *ext = fty_proto_ext (proto);
                    int rv = process_insert_inventory (device_name, ext, true, ext_map_cache, test);
                    if (rv != 0) {
                        log_error ("Could not insert inventory data into DB");
                    }
                } else if (streq (operation, "delete")) {
                    //  Vacuum the cache
                    //  The keys are formatted as asset_name:keytag[01]
                    device_name.append(":");
                    for (auto it = ext_map_cache.begin(); it != ext_map_cache.end(); ) {
                        if (it->first.compare(0, device_name.size(), device_name) == 0) {
                            it = ext_map_cache.erase(it);
                        } else {
                            ++it;
                        }
                    }
                }
            }
            fty_proto_destroy (&proto);
        }
    }

    zstr_free (&actor_name);
    zpoller_destroy (&poller);
    mlm_client_destroy (&client);
}

//  --------------------------------------------------------------------------
//  Self test of this class

void
fty_asset_inventory_test (bool /*verbose*/)
{
    std::cout << " * fty_asset_inventory:" << std::endl;

    //  Test #1: Simple create/destroy test
    {
        log_debug ("fty-asset-server-test:Test #1");
        zactor_t *self = zactor_new (fty_asset_inventory_server, static_cast<void*>( const_cast<char*>( "asset-inventory-test")));
        zclock_sleep (200);
        zactor_destroy (&self);
        log_info ("fty-asset-server-test:Test #1: OK");
    }
    static const char* endpoint = "inproc://fty_asset_inventory_test";

    zactor_t *server = zactor_new (mlm_server, static_cast<void*>( const_cast<char*>( "Malamute")));
    assert ( server != NULL );
    zstr_sendx (server, "BIND", endpoint, NULL);

    mlm_client_t *ui = mlm_client_new ();
    mlm_client_connect (ui, endpoint, 5000, "fty-asset-inventory-ui");
    mlm_client_set_producer (ui, "ASSETS-TEST");

    zactor_t *inventory_server = zactor_new (fty_asset_inventory_server, static_cast<void*>( const_cast<char*>("asset-inventory-test")));
    zstr_sendx (inventory_server, "CONNECT", endpoint, NULL);
    zsock_wait (inventory_server);
    zstr_sendx (inventory_server, "CONSUMER", "ASSETS-TEST", "inventory@.*", NULL);
    zsock_wait (inventory_server);

    // Test #2: create inventory message and process it
    {
        log_debug ("fty-asset-server-test:Test #2");
        zmsg_t *msg = fty_proto_encode_asset (
                NULL,
                "MyDC",
                "inventory",
                NULL);
        int rv = mlm_client_send (ui, "inventory@dc-1", &msg);
        assert (rv == 0);
        zclock_sleep (200);
        log_info ("fty-asset-server-test:Test #2: OK");
    }

    zactor_destroy (&inventory_server);
    mlm_client_destroy (&ui);
    zactor_destroy (&server);

    std::cout << "fty_asset_inventory: OK" << std::endl;
}
