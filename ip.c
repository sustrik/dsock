/*

  Copyright (c) 2015 Martin Sustrik

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"),
  to deal in the Software without restriction, including without limitation
  the rights to use, copy, modify, merge, publish, distribute, sublicense,
  and/or sell copies of the Software, and to permit persons to whom
  the Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included
  in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
  THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
  IN THE SOFTWARE.

*/

#if defined __linux__
#define _GNU_SOURCE
#include <netdb.h>
#include <sys/eventfd.h>
#endif

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#if !defined __sun
#include <ifaddrs.h>
#endif
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "dns/dns.h"

#include "dillsocks.h"
#include "utils.h"

DILL_CT_ASSERT(sizeof(ipaddr) >= sizeof(struct sockaddr_in));
DILL_CT_ASSERT(sizeof(ipaddr) >= sizeof(struct sockaddr_in6));

static struct dns_resolv_conf *dill_dns_conf = NULL;
static struct dns_hosts *dill_dns_hosts = NULL;
static struct dns_hints *dill_dns_hints = NULL;
static struct dns_resolver *dill_dns_resolver = NULL;

static ipaddr dill_ipany(int port, int mode)
{
    ipaddr addr;
    if(dill_slow(port < 0 || port > 0xffff)) {
        ((struct sockaddr*)&addr)->sa_family = AF_UNSPEC;
        errno = EINVAL;
        return addr;
    }
    if (mode == 0 || mode == IPADDR_IPV4 || mode == IPADDR_PREF_IPV4) {
        struct sockaddr_in *ipv4 = (struct sockaddr_in*)&addr;
        ipv4->sin_family = AF_INET;
        ipv4->sin_addr.s_addr = htonl(INADDR_ANY);
        ipv4->sin_port = htons((uint16_t)port);
    }
    else {
        struct sockaddr_in6 *ipv6 = (struct sockaddr_in6*)&addr;
        ipv6->sin6_family = AF_INET6;
        memcpy(&ipv6->sin6_addr, &in6addr_any, sizeof(in6addr_any));
        ipv6->sin6_port = htons((uint16_t)port);
    }
    errno = 0;
    return addr;
}

/* Convert literal IPv4 address to a binary one. */
static ipaddr dill_ipv4_literal(const char *addr, int port) {
    ipaddr raddr;
    struct sockaddr_in *ipv4 = (struct sockaddr_in*)&raddr;
    int rc = inet_pton(AF_INET, addr, &ipv4->sin_addr);
    dill_assert(rc >= 0);
    if(rc == 1) {
        ipv4->sin_family = AF_INET;
        ipv4->sin_port = htons((uint16_t)port);
        errno = 0;
        return raddr;
    }
    ipv4->sin_family = AF_UNSPEC;
    errno = EINVAL;
    return raddr;
}

/* Convert literal IPv6 address to a binary one. */
static ipaddr dill_ipv6_literal(const char *addr, int port) {
    ipaddr raddr;
    struct sockaddr_in6 *ipv6 = (struct sockaddr_in6*)&raddr;
    int rc = inet_pton(AF_INET6, addr, &ipv6->sin6_addr);
    dill_assert(rc >= 0);
    if(rc == 1) {
        ipv6->sin6_family = AF_INET6;
        ipv6->sin6_port = htons((uint16_t)port);
        errno = 0;
        return raddr;
    }
    ipv6->sin6_family = AF_UNSPEC;
    errno = EINVAL;
    return raddr;
}

/* Convert literal IPv4 or IPv6 address to a binary one. */
static ipaddr dill_ipliteral(const char *addr, int port, int mode) {
    ipaddr raddr;
    struct sockaddr *sa = (struct sockaddr*)&raddr;
    if(dill_slow(!addr || port < 0 || port > 0xffff)) {
        sa->sa_family = AF_UNSPEC;
        errno = EINVAL;
        return raddr;
    }
    switch(mode) {
    case IPADDR_IPV4:
        return dill_ipv4_literal(addr, port);
    case IPADDR_IPV6:
        return dill_ipv6_literal(addr, port);
    case 0:
    case IPADDR_PREF_IPV4:
        raddr = dill_ipv4_literal(addr, port);
        if(errno == 0)
            return raddr;
        return dill_ipv6_literal(addr, port);
    case IPADDR_PREF_IPV6:
        raddr = dill_ipv6_literal(addr, port);
        if(errno == 0)
            return raddr;
        return dill_ipv4_literal(addr, port);
    default:
        dill_assert(0);
    }
}

int ipfamily(const ipaddr *addr) {
    return ((struct sockaddr*)addr)->sa_family;
}

int iplen(const ipaddr *addr) {
    return ipfamily(addr) == AF_INET ?
        sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6);
}

const struct sockaddr *ipsockaddr(const ipaddr *addr) {
    return (const struct sockaddr*)addr;
}

int ipport(const ipaddr *addr) {
    return ntohs(ipfamily(addr) == AF_INET ?
        ((struct sockaddr_in*)addr)->sin_port :
        ((struct sockaddr_in6*)addr)->sin6_port);
}

/* Convert IP address from network format to ASCII dot notation. */
const char *ipaddrstr(const ipaddr *addr, char *ipstr) {
    if(ipfamily(addr) == AF_INET) {
        return inet_ntop(AF_INET, &(((struct sockaddr_in*)addr)->sin_addr),
            ipstr, INET_ADDRSTRLEN);
    }
    else {
        return inet_ntop(AF_INET6, &(((struct sockaddr_in6*)addr)->sin6_addr),
            ipstr, INET6_ADDRSTRLEN);
    }
}

ipaddr iplocal(const char *name, int port, int mode) {
    if(!name)
        return dill_ipany(port, mode);
    ipaddr addr = dill_ipliteral(name, port, mode);
#if defined __sun
    return addr;
#else
    if(errno == 0)
       return addr;
    /* Address is not a literal. It must be an interface name then. */
    struct ifaddrs *ifaces = NULL;
    int rc = getifaddrs (&ifaces);
    dill_assert (rc == 0);
    dill_assert (ifaces);
    /*  Find first IPv4 and first IPv6 address. */
    struct ifaddrs *ipv4 = NULL;
    struct ifaddrs *ipv6 = NULL;
    struct ifaddrs *it;
    for(it = ifaces; it != NULL; it = it->ifa_next) {
        if(!it->ifa_addr)
            continue;
        if(strcmp(it->ifa_name, name) != 0)
            continue;
        switch(it->ifa_addr->sa_family) {
        case AF_INET:
            dill_assert(!ipv4);
            ipv4 = it;
            break;
        case AF_INET6:
            dill_assert(!ipv6);
            ipv6 = it;
            break;
        }
        if(ipv4 && ipv6)
            break;
    }
    /* Choose the correct address family based on mode. */
    switch(mode) {
    case IPADDR_IPV4:
        ipv6 = NULL;
        break;
    case IPADDR_IPV6:
        ipv4 = NULL;
        break;
    case 0:
    case IPADDR_PREF_IPV4:
        if(ipv4)
           ipv6 = NULL;
        break;
    case IPADDR_PREF_IPV6:
        if(ipv6)
           ipv4 = NULL;
        break;
    default:
        dill_assert(0);
    }
    if(ipv4) {
        struct sockaddr_in *inaddr = (struct sockaddr_in*)&addr;
        memcpy(inaddr, ipv4->ifa_addr, sizeof (struct sockaddr_in));
        inaddr->sin_port = htons(port);
        freeifaddrs(ifaces);
        errno = 0;
        return addr;
    }
    if(ipv6) {
        struct sockaddr_in6 *inaddr = (struct sockaddr_in6*)&addr;
        memcpy(inaddr, ipv6->ifa_addr, sizeof (struct sockaddr_in6));
        inaddr->sin6_port = htons(port);
        freeifaddrs(ifaces);
        errno = 0;
        return addr;
    }
    freeifaddrs(ifaces);
    ((struct sockaddr*)&addr)->sa_family = AF_UNSPEC;
    errno = ENODEV;
    return addr;
#endif
}

ipaddr ipremote(const char *name, int port, int mode, int64_t deadline) {
    int rc;
    ipaddr addr = dill_ipliteral(name, port, mode);
    if(errno == 0)
       return addr;
    /* Load DNS config files, unless they are already chached. */
    if(dill_slow(!dill_dns_resolver)) {
        /* TODO: Maybe re-read the configuration once in a while? */
        dill_dns_conf = dns_resconf_local(&rc);
        dill_assert(dill_dns_conf);
        dill_dns_hosts = dns_hosts_local(&rc);
        dill_assert(dill_dns_hosts);
        dill_dns_hints = dns_hints_local(dill_dns_conf, &rc);
        dill_assert(dill_dns_hints);
        dill_dns_resolver = dns_res_open(dill_dns_conf, dill_dns_hosts,
            dill_dns_hints, NULL, dns_opts(), &rc);
        dill_assert(dill_dns_resolver);
    }
    /* Let's do asynchronous DNS query here. */
    dill_assert(port >= 0 && port <= 0xffff);
    char portstr[8];
    snprintf(portstr, sizeof(portstr), "%d", port);
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = PF_UNSPEC;
    struct dns_addrinfo *ai = dns_ai_open(name, portstr, DNS_T_A, &hints,
        dill_dns_resolver, &rc);
    dill_assert(ai);
    struct addrinfo *ipv4 = NULL;
    struct addrinfo *ipv6 = NULL;
    struct addrinfo *it = NULL;
    while(1) {
        rc = dns_ai_nextent(&it, ai);
        if(rc == EAGAIN) {
            int fd = dns_ai_pollfd(ai);
            dill_assert(fd >= 0);
            int events = fdwait(fd, FDW_IN, deadline);
            /* There's no guarantee that the file descriptor will be reused
               in next iteration. We have to clean the fdwait cache here
               to be on the safe side. */
            fdclean(fd);
            if(dill_slow(!events)) {
                errno = ETIMEDOUT;
                return addr;
            }
            dill_assert(events == FDW_IN);
            continue;
        }
        if(rc == ENOENT)
            break;
        if(!ipv4 && it && it->ai_family == AF_INET)
            ipv4 = it;
        if(!ipv6 && it && it->ai_family == AF_INET6)
            ipv6 = it;
        if(ipv4 && ipv6)
            break;
    }
    switch(mode) {
    case IPADDR_IPV4:
        ipv6 = NULL;
        break;
    case IPADDR_IPV6:
        ipv4 = NULL;
        break;
    case 0:
    case IPADDR_PREF_IPV4:
        if(ipv4)
           ipv6 = NULL;
        break;
    case IPADDR_PREF_IPV6:
        if(ipv6)
           ipv4 = NULL;
        break;
    default:
        dill_assert(0);
    }
    if(ipv4) {
        struct sockaddr_in *inaddr = (struct sockaddr_in*)&addr;
        memcpy(inaddr, ipv4->ai_addr, sizeof (struct sockaddr_in));
        inaddr->sin_port = htons(port);
        dns_ai_close(ai);
        errno = 0;
        return addr;
    }
    if(ipv6) {
        struct sockaddr_in6 *inaddr = (struct sockaddr_in6*)&addr;
        memcpy(inaddr, ipv6->ai_addr, sizeof (struct sockaddr_in6));
        inaddr->sin6_port = htons(port);
        dns_ai_close(ai);
        errno = 0;
        return addr;
    }
    dns_ai_close(ai);
    ((struct sockaddr*)&addr)->sa_family = AF_UNSPEC;
    errno = EADDRNOTAVAIL;
    return addr;
}

