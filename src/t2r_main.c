#define	__MODULE__	"T2R-MAIN"
#define	__IDENT__	"X.00-11"
#define	__REV__		"00.11.00"

/*
**++
**
**  FACILITY: A yet another TCP to RTU gateway for MODBUS
**
**  ENVIRONMENT: Linux
**
**  ABSTRACT:
**
**  DESCRIPTION: Main routine, read and process connfiguration, start workers.
**
**  AUTHORS: StarLet Squad and Ruslan R. Laishev (AKA: BadAss sysman)
**
**  CREATION DATE:  10-SEP-2025
**
**
**  USAGE:
**	$ tootoo3_cgw [options]
**		options:
**			/TRACE		- enable extensible debug diagnostic output
**			/LOGFILE	- a file name for the logfile
**			/SETTINGS	- configuration option for network and serial stuff
**
**  MODIFICATION HISTORY:
**
**	13-NOV-2025	RRL	X.00-02 - pre-Release candidat
**
**	18-NOV-2025	RRL	X.00-03 - Improved TS configuratiuon section by using Unit\Slave Id to deterrminate  TS request
**
**	25-AUG-2026	RRL	X.00-04 / REV: 00.04.00 - Audit fixes:
**				s_settings_process_serials(): flat parsing with an early reject of illformed
**				records, mandatory <device> and <chars>, validation of the line parameters
**				(a division by zero at baud=0), the values of <rs485>/<ts_enabled> keys are
**				checked (a mere presence of the key was enabling the feature), a normalized
**				inter-PDU timespec (t3.5 by the spec), a bound check of the table capacity,
**				a default answer timeout T2R$K_ANSTMO_MSEC, libconfig strings are collapsed
**				in a local copy;
**				s_settings_process_listeners(): the same rework - a missing <bind> was a
**				NULL dereference, sscanf() field widths, port range and inet_pton() checks,
**				the UDP transport is rejected explicitly (was accepting and failing),
**				<target> is stored as a plain ASCIIZ string (was ASCIC into char[]);
**				settings_load_n_parse(): a failure of the serials section is not lost;
**				g_settings -> g_settings; internal routines are static now;
**				main(): returns EXIT_SUCCESS/EXIT_FAILURE (was a severity code).
**
**	25-AUG-2026	RRL	X.00-04ECO01 / REV: 00.04.01 - The validation limits are the T2R$K_* constants
**				now (magic numbers are gone), every rejection diagnostic does show the
**				allowed range of the parameter.
**
**	25-AUG-2026	RRL	X.00-04ECO02 / REV: 00.04.02 - Commenting only, no functional change:
**				every routine has been supplied by the standard DESCRIPTION/INPUTS/
**				OUTPUTS/RETURNS header block with the detailed parameters description.
**
**	25-AUG-2026	RRL	X.00-05 / REV: 00.05.00 - The delivery change (no C source change): the
**				StarLet set is consumed as an installed find_package(StarLet) package
**				instead of the 3rdparty/ submodule; the <install>/<uninstall> targets;
**				the installation sequence is documented in README.md.
**
**	25-AUG-2026	RRL	X.00-05ECO01 / REV: 00.05.01 - The obsolete modbus-t2r_startup.sh is
**				removed from the delivery.
**
**	25-AUG-2026	RRL	X.00-06 / REV: 00.06.00 - The revision banner and the exit status are
**				signalled via $PUTMSG_FAO by the coded catalogue messages (REVISNF,
**				EXITST); the bilingual (EN + RU) User Guide is added under docs/.
**
**	25-AUG-2026	RRL	X.00-06ECO01 / REV: 00.06.01 - The delivery change (no C source change):
**				the User Guides are delivered as the DEC/VSI-styled PDFs with the
**				diagrams; the organization line is "NaPi World & StarLet Squad
**				collaboration".
**
**	25-AUG-2026	RRL	X.00-07 / REV: 00.07.00 - The message catalogue is FAO-only now: the dead
**				printf-styled records are removed, NETCONN/NETDISCN are recreated as
**				the FAO records and are signalled at the T2R-NET accept/disconnect
**				points (see T2R-MSGS and T2R-NET).
**
**	25-AUG-2026	RRL	X.00-07ECO01 / REV: 00.07.01 - The delivery change (no C source change):
**				README gained the "Reliability, footprint & performance" section.
**
**	25-AUG-2026	RRL	X.00-07ECO02 / REV: 00.07.02 - The delivery change (no C source change):
**				README positions the project as an alternative to mbusd for the high
**				efficiency at a lower resource consumption case.
**
**	25-AUG-2026	RRL	X.00-07ECO03 / REV: 00.07.03 - The delivery change (no C source change):
**				README gained the "Who this project is for" section.
**
**	25-AUG-2026	RRL	X.00-07ECO04 / REV: 00.07.04 - The delivery change (no C source change):
**				README is delivered in two languages: README.md + README_RU.md.
**
**	25-AUG-2026	RRL	X.00-08 / REV: 00.08.00 - The build system change (no C source change):
**				the musl C library builds are supported and verified - added the toolchain
**				file cmake/musl.cmake, fixed the linking against a libconfig which lives in
**				a non-standard prefix; the musl build is documented in the both READMEs.
**
**	25-AUG-2026	RRL	X.00-08ECO01 / REV: 00.08.01 - Use the StarLet library facilities where the
**				code was doing the same by hand: $ISINRANGE() for the range validation,
**				$ARRSZ() for the message catalogue size, $MIN() for the dump clipping.
**
**	14-OCT-2026	RRL	X.00-09 / REV: 00.09.00 - The delivery change (no C source change): the
**				default installation prefix is /opt/mbusgw-t2r (sbin/, etc/,
**				share/doc/mbusgw-t2r/); the docs and the User Guides follow the new layout.
**				The "peer close connection" is logged as -I- (see T2R-NET).
**
**	14-OCT-2026	RRL	X.00-10 / REV: 00.10.00 - The RTU answer is framed by its expected length (see
**				T2R-TTY): the FTDI USB adapters do not cut the answers anymore; the udev rule
**				contrib/99-ftdi-latency.rules is delivered, the User Guides got the FTDI line
**				in the Troubleshooting.
**
**	14-OCT-2026	RRL	X.00-11 / REV: 00.11.00 - The logging is FAO-only now: every $LOG() is replaced
**				by $PUTMSG_FAO() with a coded catalogue message (T2R-MSGS: 63 new
**				records); the settings validation diagnostics are SERxxx/LSNxxx/CFGxxx;
**				the "Time Stamp responder ... disabled" is -I- now (it is the default);
**				the message records are initialized by the field names (no warnings with
**				any StarLet release, whatever the set of the EMSG_RECORD fields is).
**--
*/

#include	<signal.h>
#include	<stdint.h>
#include	<time.h>
#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<errno.h>
#include	<unistd.h>
#include	<libconfig.h>


#define		__FAC__	"T2R"
#define		__TFAC__ __FAC__ ": "					/* Special prefix for $TRACE			*/

#include	"utility_routines.h"
#include	"t2r_defs.h"
#include	"t2r_msgs.h"
#include	"t2r_modbus.h"

#ifndef	__ARCH__NAME__
#define	__ARCH__NAME__	"VAX"
#endif

#ifdef _DEBUG
	#ifndef	__TRACE__
		#define	__TRACE__
	#endif
#endif // DEBUG


static const	ASC	__ident__ = {$ASCINI(__FAC__ " "  __IDENT__ "/"  __ARCH__NAME__   "(built at "__DATE__ " " __TIME__ " with CC " __VERSION__ ")")},
	__rev__ = {$ASCINI(__REV__)};



static	EMSG_RECORD __t2r_msgs__ [] = {					/* Create array of message records */
	/*
	 * The record text starts with the message abbreviation only: the %<facility>-<severity>-
	 * frame is prefixed by __util$putmsg*() itself (see the .facname below), so the final
	 * output line looks like: %T2R-E-CRC16ERR, <the record text>
	 */
	#define	$DEF_MSG(f, s, c, t)	{ .sts = f##$__##c, .textl = 0, .text = #c ", " t },	/* The rest is stamped by __util$inimsg() */
	__DEF_MESSAGES__
	#undef	$DEF_MSG
};


static	EMSG_RECORD_DESC __t2r_msgs_desc__  = {
	.link = NULL,							/* Must beee NULL here */
	.facno = T2R,
	.msgnr = $ARRSZ(__t2r_msgs__),
	.msgrec = __t2r_msgs__,					/* An address of the message records array */
	.facname = "T2R"					/* The %T2R-<S>- framing of __util$putmsg*() */
};


ASC	g_logfspec = {0, {0}},
	g_confspec = {0, {0}},
	g_settings = {0, {0}}
	;


int	g_exit_flag = 0,						/* Global flag 'all must to be stop'	*/
	g_trace = 0;							/* A flag to produce extensible logging	*/

int	g_logsize = 0							/* A size in octets of the log file */
	;



T2R$_SERIAL	g_serials[T2R$K_MAX_SERIALS];				/* A table of serials, filled from configuration at startup time */
int		g_serials_nr;						/* Count of records in the table */

T2R$_LISTENER	g_listeners[T2R$K_MAX_LISTENERS];			/* A table of listeners, filled from configuration at startup time */
int		g_listeners_nr;						/* Count of records in the table */



config_t	g_cfg;							/* Lib Config API context */



static const OPTS g_optstbl [] =					/* General CLI options		*/
{
	{{$ASCINI("config")},	&g_confspec, ASC$K_SZ,		OPTS$K_CONF},
	{{$ASCINI("trace")},	&g_trace, 0,			OPTS$K_OPT},
	{{$ASCINI("logfile")},	&g_logfspec, ASC$K_SZ,		OPTS$K_STR},
	{{$ASCINI("logsize")},	&g_logsize, 0,			OPTS$K_INT},

	{{$ASCINI("settings")},	&g_settings,  ASC$K_SZ,		OPTS$K_STR},

	OPTS_NULL
};



const char	help [] = { "Usage:\n" \
	"$ %s [<options_list>]\n\n" \
	"\t/CONFIG=<file>     configuration options file path\n" \
	"\t/TRACE             enable extensible diagnostic output\n" \
	"\t/LOGFILE=<file>    a specification of file to accept logging\n" \
	"\t/LOGSIZE=<number>  a maximum size of file in octets\n" \

	"\n\tExample of usage:\n\t $ %s -config=t2r_config.conf /settings=t2r_settings.conf /trace\n" };


/*
 *   DESCRIPTION: Lookup a serial context for a given device in the global table
 *
 *   INPUTS:
 *	a_target:	A device name for looking for
 *
 *   OUTPUTS:
 *	a_serial:	A returned serial context
 *
 *   RETURNS:
 *	condition code
 */
static int s_settings_find_n_get_serial (
			const	char	*a_target,
			T2R$_SERIAL	**a_serial
		)
{
	/*
	 * Run over has been defined array of serials - lookup by give name
	 */
	for ( int i = 0; i < g_serials_nr; i++)
		{
		if ( !strncasecmp(a_target, g_serials[i].devname, T2R$K_TTY_DEVNAME) )
			return	*a_serial = &g_serials[i], STS$K_SUCCESS;
		}

	return	$PUTMSG_FAO(T2R$__TGTNDEF, a_target);
}




/*
 *  DESCRIPTION: Process settings for serial communication devices with validation,
 *	fill global table of serial devices by new records. An illformed record is rejected
 *	with a diagnostic, processing is continued from the next one.
 *
 *  INPUTS:
 *	a_cfg:		A context of LIBCONFIG for settings file
 *
 *  OUTPUTS:
 *	NONE
 *
 *  IMPLICITE OUTPUTS:
 *	g_serials, g_serials_nr
 *
 *  RETURNS:
 *	condition code
 *
 */
static int	s_settings_process_serials (config_t	*a_cfg)
{
config_setting_t *l_setting, *l_args;
const char *l_str;
int	l_count, l_int = -1, l_speed, l_databits, l_stopbits;
char	l_parity = 0;
T2R$_SERIAL	*l_serial;
char	l_chars[64];

	if ( !(l_setting = config_lookup(a_cfg, "serials")) )
		return	$PUTMSG_FAO(T2R$__CFGNOSECT, "serials", config_error_text(a_cfg));

	l_serial = g_serials;

	l_count = config_setting_length(l_setting);

	if ( l_count > T2R$K_MAX_SERIALS )					/* Never run out of the g_serials[] capacity */
		{
		$PUTMSG_FAO(T2R$__CFGTRUNC, T2R$K_MAX_SERIALS, l_count, "serials");
		l_count = T2R$K_MAX_SERIALS;
		}


	for (int i = 0; i < l_count; i++)
		{
		memset(l_serial, 0, sizeof(T2R$_SERIAL) );

		if ( !(l_args = config_setting_get_elem(l_setting, i)) )
			{
			$PUTMSG_FAO(T2R$__SERBADREC, i, "illformed or missing");
			continue;
			}

		if ( !config_setting_lookup_string(l_args, "device", &l_str) )
			{
			$PUTMSG_FAO(T2R$__SERBADREC, i, "no <device> option");
			continue;
			}

		strncpy(l_serial->devname, l_str, T2R$K_TTY_DEVNAME);		/* The tail is zeroed by the memset above */

		/*
		 * config_setting_lookup_int() reports a PRESENCE of the key, so the value must be
		 * checked separately - otherwise "rs485 = 0;" would enable the RS485 support.
		 */
		if ( config_setting_lookup_int(l_args, "rs485", &l_int) && l_int )
			{
#ifdef HAVE_TIOCRS485
			l_serial->flags |=  T2R$M_SERIAL_RS485;
#else
			$PUTMSG_FAO(T2R$__SERNORS485, i, l_serial->devname);
#endif
			}

		if ( !config_setting_lookup_string(l_args, "chars", &l_str) )
			{
			$PUTMSG_FAO(T2R$__SERBADOPT, i, l_serial->devname, "no <chars> option");
			continue;
			}

		/*
		 * The string is collapsed in a local copy: libconfig owns the original one and we
		 * must not modify it in place.
		 */
		if ( sizeof(l_chars) <= (size_t) snprintf(l_chars, sizeof(l_chars), "%s", l_str) )
			{
			$PUTMSG_FAO(T2R$__SERBADOPT, i, l_serial->devname, "<chars> value is too long");
			continue;
			}

		__util$collapse(l_chars, strlen(l_chars));			/* "115200, 8, N, 1" -> "115200,8,N,1" */

		if ( 4 != sscanf(l_chars, "%d,%d,%c,%d", &l_speed, &l_databits, &l_parity, &l_stopbits) )
			{
			$PUTMSG_FAO(T2R$__SERBADCHR, i, l_serial->devname, l_chars);
			continue;
			}

		/*
		 * Validate the line parameters BEFORE any arithmetic: <l_speed> is a divisor below.
		 */
		if ( !$ISINRANGE(l_speed, T2R$K_BAUD_MIN, T2R$K_BAUD_MAX) )
			{
			$PUTMSG_FAO(T2R$__SERRANGE, i, l_serial->devname, "speed (baud)", l_speed, T2R$K_BAUD_MIN, T2R$K_BAUD_MAX);
			continue;
			}

		if ( !$ISINRANGE(l_databits, T2R$K_DATABITS_MIN, T2R$K_DATABITS_MAX) )
			{
			$PUTMSG_FAO(T2R$__SERRANGE, i, l_serial->devname, "data bits", l_databits, T2R$K_DATABITS_MIN, T2R$K_DATABITS_MAX);
			continue;
			}

		if ( !$ISINRANGE(l_stopbits, T2R$K_STOPBITS_MIN, T2R$K_STOPBITS_MAX) )
			{
			$PUTMSG_FAO(T2R$__SERRANGE, i, l_serial->devname, "stop bits", l_stopbits, T2R$K_STOPBITS_MIN, T2R$K_STOPBITS_MAX);
			continue;
			}

		switch (l_parity = toupper (l_parity))
			{
			case	'N':
			case	'E':
			case	'O':
				break;

			default:
				$PUTMSG_FAO(T2R$__SERBADPAR, i, l_serial->devname, 1, &l_parity);
				continue;
			}

		/*
		 * Form context for serial device
		 */
		l_serial->fd = -1;
		l_serial->baud = l_speed;
		l_serial->parity = l_parity;
		l_serial->stopbits = l_stopbits;
		l_serial->databits = l_databits;

		l_serial->anstmo_msec = T2R$K_ANSTMO_MSEC;			/* A default which can be overriden below */

		if ( config_setting_lookup_int(l_args, "iotmo", &l_int) && (0 < l_int) )
			l_serial->anstmo_msec = l_int;

		/* A time to shift out a single octet: start + data + parity + stop bits */
		l_serial->onebyte_time_usec = (1000000 * (1 + l_serial->databits
			+ ((l_serial->parity == 'N') ? 0 : 1) + l_serial->stopbits)) / l_serial->baud;

		/*
		 * t3.5 - the inter-frame silent interval by the MODBUS over Serial Line specification:
		 * 3.5 character times, but not less than 1750 usec for the rates above 19200 baud.
		 */
		l_serial->inter_pdu_usec = (7 * l_serial->onebyte_time_usec) / 2;

		if ( (19200 < l_serial->baud) && (1750 > l_serial->inter_pdu_usec) )
			l_serial->inter_pdu_usec = 1750;

		l_serial->inter_pdu_ts.tv_sec = l_serial->inter_pdu_usec / 1000000;
		l_serial->inter_pdu_ts.tv_nsec = (long) (l_serial->inter_pdu_usec % 1000000) * 1000L;


		if ( config_setting_lookup_int(l_args, "ts_enabled", &l_int) && l_int )
			{
			l_serial->flags |=  T2R$M_SERIAL_ADDTS;

			if ( config_setting_lookup_int(l_args, "ts_fncode", &l_int) )
				l_serial->ts_fncode =  l_int;
			else	l_serial->ts_fncode = T2R$K_TS_FNCODE;

			if ( config_setting_lookup_int(l_args, "ts_base_reg0", &l_int) )
				l_serial->ts_base_reg0 =  l_int;
			else	l_serial->ts_base_reg0 = T2R$K_TS_BASE_REG0;

			if ( config_setting_lookup_int(l_args, "ts_unit", &l_int) )
				l_serial->ts_unit_nr =  l_int;
			else	l_serial->ts_unit_nr = T2R$K_TS_UNIT;
			}

		if ( (l_int = pthread_mutex_init(&l_serial->lock, NULL)) )
			{
			$PUTMSG_FAO(T2R$__DEVSYSERR, l_serial->devname, "pthread_mutex_init()", l_int, errno);
			continue;
			}

		$PUTMSG_FAO(T2R$__DEVADDED, i, l_serial->devname, l_serial->baud, l_serial->databits, 1, &l_parity,
			l_serial->stopbits, l_serial->anstmo_msec, l_serial->inter_pdu_usec);

		if ( l_serial->flags &  T2R$M_SERIAL_ADDTS )
			$PUTMSG_FAO(T2R$__TSENABLED, l_serial->devname, l_serial->ts_unit_nr, l_serial->ts_fncode, l_serial->ts_base_reg0);
		else	$PUTMSG_FAO(T2R$__TSDISABLED, l_serial->devname);


		l_serial++;
		g_serials_nr++;
		}

	return	g_serials_nr ? STS$K_SUCCESS : $PUTMSG_FAO(T2R$__NOSERIALS);
}




/*
 *  DESCRIPTION: Process settings for network listeners with validation, fill global table of
 *	listeners by new records. An illformed record is rejected with a diagnostic, processing
 *	is continued from the next one.
 *
 *  INPUTS:
 *	a_cfg:		A context of LIBCONFIG for settings file
 *
 *  OUTPUTS:
 *	NONE
 *
 *  IMPLICITE OUTPUTS:
 *	g_listeners, g_listeners_nr
 *
 *  RETURNS:
 *	condition code
 *
 */
static int	s_settings_process_listeners (config_t	*a_cfg)
{
config_setting_t *l_setting, *l_args;
const char *l_str;
int	l_count, l_int = -1, l_port_nr;
T2R$_LISTENER	*l_listener;
char	l_bind[128], l_proto[16], l_laddr[32], l_port[8];

	if ( !(l_setting = config_lookup(a_cfg, "listeners")) )
		return	$PUTMSG_FAO(T2R$__CFGNOSECT, "listeners", config_error_text(a_cfg));

	l_listener = g_listeners;

	l_count = config_setting_length(l_setting);

	if ( l_count > T2R$K_MAX_LISTENERS )					/* Never run out of the g_listeners[] capacity */
		{
		$PUTMSG_FAO(T2R$__CFGTRUNC, T2R$K_MAX_LISTENERS, l_count, "listeners");
		l_count = T2R$K_MAX_LISTENERS;
		}


	for (int i = 0; i < l_count; i++)
		{
		memset(l_listener, 0, sizeof(T2R$_LISTENER));			/* Never reuse a leftover of a rejected record */

		if ( !(l_args = config_setting_get_elem(l_setting, i)) )
			{
			$PUTMSG_FAO(T2R$__LSNBADREC, i, "illformed or missing");
			continue;
			}

		if ( !config_setting_lookup_string(l_args, "bind", &l_str) )
			{
			$PUTMSG_FAO(T2R$__LSNBADREC, i, "no <bind> option");
			continue;
			}

		/*
		 * The string is collapsed in a local copy: libconfig owns the original one and we
		 * must not modify it in place.
		 */
		if ( sizeof(l_bind) <= (size_t) snprintf(l_bind, sizeof(l_bind), "%s", l_str) )
			{
			$PUTMSG_FAO(T2R$__LSNBADREC, i, "<bind> value is too long");
			continue;
			}

		__util$collapse(l_bind, strlen(l_bind));			/* "TCP: 0.0.0.0: 502" -> "TCP:0.0.0.0:502" */

		if ( 3 != sscanf(l_bind, "%15[TCPUDPtcpudp]:%31[0-9.]:%7[0-9]", l_proto, l_laddr, l_port) )
			{
			$PUTMSG_FAO(T2R$__LSNBADBIND, i, l_bind);
			continue;
			}

		switch ( toupper (*l_proto) )
			{
			case	'T':
				l_listener->proto = IPPROTO_TCP;
				break;

			case	'U':						/* listen()/accept() on SOCK_DGRAM does not work */
				$PUTMSG_FAO(T2R$__LSNBADREC, i, "UDP transport is not implemented yet");
				continue;

			default:
				$PUTMSG_FAO(T2R$__LSNBADBIND, i, l_bind);
				continue;
			}

		if ( !$ISINRANGE((l_port_nr = atoi(l_port)), T2R$K_PORT_MIN, T2R$K_PORT_MAX) )
			{
			$PUTMSG_FAO(T2R$__LSNRANGE, i, l_port_nr, T2R$K_PORT_MIN, T2R$K_PORT_MAX);
			continue;
			}

		l_listener->sk.sin_family = AF_INET;
		l_listener->sk.sin_port = htons ((uint16_t) l_port_nr);

		if ( 1 != (l_int = inet_pton(AF_INET, l_laddr, &l_listener->sk.sin_addr)) )
			{
			$PUTMSG_FAO(T2R$__LSNBADADDR, i, l_laddr, l_int, errno);
			continue;
			}

		if ( config_setting_lookup_int(l_args, "iotmo", &l_int) )
			l_listener->iotmo = l_int;
		if ( config_setting_lookup_int(l_args, "connlm", &l_int) )
			l_listener->connlm = l_int;

		if ( !config_setting_lookup_string(l_args, "target", &l_str) )
			{
			$PUTMSG_FAO(T2R$__LSNBADREC, i, "no <target> device");
			continue;
			}

		if ( !(1 & s_settings_find_n_get_serial (l_str, &l_listener->serial)) )
			{
			$PUTMSG_FAO(T2R$__LSNNOTGT, i, l_str);
			continue;
			}

		strncpy(l_listener->target, l_str, T2R$K_TTY_DEVNAME);		/* The tail is zeroed by the memset above */


		$PUTMSG_FAO(T2R$__LSNADDED, i, l_listener->serial->devname, (l_listener->proto == IPPROTO_UDP) ? "UDP": "TCP",
			&l_listener->sk.sin_addr, ntohs(l_listener->sk.sin_port), l_listener->iotmo, l_listener->connlm);


		l_listener++;
		g_listeners_nr++;
		}


	return	g_listeners_nr ? STS$K_SUCCESS : $PUTMSG_FAO(T2R$__NOLSNRS);
}




/*
 *   DESCRIPTION: Load the settings file by the LIBCONFIG API and process its sections: the
 *	<serials> table first (the listeners refer to the serial devices), then the <listeners>
 *	table. A failure of any stage aborts the whole processing.
 *
 *   INPUTS:
 *	a_settings_conf: A file specification of the settings file
 *
 *   OUTPUTS:
 *	NONE
 *
 *   IMPLICITE OUTPUTS:
 *	g_cfg, g_serials, g_serials_nr, g_listeners, g_listeners_nr
 *
 *   RETURNS:
 *	condition code
 */
static int	s_settings_load_n_parse (
		const char	*a_settings_conf
		)
{
int	l_rc;

	config_init(&g_cfg);
#ifdef	CONFIG_OPTION_IGNORECASE
	config_set_options (&g_cfg, CONFIG_OPTION_IGNORECASE);
#endif

	/* Read the file. If there is an error, report it and exit. */
	if( !config_read_file(&g_cfg, a_settings_conf))
		{
		$PUTMSG_FAO(T2R$__CFGPARSE, config_error_file(&g_cfg), config_error_line(&g_cfg), config_error_text(&g_cfg), errno);
		config_destroy(&g_cfg);
		return	STS$K_ERROR;
		}

	if ( !(1 & (l_rc = s_settings_process_serials (&g_cfg))) )
		return	l_rc;

	return	s_settings_process_listeners (&g_cfg);
}




/*
 *   DESCRIPTION: Validate the has been accepted run-time parameters as a whole, show them in
 *	the log for the operational control.
 *
 *   INPUTS:
 *	NONE
 *
 *   OUTPUTS:
 *	NONE
 *
 *   IMPLICITE INPUTS:
 *	g_settings, g_logfspec
 *
 *   RETURNS:
 *	condition code
 */
static int	s_config_validation	(void)
{
int l_rc;

	l_rc = s_settings_load_n_parse ($ASCPTR(&g_settings));
	return	l_rc;
}



/*
 *   DESCRIPTION: A handler of the asynchronous signals. The termination signals (SIGTERM,
 *	SIGINT, SIGQUIT) do set the <g_exit_flag>, a repeated one terminates the process
 *	immediately. SIGUSR1 toggles the tracing at run-time. Only the async-signal-safe calls
 *	are used here: write() instead of fprintf()/fflush().
 *
 *   INPUTS:
 *	a_signo:	A number of the has been delivered signal
 *
 *   OUTPUTS:
 *	NONE
 *
 *   IMPLICITE OUTPUTS:
 *	g_exit_flag, g_trace
 *
 *   RETURNS:
 *	NONE
 */
static	void	s_sig_handler (int a_signo)
{
	/*
	 * Only the async-signal-safe calls are allowed here: write() instead of fprintf()/fflush().
	 */
	if ( g_exit_flag )
		{
		static const char l_msg [] = "Exit flag has been set, exiting ...\n";
		if ( 0 > write(STDOUT_FILENO, l_msg, sizeof(l_msg) - 1) ) {;}

		_exit(a_signo);
		}


	if ( (a_signo == SIGTERM) || (a_signo == SIGINT) || (a_signo == SIGQUIT))
		{
		static const char l_msg [] = "Got a termination signal, set exit_flag!\n";
		if ( 0 > write(STDOUT_FILENO, l_msg, sizeof(l_msg) - 1) ) {;}

		g_exit_flag = 1;
		return;
		}
	else if ( a_signo == SIGUSR1)
		{
		g_trace = !g_trace;

		if ( g_trace )
			{
			static const char l_msg [] = "Set /TRACE=ON\n";
			if ( 0 > write(STDOUT_FILENO, l_msg, sizeof(l_msg) - 1) ) {;}
			}
		else	{
			static const char l_msg [] = "Set /TRACE=OFF\n";
			if ( 0 > write(STDOUT_FILENO, l_msg, sizeof(l_msg) - 1) ) {;}
			}

		return;
		}
	else	{
		static const char l_msg [] = "Got an unexpected signal\n";
		if ( 0 > write(STDOUT_FILENO, l_msg, sizeof(l_msg) - 1) ) {;}
		}

	_exit(a_signo);
}

/*
 *   DESCRIPTION: Establish the handler of the asynchronous signals for the termination and the
 *	trace toggling, ignore SIGPIPE (a write to a has been closed socket must be seen as an
 *	EPIPE error, not as a process termination).
 *
 *   INPUTS:
 *	NONE
 *
 *   OUTPUTS:
 *	NONE
 *
 *   RETURNS:
 *	NONE
 */
static void	s_init_sig_handler(void)
{
const int siglist [] = {SIGTERM, SIGINT, SIGUSR1, SIGQUIT, 0 };

	/*
	 * Establishing a signals handler
	 */
	signal(SIGPIPE, SIG_IGN);	/* We don't want to crash the server due fucking unix shit */

	for ( int i = 0; siglist[i]; i++)
		{
		if ( (signal(siglist[i], s_sig_handler)) == SIG_ERR )
			$PUTMSG_FAO(T2R$__SIGERR, siglist[i], errno);

		$IFTRACE(g_trace, "Set handler for signal %d/%#x (%s)", siglist[i], siglist[i], strsignal(siglist[i]));
		}
}



/*
 *   DESCRIPTION: Main entry point of the program: accept and parse the command line arguments,
 *	load and validate the run-time parameters, establish the signal handling, start the
 *	network subsystem and stay in the main loop until the exit flag is set, then stop the
 *	subsystems in the reverse order.
 *
 *   INPUTS:
 *	argc:		A count of the command line arguments
 *	argv:		An array of the command line arguments; see g_optstbl for the accepted
 *			options (/CONFIG, /TRACE, /LOGFILE, /LOGSIZE, /SETTINGS)
 *
 *   OUTPUTS:
 *	NONE
 *
 *   RETURNS:
 *	EXIT_SUCCESS - a normal termination by the exit flag, EXIT_FAILURE - otherwise
 */

int	main(int argc, char **argv)
{
int l_rc = 0;


	__util$inimsg(&__t2r_msgs_desc__);

	$PUTMSG_FAO(T2R$__REVISNF, &__ident__, &__rev__);

	/*
	 * Process command line arguments
	 */
	if ( !(1 & __util$getparams(argc, argv, g_optstbl)) )
		return	$PUTMSG_FAO(T2R$__CFGERR);


	if ( $ASCLEN(&g_logfspec) )
		{
		__util$deflog($ASCPTR(&g_logfspec), NULL);

		$PUTMSG_FAO(T2R$__REVISNF, &__ident__, &__rev__);
		}

	if ( g_trace )
		__util$showparams(g_optstbl);

	if ( !(1 & s_config_validation()) )
		return	$PUTMSG_FAO(T2R$__CFGABORT);



	s_init_sig_handler ();

	if ( (1 & (l_rc = t2r$net_start_listeners ())) )
		{
		while ( !g_exit_flag)
			{
			for (int delay = 3;  (delay = sleep(delay)); );
			}
		l_rc = t2r$net_stop_listeners ();
		}

	$PUTMSG_FAO(T2R$__EXITST, g_exit_flag, l_rc);

	return	(1 & l_rc) ? EXIT_SUCCESS : EXIT_FAILURE;
}
