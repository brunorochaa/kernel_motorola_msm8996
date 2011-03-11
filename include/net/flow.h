/*
 *
 *	Generic internet FLOW.
 *
 */

#ifndef _NET_FLOW_H
#define _NET_FLOW_H

#include <linux/in6.h>
#include <asm/atomic.h>

struct flowi_common {
	int	flowic_oif;
	int	flowic_iif;
	__u32	flowic_mark;
	__u8	flowic_tos;
	__u8	flowic_scope;
	__u8	flowic_proto;
	__u8	flowic_flags;
#define FLOWI_FLAG_ANYSRC		0x01
#define FLOWI_FLAG_PRECOW_METRICS	0x02
#define FLOWI_FLAG_CAN_SLEEP		0x04
	__u32	flowic_secid;
};

union flowi_uli {
	struct {
		__be16	sport;
		__be16	dport;
	} ports;

	struct {
		__u8	type;
		__u8	code;
	} icmpt;

	struct {
		__le16	sport;
		__le16	dport;
	} dnports;

	__be32		spi;
	__be32		gre_key;

	struct {
		__u8	type;
	} mht;
};

struct flowi {
	struct flowi_common	__fl_common;
#define flowi_oif		__fl_common.flowic_oif
#define flowi_iif		__fl_common.flowic_iif
#define flowi_mark		__fl_common.flowic_mark
#define flowi_tos		__fl_common.flowic_tos
#define flowi_scope		__fl_common.flowic_scope
#define flowi_proto		__fl_common.flowic_proto
#define flowi_flags		__fl_common.flowic_flags
#define flowi_secid		__fl_common.flowic_secid

	union {
		struct {
			__be32			daddr;
			__be32			saddr;
		} ip4_u;
		
		struct {
			struct in6_addr		daddr;
			struct in6_addr		saddr;
			__be32			flowlabel;
		} ip6_u;

		struct {
			__le16			daddr;
			__le16			saddr;
			__u8			scope;
		} dn_u;
	} nl_u;
#define fld_dst		nl_u.dn_u.daddr
#define fld_src		nl_u.dn_u.saddr
#define fld_scope	nl_u.dn_u.scope
#define fl6_dst		nl_u.ip6_u.daddr
#define fl6_src		nl_u.ip6_u.saddr
#define fl6_flowlabel	nl_u.ip6_u.flowlabel
#define fl4_dst		nl_u.ip4_u.daddr
#define fl4_src		nl_u.ip4_u.saddr
#define fl4_tos		flowi_tos
#define fl4_scope	flowi_scope

	union flowi_uli uli_u;
#define fl_ip_sport	uli_u.ports.sport
#define fl_ip_dport	uli_u.ports.dport
#define fl_icmp_type	uli_u.icmpt.type
#define fl_icmp_code	uli_u.icmpt.code
#define fl_ipsec_spi	uli_u.spi
#define fl_mh_type	uli_u.mht.type
#define fl_gre_key	uli_u.gre_key
} __attribute__((__aligned__(BITS_PER_LONG/8)));

#define FLOW_DIR_IN	0
#define FLOW_DIR_OUT	1
#define FLOW_DIR_FWD	2

struct net;
struct sock;
struct flow_cache_ops;

struct flow_cache_object {
	const struct flow_cache_ops *ops;
};

struct flow_cache_ops {
	struct flow_cache_object *(*get)(struct flow_cache_object *);
	int (*check)(struct flow_cache_object *);
	void (*delete)(struct flow_cache_object *);
};

typedef struct flow_cache_object *(*flow_resolve_t)(
		struct net *net, const struct flowi *key, u16 family,
		u8 dir, struct flow_cache_object *oldobj, void *ctx);

extern struct flow_cache_object *flow_cache_lookup(
		struct net *net, const struct flowi *key, u16 family,
		u8 dir, flow_resolve_t resolver, void *ctx);

extern void flow_cache_flush(void);
extern atomic_t flow_cache_genid;

static inline int flow_cache_uli_match(const struct flowi *fl1,
				       const struct flowi *fl2)
{
	return (fl1->flowi_proto == fl2->flowi_proto &&
		!memcmp(&fl1->uli_u, &fl2->uli_u, sizeof(fl1->uli_u)));
}

#endif
