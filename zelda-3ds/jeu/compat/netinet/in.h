/*
 * Version 3DS de <netinet/in.h> : celle de libctru, plus des définitions IPv6 minimales.
 * La 3DS ne gère pas IPv6 ; ces types permettent seulement de compiler le code réseau de
 * borealis (les appels IPv6 échouent proprement à l'exécution).
 */
#pragma once

#include_next <netinet/in.h>

#ifndef ZELDA3DS_IPV6_FACTICE
#define ZELDA3DS_IPV6_FACTICE

#ifndef IPPROTO_IPV6
#define IPPROTO_IPV6 41
#endif
#ifndef IPV6_V6ONLY
#define IPV6_V6ONLY 26
#endif
#ifndef INET6_ADDRSTRLEN
#define INET6_ADDRSTRLEN 46
#endif

struct in6_addr {
  unsigned char s6_addr[16];
};

struct sockaddr_in6 {
  sa_family_t sin6_family;
  in_port_t sin6_port;
  uint32_t sin6_flowinfo;
  struct in6_addr sin6_addr;
  uint32_t sin6_scope_id;
};

#endif
