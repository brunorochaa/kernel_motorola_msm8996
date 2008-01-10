/*
 * ipv6 in net namespaces
 */

#include <net/inet_frag.h>

#ifndef __NETNS_IPV6_H__
#define __NETNS_IPV6_H__

struct ctl_table_header;

struct netns_sysctl_ipv6 {
#ifdef CONFIG_SYSCTL
	struct ctl_table_header *table;
#endif
	struct inet_frags_ctl frags;
	int bindv6only;
	int flush_delay;
	int ip6_rt_max_size;
	int ip6_rt_gc_min_interval;
	int ip6_rt_gc_timeout;
	int ip6_rt_gc_interval;
	int ip6_rt_gc_elasticity;
	int ip6_rt_mtu_expires;
	int ip6_rt_min_advmss;
};

struct netns_ipv6 {
	struct netns_sysctl_ipv6 sysctl;
};
#endif
