/* dhcp.h

   Protocol structures... */

/*
 * Copyright (c) 2004 by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 1995-2003 by Internet Software Consortium
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 *   Internet Systems Consortium, Inc.
 *   950 Charter Street
 *   Redwood City, CA 94063
 *   <info@isc.org>
 *   http://www.isc.org/
 *
 * This software has been written for Internet Systems Consortium
 * by Ted Lemon in cooperation with Vixie Enterprises.  To learn more
 * about Internet Systems Consortium, see ``http://www.isc.org''.
 * To learn more about Vixie Enterprises, see ``http://www.vix.com''.
 */

#define DHCP_UDP_OVERHEAD	(14 + /* Ethernet header */		\
				 20 + /* IP header */			\
				 8)   /* UDP header */
#define DHCP_SNAME_LEN		64
#define DHCP_FILE_LEN		128
#define DHCP_FIXED_NON_UDP	236
#define DHCP_FIXED_LEN		(DHCP_FIXED_NON_UDP + DHCP_UDP_OVERHEAD)
						/* Everything but options. */
#define DHCP_MTU_MAX		1500
#define DHCP_OPTION_LEN		(DHCP_MTU_MAX - DHCP_FIXED_LEN)

#define BOOTP_MIN_LEN		300
#define DHCP_MIN_LEN            548

struct dhcp_packet {
	u_int8_t  op;		/* 0: Message opcode/type */
	u_int8_t  htype;	/* 1: Hardware addr type (net/if_types.h) */
	u_int8_t  hlen;		/* 2: Hardware addr length */
	u_int8_t  hops;		/* 3: Number of relay agent hops from client */
	u_int32_t xid;		/* 4: Transaction ID */
	u_int16_t secs;		/* 8: Seconds since client started looking */
	u_int16_t flags;	/* 10: Flag bits */
	struct in_addr ciaddr;	/* 12: Client IP address (if already in use) */
	struct in_addr yiaddr;	/* 16: Client IP address */
	struct in_addr siaddr;	/* 18: IP address of next server to talk to */
	struct in_addr giaddr;	/* 20: DHCP relay agent IP address */
	unsigned char chaddr [16];	/* 24: Client hardware address */
	char sname [DHCP_SNAME_LEN];	/* 40: Server name */
	char file [DHCP_FILE_LEN];	/* 104: Boot filename */
	unsigned char options [DHCP_OPTION_LEN];
				/* 212: Optional parameters
				   (actual length dependent on MTU). */
};

/* BOOTP (rfc951) message types */
#define	BOOTREQUEST	1
#define BOOTREPLY	2

/* Possible values for flags field... */
#define BOOTP_BROADCAST 32768L

/* Possible values for hardware type (htype) field... */
#define HTYPE_ETHER	1               /* Ethernet 10Mbps              */
#define HTYPE_IEEE802	6               /* IEEE 802.2 Token Ring...	*/
#define HTYPE_FDDI	8		/* FDDI...			*/

/* Magic cookie validating dhcp options field (and bootp vendor
   extensions field). */
#define DHCP_OPTIONS_COOKIE	"\143\202\123\143"

/* DHCP Option codes: */

#define DHO_PAD				0
#define DHO_SUBNET_MASK			1
#define DHO_TIME_OFFSET			2
#define DHO_ROUTERS			3
#define DHO_TIME_SERVERS		4
#define DHO_NAME_SERVERS		5
#define DHO_DOMAIN_NAME_SERVERS		6
#define DHO_LOG_SERVERS			7
#define DHO_COOKIE_SERVERS		8
#define DHO_LPR_SERVERS			9
#define DHO_IMPRESS_SERVERS		10
#define DHO_RESOURCE_LOCATION_SERVERS	11
#define DHO_HOST_NAME			12
#define DHO_BOOT_SIZE			13
#define DHO_MERIT_DUMP			14
#define DHO_DOMAIN_NAME			15
#define DHO_SWAP_SERVER			16
#define DHO_ROOT_PATH			17
#define DHO_EXTENSIONS_PATH		18
#define DHO_IP_FORWARDING		19
#define DHO_NON_LOCAL_SOURCE_ROUTING	20
#define DHO_POLICY_FILTER		21
#define DHO_MAX_DGRAM_REASSEMBLY	22
#define DHO_DEFAULT_IP_TTL		23
#define DHO_PATH_MTU_AGING_TIMEOUT	24
#define DHO_PATH_MTU_PLATEAU_TABLE	25
#define DHO_INTERFACE_MTU		26
#define DHO_ALL_SUBNETS_LOCAL		27
#define DHO_BROADCAST_ADDRESS		28
#define DHO_PERFORM_MASK_DISCOVERY	29
#define DHO_MASK_SUPPLIER		30
#define DHO_ROUTER_DISCOVERY		31
#define DHO_ROUTER_SOLICITATION_ADDRESS	32
#define DHO_STATIC_ROUTES		33
#define DHO_TRAILER_ENCAPSULATION	34
#define DHO_ARP_CACHE_TIMEOUT		35
#define DHO_IEEE802_3_ENCAPSULATION	36
#define DHO_DEFAULT_TCP_TTL		37
#define DHO_TCP_KEEPALIVE_INTERVAL	38
#define DHO_TCP_KEEPALIVE_GARBAGE	39
#define DHO_NIS_DOMAIN			40
#define DHO_NIS_SERVERS			41
#define DHO_NTP_SERVERS			42
#define DHO_VENDOR_ENCAPSULATED_OPTIONS	43
#define DHO_NETBIOS_NAME_SERVERS	44
#define DHO_NETBIOS_DD_SERVER		45
#define DHO_NETBIOS_NODE_TYPE		46
#define DHO_NETBIOS_SCOPE		47
#define DHO_FONT_SERVERS		48
#define DHO_X_DISPLAY_MANAGER		49
#define DHO_DHCP_REQUESTED_ADDRESS	50
#define DHO_DHCP_LEASE_TIME		51
#define DHO_DHCP_OPTION_OVERLOAD	52
#define DHO_DHCP_MESSAGE_TYPE		53
#define DHO_DHCP_SERVER_IDENTIFIER	54
#define DHO_DHCP_PARAMETER_REQUEST_LIST	55
#define DHO_DHCP_MESSAGE		56
#define DHO_DHCP_MAX_MESSAGE_SIZE	57
#define DHO_DHCP_RENEWAL_TIME		58
#define DHO_DHCP_REBINDING_TIME		59
#define DHO_VENDOR_CLASS_IDENTIFIER	60
#define DHO_DHCP_CLIENT_IDENTIFIER	61
#define DHO_NWIP_DOMAIN_NAME		62
#define DHO_NWIP_SUBOPTIONS		63
#define DHO_USER_CLASS			77
#define DHO_FQDN			81
#define DHO_DHCP_AGENT_OPTIONS		82
#define DHO_SUBNET_SELECTION		118 /* RFC3011! */
/* The DHO_AUTHENTICATE option is not a standard yet, so I've
   allocated an option out of the "local" option space for it on a
   temporary basis.  Once an option code number is assigned, I will
   immediately and shamelessly break this, so don't count on it
   continuing to work. */
#define DHO_AUTHENTICATE		210

#define DHO_END				255

/* DHCP message types. */
#define DHCPDISCOVER	1
#define DHCPOFFER	2
#define DHCPREQUEST	3
#define DHCPDECLINE	4
#define DHCPACK		5
#define DHCPNAK		6
#define DHCPRELEASE	7
#define DHCPINFORM	8

/* Relay Agent Information option subtypes: */
#define RAI_CIRCUIT_ID	1
#define RAI_REMOTE_ID	2
#define RAI_AGENT_ID	3

/* FQDN suboptions: */
#define FQDN_NO_CLIENT_UPDATE		1
#define FQDN_SERVER_UPDATE		2
#define FQDN_ENCODED			3
#define FQDN_RCODE1			4
#define FQDN_RCODE2			5
#define FQDN_HOSTNAME			6
#define FQDN_DOMAINNAME			7
#define FQDN_FQDN			8
#define FQDN_SUBOPTION_COUNT		8

/*
 * DHCPv6 message types, defined in section 5.3 of RFC 3315
 */
#define DHCPV6_SOLICIT			1
#define DHCPV6_ADVERTISE		2
#define DHCPV6_REQUEST			3
#define DHCPV6_CONFIRM			4
#define DHCPV6_RENEW			5
#define DHCPV6_REBIND			6
#define DHCPV6_REPLY			7
#define DHCPV6_RELEASE			8
#define DHCPV6_DECLINE			9
#define DHCPV6_RECONFIGURE		10
#define DHCPV6_INFORMATION_REQUEST	11
#define DHCPV6_RELAY_FORW		12
#define DHCPV6_RELAY_REPL		13
#define DHCPV6_LEASEQUERY		14
#define DHCPV6_LEASEQUERY_REPLY		15

/* DHCPv6 Option codes: */
// 8,1,2,3,39,16,6

#define D6O_CLIENTID				1 /* RFC3315 */
#define D6O_SERVERID				2
#define D6O_IA_NA				3
#define D6O_IA_TA				4
#define D6O_IAADDR				5
#define D6O_ORO					6
#define D6O_PREFERENCE				7
#define D6O_ELAPSED_TIME			8
#define D6O_RELAY_MSG				9
/* Option code 10 unassigned. */
#define D6O_AUTH				11
#define D6O_UNICAST				12
#define D6O_STATUS_CODE				13
#define D6O_RAPID_COMMIT			14
#define D6O_USER_CLASS				15
#define D6O_VENDOR_CLASS			16
#define D6O_VENDOR_OPTS				17
#define D6O_INTERFACE_ID			18
#define D6O_RECONF_MSG				19
#define D6O_RECONF_ACCEPT			20
#define D6O_SIP_SERVERS_DNS			21 /* RFC3319 */
#define D6O_SIP_SERVERS_ADDR			22 /* RFC3319 */
#define D6O_NAME_SERVERS			23 /* RFC3646 */
#define D6O_DOMAIN_SEARCH			24 /* RFC3646 */
#define D6O_IA_PD				25 /* RFC3633 */
#define D6O_IAPREFIX				26 /* RFC3633 */
#define D6O_NIS_SERVERS				27 /* RFC3898 */
#define D6O_NISP_SERVERS			28 /* RFC3898 */
#define D6O_NIS_DOMAIN_NAME			29 /* RFC3898 */
#define D6O_NISP_DOMAIN_NAME			30 /* RFC3898 */
#define D6O_SNTP_SERVERS			31 /* RFC4075 */
#define D6O_INFORMATION_REFRESH_TIME		32 /* RFC4242 */
#define D6O_BCMCS_SERVER_D			33 /* RFC4280 */
#define D6O_BCMCS_SERVER_A			34 /* RFC4280 */
/* 35 is unassigned */
#define D6O_GEOCONF_CIVIC			36 /* RFC4776 */
#define D6O_REMOTE_ID				37 /* RFC4649 */
#define D6O_SUBSCRIBER_ID			38 /* RFC4580 */
#define D6O_CLIENT_FQDN				39 /* RFC4704 */
#define D6O_PANA_AGENT				40 /* paa-option */
#define D6O_NEW_POSIX_TIMEZONE			41 /* RFC4833 */
#define D6O_NEW_TZDB_TIMEZONE			42 /* RFC4833 */
#define D6O_ERO					43 /* RFC4994 */
#define D6O_LQ_QUERY				44 /* RFC5007 */
#define D6O_CLIENT_DATA				45 /* RFC5007 */
#define D6O_CLT_TIME				46 /* RFC5007 */
#define D6O_LQ_RELAY_DATA			47 /* RFC5007 */
#define D6O_LQ_CLIENT_LINK			48 /* RFC5007 */

/*
 * Status Codes, from RFC 3315 section 24.4, and RFC 3633, 5007.
 */
#define STATUS_Success		0
#define STATUS_UnspecFail	1
#define STATUS_NoAddrsAvail	2
#define STATUS_NoBinding	3
#define STATUS_NotOnLink	4
#define STATUS_UseMulticast	5
#define STATUS_NoPrefixAvail	6
#define STATUS_UnknownQueryType	7
#define STATUS_MalformedQuery	8
#define STATUS_NotConfigured	9
#define STATUS_NotAllowed	10

#define DHCP6_OPTION_LEN	1000
#define DHCP6_PACKET_LEN	(DHCP6_OPTION_LEN + 4)

#define DHCP6_LOCAL_PORT	546
#define DHCP6_SERVER_PORT	547

/*
 * Normal packet format, defined in section 6 of RFC 3315
 */
struct dhcpv6_packet {
	unsigned char msg_type;
	unsigned char transaction_id[3];
	unsigned char options[DHCP6_OPTION_LEN];
};

