/*  =========================================================================
    dns - DNS and networking helper

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
    dns - DNS and networking helper
@discuss
@end
*/

#include "dns.h"

#include <netdb.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <cstdio>
#include <cstring>
#include <iostream>

#include <cassert>

std::set<std::string> name_to_ip4 (const char *name)
{
    char buffer[INET_ADDRSTRLEN];
    struct addrinfo *info, *infos = NULL;
    struct addrinfo hint;
    std::set<std::string> result;

    memset(&hint, 0, sizeof (struct addrinfo));
    hint.ai_family = AF_INET;
    getaddrinfo(name, NULL, &hint, &infos);
    info = infos;
    while (info) {
        buffer[0] = 0;
        if (info->ai_family == AF_INET) {
            inet_ntop (AF_INET, &(reinterpret_cast<struct sockaddr_in *>(info->ai_addr))->sin_addr, buffer, sizeof (buffer));
            result.insert (buffer);
        }
        info = info->ai_next;
    }
    freeaddrinfo(infos);
    return result;
}

std::set<std::string> name_to_ip6 (const char *name)
{
    char buffer[INET6_ADDRSTRLEN];
    struct addrinfo *info, *infos = NULL;
    struct addrinfo hint;
    std::set<std::string> result;

    memset(&hint, 0, sizeof (struct addrinfo));
    hint.ai_family = AF_INET6;
    getaddrinfo(name, NULL, &hint, &infos);
    info = infos;
    while (info) {
        buffer[0] = 0;
        if (info->ai_family == AF_INET6) {
            inet_ntop (AF_INET6, &(reinterpret_cast<struct sockaddr_in6 *>(info->ai_addr))->sin6_addr, buffer, sizeof (buffer));
            result.insert (buffer);
        }
        info = info->ai_next;
    }
    freeaddrinfo(infos);
    return result;
}

std::set<std::string> name_to_ip (const char *name)
{
    std::set<std::string> ip4 = name_to_ip4 (name);
    std::set<std::string> ip6 = name_to_ip6 (name);
    ip4.insert (ip6.begin(), ip6.end());
    return ip4;
}

std::string ip_to_name (const char *ip)
{
    char hbuf[NI_MAXHOST];

    if (!ip) return "";
    if (strchr(ip, ':')) {
        // IPv6
        struct sockaddr_in6 sa_in;
        memset (&sa_in, 0, sizeof(sa_in));
        sa_in.sin6_family = AF_INET6;

        if (inet_pton (AF_INET6, ip, &sa_in.sin6_addr) == 1) {
            if (
                getnameinfo(
                    reinterpret_cast<struct sockaddr *>(&sa_in),
                    sizeof(sa_in),
                    hbuf,
                    sizeof(hbuf),
                    NULL,
                    0,
                    NI_NAMEREQD) == 0
            ) {
                return hbuf;
            }
        }
    }
    // IPv4
    struct sockaddr_in sa_in;
    memset (&sa_in, 0, sizeof(sa_in));
    sa_in.sin_family = AF_INET;

    if (inet_pton (AF_INET, ip, &sa_in.sin_addr) == 1) {
        if (
            getnameinfo(
                reinterpret_cast<struct sockaddr *>(&sa_in),
                sizeof(sa_in),
                hbuf,
                sizeof(hbuf),
                NULL,
                0,
                NI_NAMEREQD) == 0
        ) {
            return hbuf;
        }
    }
    return "";
}

std::map<std::string,std::set<std::string>> local_addresses()
{
    struct ifaddrs *interfaces, *iface;
    char host[NI_MAXHOST];
    std::map<std::string,std::set<std::string>> result;

    if (getifaddrs (&interfaces) == -1) {
        return result;
    }

    for (iface = interfaces; iface != NULL; iface = iface->ifa_next) {
        if (iface->ifa_addr == NULL) continue;
        int family = iface->ifa_addr->sa_family;
        if (family == AF_INET || family == AF_INET6) {
            if (
                getnameinfo(iface->ifa_addr,
                            (family == AF_INET) ? sizeof(struct sockaddr_in) :
                            sizeof(struct sockaddr_in6),
                            host, NI_MAXHOST,
                            NULL, 0, NI_NUMERICHOST) == 0
            ) {
                // sometimes IPv6 addres looks like ::2342%IfaceName
                char *p = strchr (host, '%');
                if (p) *p = 0;

                auto it = result.find (iface->ifa_name);
                if (it == result.end()) {
                    std::set<std::string> aSet;
                    aSet.insert (host);
                    result [iface->ifa_name] = aSet;
                } else {
                    result [iface->ifa_name].insert (host);
                }
            }
        }
    }
    freeifaddrs (interfaces);
    return result;
}

//  --------------------------------------------------------------------------
//  Self test of this class

void dns_test (bool /*verbose*/)
{
    std::cout << " * dns:" << std::endl;

    {
        int found = 0;
        for (auto a : name_to_ip4 ("localhost")) {
            if (a == "127.0.0.1") found++;
        }
        assert (found == 1);
    }

    assert (! ip_to_name("127.0.0.1").empty ());

    std::cout << "dns: OK" << std::endl;
}

