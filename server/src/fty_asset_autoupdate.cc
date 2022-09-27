/*  =========================================================================
    fty_asset_autoupdate - Asset server, that udates some of asset information on change like IP address in case of DHCP

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
    fty_asset_autoupdate - Asset server, that udpates some of asset information on change like IP address in case of DHCP
@discuss
@end
*/

#include "fty_asset_autoupdate.h"
#include "dns.h" //local_addresses

#include <fty_log.h>
#include <fty_proto.h>
#include <malamute.h>
#include <string>
#include <vector>
#include <iostream>

//  Structure of our class

struct fty_asset_autoupdate_t {
    mlm_client_t *client{nullptr};
    char *name{nullptr};
    char *asset_agent_name{nullptr};
    std::vector<std::string> rcs;
    bool verbose{false};
};

//  --------------------------------------------------------------------------
//  Destroy the fty_asset_autoupdate

static void
fty_asset_autoupdate_destroy (fty_asset_autoupdate_t **self_p)
{
    if (self_p && *self_p) {
        fty_asset_autoupdate_t *self = *self_p;

        zstr_free (&self->name);
        zstr_free (&self->asset_agent_name);
        mlm_client_destroy (&self->client);

        delete self;
        *self_p = NULL;
    }
}

//  --------------------------------------------------------------------------
//  Create a new fty_asset_autoupdate

static fty_asset_autoupdate_t *
fty_asset_autoupdate_new (void)
{
    fty_asset_autoupdate_t *self = new (fty_asset_autoupdate_t); //due to std::vector<>
    if (!self) {
        return NULL;
    }

    self->verbose = false;
    self->client = mlm_client_new ();

    if (!self->client) {
        fty_asset_autoupdate_destroy (&self);
    }
    return self;
}

#define is_ipv6(X) (X.find(':') != std::string::npos)
#define icase_streq(X,Y) (strcasecmp ((X),(Y)) == 0)

static void
autoupdate_update_rc_self (fty_asset_autoupdate_t *self, const std::string &assetName)
{
    if (!self) return;

    zhash_t *inventory = zhash_new ();
    zhash_t *aux = zhash_new ();

    zhash_autofree (inventory);
    zhash_autofree (aux);

    zhash_update (aux, "time", static_cast<void*>( const_cast<char*>(std::to_string (zclock_time () / 1000).c_str ())));

    std::string key, topic;
    auto ifaces = local_addresses();

    bool haveLAN1 = (ifaces.find ("LAN1") != ifaces.cend ());

    if (haveLAN1) {
        // hopefully running on RC
        int ipv4index = 0;
        int ipv6index = 0;
        for (int i = 1; i <= 3; ++i) {
            const auto iface = ifaces.find( "LAN" + std::to_string (i));
            if (iface != ifaces.cend()) {
                for (auto addr: iface->second) {
                    if (is_ipv6 (addr)) {
                        key = "ipv6." + std::to_string (++ipv6index);
                    } else {
                        key = "ip." + std::to_string (++ipv4index);
                    }
                    zhash_update (inventory, key.c_str(), static_cast<void*>( const_cast<char*>(addr.c_str ())));
                }
            }
        }
        zmsg_t *msg = fty_proto_encode_asset (
            aux,
            assetName.c_str (),
            "inventory",
            inventory);
        if (msg) {
            topic = "inventory@" + assetName;
            log_debug ("new inventory message %s", topic.c_str());
            int r = mlm_client_send (self->client, topic.c_str (), &msg);
            if( r != 0 ) log_error("failed to send inventory %s result %" PRIi32, topic.c_str(), r);
            zmsg_destroy (&msg);
        }
    } else {
        // interfaces have unexpected names, publish them in unspecified order
        int ipv4index = 0;
        int ipv6index = 0;
        for (auto interface: ifaces) {
            if (interface.first == "lo") continue;
            for (auto addr: interface.second) {
                if (is_ipv6 (addr)) {
                    key = "ipv6." + std::to_string (++ipv6index);
                } else {
                    key = "ip." + std::to_string (++ipv4index);
                }
                zhash_update (inventory, key.c_str(), static_cast<void*>( const_cast<char*>(addr.c_str ())));
            }
        }
        zmsg_t *msg = fty_proto_encode_asset (
            aux,
            assetName.c_str (),
            "inventory",
            inventory);
        if (msg) {
            topic = "inventory@" + assetName;
            log_debug ("new inventory message %s", topic.c_str());
            int r = mlm_client_send (self->client, topic.c_str (), &msg);
            if( r != 0 ) log_error("failed to send inventory %s result %" PRIi32, topic.c_str(), r);
            zmsg_destroy (&msg);
        }
    }
    zhash_destroy (&inventory);
    zhash_destroy (&aux);
}

static void
autoupdate_update_rc_information (fty_asset_autoupdate_t *self)
{
    if (!self) return;

    if (self->rcs.empty() )
        return;
    if (self->rcs.size() == 1) {
        // there is only one rc, must be me
        autoupdate_update_rc_self (self, self->rcs[0]);
        return;
    }
    // so self->rcs.size() > 1
    // there are more rc, test if one of my dns names == rc name
    // resolve dns names of all IP addresses on all interfaces
    // compare resolved name with list of RCs
    for (const auto &interface: local_addresses ()) {
        for (const auto &ip: interface.second) {
            std::string name = ip_to_name (ip.c_str());
            if ( name.empty () ) {
                continue;
            }

            std::string hostname = name;
            size_t i = hostname.find ('.');
            if (i != std::string::npos) {
                hostname.resize (i);
            }
            log_info ("%s:\tip='%s', dns_name='%s'", self->name, ip.c_str (), name.c_str ());
            for (const auto &rc: self->rcs) {
                if(rc.empty() || hostname.empty())
                    continue;
                if ( icase_streq (rc.c_str (), hostname.c_str ()) ||
                     icase_streq (rc.c_str (), name.c_str ())) {
                    // asset name == hostname this is me
                    autoupdate_update_rc_self (self, rc);
                    return;
                }
            }
        }
    }
}

static void
autoupdate_request_all_rcs (fty_asset_autoupdate_t *self)
{
    if (!self) return;

    if ( self->verbose)
        log_debug ("%s:\tRequest RC list", self->name);
    zmsg_t *msg = zmsg_new ();
    zmsg_addstr (msg, "GET");
    zmsg_addstr (msg, "");
    zmsg_addstr (msg, "rack controller");
    int rv = mlm_client_sendto (self->client, self->asset_agent_name, "ASSETS_IN_CONTAINER", NULL, 5000, &msg);
    if (rv != 0) {
        log_error ("%s:\tRequest RC list failed", self->name);
    }
    zmsg_destroy (&msg);
}

static void
autoupdate_update (fty_asset_autoupdate_t *self)
{
    autoupdate_update_rc_information (self);
}

static void
autoupdate_handle_message (fty_asset_autoupdate_t *self, zmsg_t *message)
{
    if (!self || !message ) return;

    const char *sender = mlm_client_sender (self->client);
    const char *subj = mlm_client_subject (self->client);
    if (streq (sender, self->asset_agent_name)) {
        if (streq (subj, "ASSETS_IN_CONTAINER")) {
            if ( self->verbose ) {
                log_debug ("%s:\tGot reply with RC:", self->name);
                zmsg_print (message);
            }
            self->rcs.clear ();
            char *okfail = zmsg_popstr (message);
            if ( okfail && streq (okfail, "OK")) {
                char *rc = zmsg_popstr(message);
                while (rc) {
                    self->rcs.push_back(rc);
                    // TODO: ask agent-asset for device details
                    zstr_free (&rc);
                    rc = zmsg_popstr(message);
                }
            }
            zstr_free (&okfail);
            autoupdate_update (self);
        }
    }
}

void
fty_asset_autoupdate_server (zsock_t *pipe, void *args)
{
    assert(args);

    fty_asset_autoupdate_t *self = fty_asset_autoupdate_new ();
    if (!self) {
        log_error("fty_asset_autoupdate_new failed");
        return;
    }

    zpoller_t *poller = zpoller_new (pipe, mlm_client_msgpipe(self->client), NULL);
    if (!poller) {
        log_error("zpoller_new failed");
        fty_asset_autoupdate_destroy (&self);
        return;
    }

    self->name = strdup (static_cast<char*>(args));

    // Signal need to be send as it is required by "actor_new"
    zsock_signal (pipe, 0);

    log_info ("%s:\tStarted", self->name);

    while (!zsys_interrupted) {
        void *which = zpoller_wait (poller, -1);
        if (which == NULL) {
            if (zpoller_terminated(poller) || zsys_interrupted) {
                break;
            }
        }
        else if (which == pipe) {
            zmsg_t *msg = zmsg_recv (pipe);
            char *cmd = zmsg_popstr (msg);
            log_debug ("%s:\tActor command=%s", self->name, cmd);

            if (streq (cmd, "$TERM")) {
                if ( !self->verbose ) // ! is here intentionally, to get rid of duplication information
                    log_info ("%s:\tGot $TERM", self->name);
                zstr_free (&cmd);
                zmsg_destroy (&msg);
                break;
            }
            else
            if (streq (cmd, "VERBOSE")) {
                self->verbose = true;
            }
            else
            if (streq (cmd, "CONNECT")) {
                char* endpoint = zmsg_popstr (msg);
                int rv = mlm_client_connect (self->client, endpoint, 1000, self->name);
                if (rv == -1) {
                    log_error ("%s:\tCan't connect to malamute endpoint '%s'", self->name, endpoint);
                }
                zstr_free (&endpoint);
                zsock_signal (pipe, 0);
            }
            else
            if (streq (cmd, "PRODUCER")) {
                char* stream = zmsg_popstr (msg);
                int rv = mlm_client_set_producer (self->client, stream);
                if (rv == -1) {
                    log_error ("%s:\tCan't set producer on stream '%s'", self->name, stream);
                }
                zstr_free (&stream);
                zsock_signal (pipe, 0);
            }
            else
            if (streq (cmd, "CONSUMER")) {
                char* stream = zmsg_popstr (msg);
                char* pattern = zmsg_popstr (msg);
                int rv = mlm_client_set_consumer (self->client, stream, pattern);
                if (rv == -1) {
                    log_error ("%s:\tCan't set consumer on stream '%s', '%s'", self->name, stream, pattern);
                }
                zstr_free (&pattern);
                zstr_free (&stream);
                zsock_signal (pipe, 0);
            }
            else
            if (streq (cmd, "WAKEUP")) {
                autoupdate_request_all_rcs (self);
            }
            else
            if (streq (cmd, "ASSET_AGENT_NAME")) {
                char *name = zmsg_popstr (msg);
                zstr_free(&self->asset_agent_name);
                self->asset_agent_name = name;
            }
            else {
                log_info ("%s:\tUnhandled command %s", self->name, cmd);
            }
            zstr_free (&cmd);
            zmsg_destroy (&msg);
        }
        else if (which == mlm_client_msgpipe(self->client)) {
            zmsg_t *zmessage = mlm_client_recv (self->client);
            autoupdate_handle_message (self, zmessage);
            zmsg_destroy (&zmessage);
        }
    }

    log_info ("%s:\tended", self->name);

    zpoller_destroy (&poller);
    fty_asset_autoupdate_destroy (&self);
}

//  --------------------------------------------------------------------------
//  Self test of this class

void
fty_asset_autoupdate_test (bool /*verbose*/)
{
    std::cout << " * fty_asset_autoupdate: " << std::endl;

    //  Simple create/destroy test
    fty_asset_autoupdate_t *self = fty_asset_autoupdate_new ();
    assert (self);
    fty_asset_autoupdate_destroy (&self);
    assert (!self);

    std::cout << "fty_asset_autoupdate: OK"  << std::endl;
}
