/*
*
*  DHCPv6 load generator
*
*
*/

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <signal.h>
#include <time.h>
#include <assert.h>
#include <sys/time.h>
#include <sys/types.h>
#ifdef linux
 #include <sys/ioctl.h>
#endif
#include <poll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "dhcp.h"

static const char hex[17] = "0123456789abcdef";

/* Local Definitions */
#define MAX_TOKENS		 54
#define MAX_DUID_LEN		130
#define DUID_LLT_LEN	 	 14
#define MAX_IA			 16

typedef struct {
	uint32_t	advertise_latency_avg;
	uint32_t	advertise_latency_min;
	uint32_t	advertise_latency_max;

	uint32_t	reply_latency_avg;
	uint32_t	reply_latency_min;
	uint32_t	reply_latency_max;

	uint32_t	solicits_sent;
	uint32_t	requests_sent;
	uint32_t	releases_sent;
	uint32_t	declines_sent;
	uint32_t	informs_sent;
	uint32_t	confirms_sent;
	uint32_t	renews_sent;
	uint32_t	rebinds_sent;

	uint32_t	solicit_acks_received;
	uint32_t	solicit_naks_received;
	uint32_t	request_acks_received;
	uint32_t	request_naks_received;
	uint32_t	decline_acks_received;
	uint32_t	decline_naks_received;
	uint32_t	release_acks_received;
	uint32_t	release_naks_received;
	uint32_t	inform_acks_received;
	uint32_t	inform_naks_received;
	uint32_t	confirm_acks_received;
	uint32_t	confirm_naks_received;
	uint32_t	renew_acks_received;
	uint32_t	renew_naks_received;
	uint32_t	rebind_acks_received;
	uint32_t	rebind_naks_received;

	uint32_t	solicit_ack_timeouts;
	uint32_t	request_ack_timeouts;
	uint32_t	renew_ack_timeouts;
	uint32_t	rebind_ack_timeouts;
	uint32_t	release_ack_timeouts;
	uint32_t	decline_ack_timeouts;
	uint32_t	inform_ack_timeouts;
	uint32_t	confirm_ack_timeouts;

	uint32_t	errors;
	uint32_t	failed;
	uint32_t	completed;
} dhcp_stats_t;

typedef struct IA_DATA_T {
	uint32_t		iaid;
	struct in6_addr		ipaddr;
	uint8_t			prefix_len;
} ia_data_t;

typedef struct DHCP_SESSION_T {
	uint8_t			*transaction_id;
	uint32_t		state;
	uint8_t			options[DHCP6_OPTION_LEN];
	uint8_t			options_length;
	uint32_t		timeouts;
	uint32_t		session_start;
	uint32_t		lease_time;
	struct timeval		last_sent;
	struct timeval		last_received;
	uint8_t			type_last_received;
	uint8_t			type_last_sent;
	uint8_t			serverid[MAX_DUID_LEN];
	uint16_t		serverid_len;
	uint8_t			mac[6];
	char			hostname[64];
	ia_data_t		ia[MAX_IA];
	uint8_t			num_ia;
	uint8_t			recv_ia;
} dhcp_session_t;

typedef struct DHCP_SERVER_T {
	struct sockaddr_in6	sa;
	dhcp_stats_t		stats;
	dhcp_session_t		*list;
	uint32_t		active;
	struct timeval		first_packet_sent;
	struct timeval		last_packet_sent;
	struct timeval		last_packet_received;
	struct DHCP_SERVER_T	*next;
} dhcp_server_t;


typedef struct LEASE_DATA_T {
	uint8_t			mac[6];
	uint8_t			serverid[MAX_DUID_LEN];
	uint16_t		serverid_len;
	struct in6_addr		sa;
	char			*hostname;
	struct LEASE_DATA_T	*next;
	ia_data_t		ia[MAX_IA];
	uint8_t			num_ia;
} lease_data_t;

// Session States
#define UNALLOCATED		0
#define SESSION_ALLOCATED	1
#define SOLICIT_SENT		(1<<1)
#define RAPID_SOLICIT_SENT	(1<<2)
#define RENEW_SENT		(1<<3)
#define REQUEST_SENT		(1<<4)
#define RELEASE_SENT		(1<<5)
#define DECLINE_SENT		(1<<6)
#define INFORM_SENT		(1<<7)
#define REBIND_SENT		(1<<8)
#define CONFIRM_SENT		(1<<9)
#define SOLICIT_ACK		(1<<10)
#define SOLICIT_NAK		(1<<11)
#define REQUEST_ACK		(1<<12)
#define REQUEST_NAK		(1<<13)
#define RENEW_ACK		(1<<14)
#define RENEW_NAK		(1<<15)
#define REBIND_ACK		(1<<16)
#define REBIND_NAK		(1<<17)
#define RELEASE_ACK		(1<<18)
#define RELEASE_NAK		(1<<19)
#define DECLINE_ACK		(1<<20)
#define DECLINE_NAK		(1<<21)
#define INFORM_ACK		(1<<22)
#define INFORM_NAK		(1<<23)
#define CONFIRM_ACK		(1<<24)
#define CONFIRM_NAK		(1<<25)
#define PACKET_ERROR		(1<<26)

#define DELTATV(a,b) (1000000*(a.tv_sec - b.tv_sec) + a.tv_usec - b.tv_usec)

/* Globals */
__const char *typestrings[] = {"SOLICIT", "ADVERTISE", "REQUEST",
		"CONFIRM", "RENEW", "REBIND", "REPLY", "RELEASE", "DECLINE",
		"RECONFIGURE", "INFORMATION_REQUEST", "RELAY_FORW", "RELAY_REPL",
		"LEASEQUERY", "LEASEQUERY_REPLY"};
__const char *optionstrings[] = { "CLIENTID", "SERVERID", "IA_NA",
		"IA_TA", "IAADDR", "ORO", "PREFERENCE", "ELAPSED_TIME",
		"RELAY_MSG", "!UNASSIGNED!", "AUTH", "UNICAST", "STATUS_CODE",
		"RAPID_COMMIT", "USER_CLASS", "VENDOR_CLASS", "VENDOR_OPTS",
		"INTERFACE_ID", "RECONF_MSG", "RECONF_ACCEPT", "SIP_SERVERS_DNS",
		"SIP_SERVERS_ADDR", "NAME_SERVERS", "DOMAIN_SEARCH", "IA_PD",
		"IAPREFIX", "NIS_SERVERS", "NISP_SERVERS", "NIS_DOMAIN_NAME",
		"NISP_DOMAIN_NAME", "SNTP_SERVERS", "INFORMATION_REFRESH_TIME",
		"BCMCS_SERVER_D", "BCMCS_SERVER_A", "!UNASSIGNED!", "GEOCONF_CIVIC",
		"REMOTE_ID", "SUBSCRIBER_ID", "CLIENT_FQDN", "PANA_AGENT",
		"NEW_POSIX_TIMEZONE", "NEW_TZDB_TIMEZONE", "ERO", "LQ_QUERY",
		"CLIENT_DATA", "CLT_TIME", "LQ_RELAY_DATA", "LQ_CLIENT_LINK" };
static __const char	*version="Tue Aug 27 12:58:20 PDT 2013";
static uint32_t		socket_bufsize=128*1024;
static uint32_t		send_delay;
static uint32_t		ia_id;
static int		use_sequential_mac;
static int		send_hostname;
static int		random_hostname;
static int		send_release;
static int		send_decline;
//static int		send_info_request;
static int		start_from = DHCPV6_RENEW;
static int		retransmit;
static int		dhcp_ping;
static uint8_t		request_prefix;
//static int		renew_lease;
static int		send_until_answered;
static int		verbose;
static int		sock = -1;
static int		use_relay;
static int		rapid_commit;
static int		server_should_ddns = 1;
static uint32_t		timeout = 5000000UL;
static uint32_t		number_requests=1;
static uint32_t		max_sessions = 25;
static uint32_t		num_per_mac = 1;
static dhcp_server_t	*servers;
static uint32_t		num_servers;
static struct in6_addr	srcaddr = IN6ADDR_ANY_INIT;
static uint8_t		firstmac[6];
static char		*update_domain;
static char		*logfile;
static char		*input_file;
static char		*output_file;
FILE			*logfp;
static FILE		*outfp;
static time_t		start_time;
static uint16_t		sol_optseq[64], req_optseq[64], ren_optseq[64];
static int		opt_seq;

static int nrequests = 0;
static uint16_t *info_requests;

static int nextraoptions = 0;
static struct option_st {
    int option_no;
    int data_len;
    char *data;
} **extraoptions = NULL;

/* Function prototypes */
static void			reader(void);
static void			sender(void);
static void			fill_session(dhcp_session_t *, lease_data_t *);
static int			process_packet(void *, struct timeval *, uint32_t);
static int			send_packet6(uint8_t, dhcp_session_t *, dhcp_server_t *);
static void			parse_args(int , char **);
static int			add_servers(const char *);
static uint32_t			read_lease_data(lease_data_t **);
static void			usage(void);
static int			process_sessions(void);
static void			print_packet(uint32_t, struct dhcpv6_packet *, const char *);
static int			test_statistics(void);
static struct in6_addr		get_local_addr(void);
static int			addoption(int , char *);
static dhcp_session_t		*find_free_session(dhcp_server_t *);
static int			addoption(int , char *);
void 				getmac(uint8_t *);
int				get_tokens(char *, char **, int);
void				print_lease( FILE *, dhcp_session_t *,struct in6_addr *);
int				pack_client_fqdn(uint8_t *, char *);
void				decode_state(uint32_t);
int				encode_domain(char *, uint8_t *);
int 				fill_iafu_mess( dhcp_session_t *, uint8_t *);
int				parse_opt_seq( char *);
int				add_opt_seq(uint8_t, uint8_t *, dhcp_session_t *);
