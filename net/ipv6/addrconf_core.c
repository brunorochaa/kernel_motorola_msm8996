/*
 * IPv6 library code, needed by static components when full IPv6 support is
 * not configured or static.
 */

#include <linux/export.h>
#include <net/ipv6.h>
#include <net/addrconf.h>

#define IPV6_ADDR_SCOPE_TYPE(scope)	((scope) << 16)

static inline unsigned int ipv6_addr_scope2type(unsigned int scope)
{
	switch (scope) {
	case IPV6_ADDR_SCOPE_NODELOCAL:
		return (IPV6_ADDR_SCOPE_TYPE(IPV6_ADDR_SCOPE_NODELOCAL) |
			IPV6_ADDR_LOOPBACK);
	case IPV6_ADDR_SCOPE_LINKLOCAL:
		return (IPV6_ADDR_SCOPE_TYPE(IPV6_ADDR_SCOPE_LINKLOCAL) |
			IPV6_ADDR_LINKLOCAL);
	case IPV6_ADDR_SCOPE_SITELOCAL:
		return (IPV6_ADDR_SCOPE_TYPE(IPV6_ADDR_SCOPE_SITELOCAL) |
			IPV6_ADDR_SITELOCAL);
	}
	return IPV6_ADDR_SCOPE_TYPE(scope);
}

int __ipv6_addr_type(const struct in6_addr *addr)
{
	__be32 st;

	st = addr->s6_addr32[0];

	/* Consider all addresses with the first three bits different of
	   000 and 111 as unicasts.
	 */
	if ((st & htonl(0xE0000000)) != htonl(0x00000000) &&
	    (st & htonl(0xE0000000)) != htonl(0xE0000000))
		return (IPV6_ADDR_UNICAST |
			IPV6_ADDR_SCOPE_TYPE(IPV6_ADDR_SCOPE_GLOBAL));

	if ((st & htonl(0xFF000000)) == htonl(0xFF000000)) {
		/* multicast */
		/* addr-select 3.1 */
		return (IPV6_ADDR_MULTICAST |
			ipv6_addr_scope2type(IPV6_ADDR_MC_SCOPE(addr)));
	}

	if ((st & htonl(0xFFC00000)) == htonl(0xFE800000))
		return (IPV6_ADDR_LINKLOCAL | IPV6_ADDR_UNICAST |
			IPV6_ADDR_SCOPE_TYPE(IPV6_ADDR_SCOPE_LINKLOCAL));		/* addr-select 3.1 */
	if ((st & htonl(0xFFC00000)) == htonl(0xFEC00000))
		return (IPV6_ADDR_SITELOCAL | IPV6_ADDR_UNICAST |
			IPV6_ADDR_SCOPE_TYPE(IPV6_ADDR_SCOPE_SITELOCAL));		/* addr-select 3.1 */
	if ((st & htonl(0xFE000000)) == htonl(0xFC000000))
		return (IPV6_ADDR_UNICAST |
			IPV6_ADDR_SCOPE_TYPE(IPV6_ADDR_SCOPE_GLOBAL));			/* RFC 4193 */

	if ((addr->s6_addr32[0] | addr->s6_addr32[1]) == 0) {
		if (addr->s6_addr32[2] == 0) {
			if (addr->s6_addr32[3] == 0)
				return IPV6_ADDR_ANY;

			if (addr->s6_addr32[3] == htonl(0x00000001))
				return (IPV6_ADDR_LOOPBACK | IPV6_ADDR_UNICAST |
					IPV6_ADDR_SCOPE_TYPE(IPV6_ADDR_SCOPE_LINKLOCAL));	/* addr-select 3.4 */

			return (IPV6_ADDR_COMPATv4 | IPV6_ADDR_UNICAST |
				IPV6_ADDR_SCOPE_TYPE(IPV6_ADDR_SCOPE_GLOBAL));	/* addr-select 3.3 */
		}

		if (addr->s6_addr32[2] == htonl(0x0000ffff))
			return (IPV6_ADDR_MAPPED |
				IPV6_ADDR_SCOPE_TYPE(IPV6_ADDR_SCOPE_GLOBAL));	/* addr-select 3.3 */
	}

	return (IPV6_ADDR_UNICAST |
		IPV6_ADDR_SCOPE_TYPE(IPV6_ADDR_SCOPE_GLOBAL));	/* addr-select 3.4 */
}
EXPORT_SYMBOL(__ipv6_addr_type);

static ATOMIC_NOTIFIER_HEAD(inet6addr_chain);

int register_inet6addr_notifier(struct notifier_block *nb)
{
	return atomic_notifier_chain_register(&inet6addr_chain, nb);
}
EXPORT_SYMBOL(register_inet6addr_notifier);

int unregister_inet6addr_notifier(struct notifier_block *nb)
{
	return atomic_notifier_chain_unregister(&inet6addr_chain, nb);
}
EXPORT_SYMBOL(unregister_inet6addr_notifier);

int inet6addr_notifier_call_chain(unsigned long val, void *v)
{
	return atomic_notifier_call_chain(&inet6addr_chain, val, v);
}
EXPORT_SYMBOL(inet6addr_notifier_call_chain);

const struct ipv6_stub *ipv6_stub __read_mostly;
EXPORT_SYMBOL_GPL(ipv6_stub);

/* IPv6 Wildcard Address and Loopback Address defined by RFC2553 */
const struct in6_addr in6addr_loopback = IN6ADDR_LOOPBACK_INIT;
EXPORT_SYMBOL(in6addr_loopback);
const struct in6_addr in6addr_any = IN6ADDR_ANY_INIT;
EXPORT_SYMBOL(in6addr_any);
const struct in6_addr in6addr_linklocal_allnodes = IN6ADDR_LINKLOCAL_ALLNODES_INIT;
EXPORT_SYMBOL(in6addr_linklocal_allnodes);
const struct in6_addr in6addr_linklocal_allrouters = IN6ADDR_LINKLOCAL_ALLROUTERS_INIT;
EXPORT_SYMBOL(in6addr_linklocal_allrouters);
const struct in6_addr in6addr_interfacelocal_allnodes = IN6ADDR_INTERFACELOCAL_ALLNODES_INIT;
EXPORT_SYMBOL(in6addr_interfacelocal_allnodes);
const struct in6_addr in6addr_interfacelocal_allrouters = IN6ADDR_INTERFACELOCAL_ALLROUTERS_INIT;
EXPORT_SYMBOL(in6addr_interfacelocal_allrouters);
const struct in6_addr in6addr_sitelocal_allrouters = IN6ADDR_SITELOCAL_ALLROUTERS_INIT;
EXPORT_SYMBOL(in6addr_sitelocal_allrouters);
