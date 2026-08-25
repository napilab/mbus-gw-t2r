#define	__MODULE__	"T2R-NET"
#define	__IDENT__	"X.00-07"
#define	__REV__		"00.07.00"

/*
**++
**
**  FACILITY: A yet another TCP to RTU gateway for MODBUS
**
**  ENVIRONMENT: Linux
**
**  DESCRIPTION: A set of routines related to network I/O
**
**  AUTHORS: StarLet Squad and Ruslan R. Laishev (AKA: BadAss sysman)
**
**  CREATION DATE:  26-SEP-2025
**
**  MODIFICATION HISTORY:
**
**	25-AUG-2026	RRL	X.00-04 / REV: 00.04.00 - Audit fixes:
**				s_net_listener(): fixed a detection of the accept() failure (the condition
**				was always false), calloc() check, sockaddr length is reset before every
**				accept(), EINTR/error handling of poll();
**				s_net_session(): pthread_detach() - the session threads were never joined
**				(a resource leak), added the missing return statement;
**				s_net_mbap_rx(): the MBAP header is not parsed until it is received as a
**				whole (a read of an uninitialized memory), added the protocol identifier
**				and the minimal length checks, fixed the missing $LOG argument;
**				t2r$net_start_listeners(): the target device is opened BEFORE the socket
**				is published to the poll() array, the pollfd array is initialized properly,
**				the listening socket is not leaked on the error paths.
**
**	25-AUG-2026	RRL	X.00-04ECO02 / REV: 00.04.02 - Commenting only, no functional change:
**				every routine has been supplied by the standard DESCRIPTION/INPUTS/
**				OUTPUTS/RETURNS header block with the detailed parameters description.
**
**	25-AUG-2026	RRL	X.00-06 / REV: 00.06.00 - The listener lifecycle diagnostics are signalled
**				via $PUTMSG_FAO by the coded catalogue messages now: LSNRRDY, LSNRERR.
**
**	25-AUG-2026	RRL	X.00-07 / REV: 00.07.00 - The client connect/disconnect events are
**				signalled via $PUTMSG_FAO by the coded NETCONN/NETDISCN messages;
**				the duplicating "Start session" line is removed (NETCONN covers it).
**
**--
*/

#include	<time.h>
#include	<stdio.h>
#include	<stdlib.h>
#include	<errno.h>
#include	<unistd.h>
#include	<poll.h>
#include	<netinet/tcp.h>


#define		__FAC__	"T2R"
#define		__TFAC__ __FAC__ ": "					/* Special prefix for $TRACE			*/

#include	"utility_routines.h"
#include	"t2r_defs.h"
#include	"t2r_msgs.h"
#include	"t2r_modbus.h"


extern int	g_exit_flag, g_trace;


extern T2R$_LISTENER	g_listeners[];
extern int		g_listeners_nr;

extern T2R$_SERIAL	g_serials[];						/* Is used by t2r$net_stop_listeners() */
extern int		g_serials_nr;
static struct pollfd	s_pfd_lsnr[T2R$K_MAX_LISTENERS];			/* Initialized in t2r$net_start_listeners() */

static const	int	s_one = 1;


/*
 *   DESCRIPTION: Read a next portion of data from the session's TCP socket into the network
 *	buffer of the session. A single recv() call is performed: the routine is driven by the
 *	session's state machine and is called until <datalen> octets are accumulated.
 *
 *   INPUTS:
 *	a_session:	A context of the network session; .sd - socket, .datalen - octets still
 *			to be read, .netbuf_dsc - a descriptor of the accumulating buffer
 *
 *   OUTPUTS:
 *	a_session:	.netbuf_dsc->len is advanced by the has been read octets,
 *			.datalen is decreased accordingly
 *
 *   RETURNS:
 *	condition code; STS$K_WARN - the peer has closed the connection
 */
static int	s_net_recvn (
		T2R$_SESSION	*a_session
		)
{
int	l_rc;
T2R$_DESC	*l_netbuf_dsc = a_session->netbuf_dsc;

//	if ( !a_session->datalen )							/* Just to check that we need to do anything ? */
//		return	$LOG(STS$K_WARN, "[#%d] --- no data to be read", a_session->sd), STS$K_SUCCESS;


											/* Read a data from socket */
	if ( !(l_rc = recv(a_session->sd, l_netbuf_dsc->data + l_netbuf_dsc->len, a_session->datalen, MSG_NOSIGNAL)) )
		return	$LOG(STS$K_WARN, "[#%d] --- peer close connection, errno: %d", a_session->sd, errno);

	if ( 0 > l_rc )									/* Check return status */
		return	$LOG(STS$K_ERROR, "[#%d] --- error during read data, recv()->%d, errno: %d", a_session->sd, l_rc, errno);


	l_netbuf_dsc->len += l_rc;							/* Adjust count of data in the buffer */
	a_session->datalen -= l_rc;							/* Adjust data to be read at next iteration ? */


	return	STS$K_SUCCESS;
}



/*
 *   DESCRIPTION: Send a next portion of data from the network buffer of the session to the
 *	session's TCP socket. A single send() call is performed: the routine is called by the
 *	session's state machine until the whole buffer is transmitted.
 *
 *   INPUTS:
 *	a_session:	A context of the network session; .sd - socket, .datalen - octets already
 *			sent, .netbuf_dsc - a descriptor of the buffer to be transmitted
 *
 *   OUTPUTS:
 *	a_session:	.datalen is advanced by the has been sent octets
 *
 *   RETURNS:
 *	condition code; STS$K_WARN - the peer has closed the connection
 */
static int	s_net_sendn (
		T2R$_SESSION	*a_session
		)
{
int	l_rc;
T2R$_DESC	*l_netbuf_dsc = a_session->netbuf_dsc;

	if ( a_session->datalen == l_netbuf_dsc->len )					/* Just to check that we need to do anything ? */
		return	$LOG(STS$K_WARN, "[#%d] --- all data has been sent (%d octets)", a_session->sd, a_session->datalen), STS$K_SUCCESS;


											/* Read a data from socket */
	if ( !(l_rc = send(a_session->sd, l_netbuf_dsc->data + a_session->datalen, l_netbuf_dsc->len - a_session->datalen, MSG_NOSIGNAL)) )
		return	$LOG(STS$K_WARN, "[#%d] --- peer close connection, errno: %d", a_session->sd, errno);

	if ( 0 > l_rc )									/* Check return status */
		return	$LOG(STS$K_ERROR, "[#%d] --- error during send data, send()->%d, errno: %d", a_session->sd, l_rc, errno);


	a_session->datalen += l_rc;							/* Adjust data to be send at next iteration ? */

	return	STS$K_SUCCESS;
}






/*
 *   DESCRIPTION: Check and validate has been read data in the buffer, compute a length of data to be read from network socket,
 *	read from socket.
 *
 *   INPUTS:
 *	a_session:	A session context
 *
 *   OUTPUTS:
 *	NONE:
 *
 *   IMPLICITE OUTPUTS:
 *	a_session:	update data buffer and counters
 *
 *   RETURNS:
 *	T2R$__FULLMBAP:		Whole MBAP PDU in the buffer
 *	<condition code>
 */
static int	s_net_mbap_rx (
		T2R$_SESSION	*a_session
		)
{
int	l_rc, l_len;
MODBUS_ADU_T	*l_mbaph;
T2R$_DESC	*l_netbuf_dsc = a_session->netbuf_dsc;

	a_session->datalen = l_netbuf_dsc->len ? a_session->datalen : MODBUS$SZ_MINMBAP;

	if ( !(1 & (l_rc = s_net_recvn(a_session))) )
		return	l_rc;

	/*
	 * A peer is free to deliver the header by pieces, so we do not touch any field of the MBAP
	 * until a whole header is in the buffer - otherwise we would parse an uninitialized memory.
	 */
	if ( l_netbuf_dsc->len < MODBUS$SZ_MINMBAP )
		return	STS$K_SUCCESS;							/* Keep reading the rest of the header */

	l_mbaph = (MODBUS_ADU_T *) l_netbuf_dsc->data;

	if ( ntohs(l_mbaph->proto) )							/* 0 - is the MODBUS protocol identifier */
		return	$LOG(STS$K_ERROR, "[#%d] --- unexpected protocol identifier %d", a_session->sd,
				ntohs(l_mbaph->proto));

	l_len = ntohs(l_mbaph->len);							/* Get length of the MBAP data part */

	if ( 2 > l_len )								/* Unit + at least a function code */
		return	$LOG(STS$K_ERROR, "[#%d] --- MODBUS PDU is too short (%d octets)", a_session->sd, l_len);

	l_len -= 1;									/* Unit field is already there */

	if ( l_len > MODBUS$SZ_MAXPDU )
		return	$LOG(STS$K_ERROR, "[#%d] --- MODBUS PDU is too long (%d > %d octets)", a_session->sd,
				l_len, MODBUS$SZ_MAXPDU );

	if ( l_netbuf_dsc->len == (l_len + MODBUS$SZ_MINMBAP) )			     	/* Is the whole MODBAS PDU in the buffer already ? */
		return	T2R$__FULLMBAP;

	if ( l_netbuf_dsc->len ==  MODBUS$SZ_MINMBAP )					/* Did we get whole MBAP Header ? */
		a_session->datalen = l_len;

	return	STS$K_SUCCESS;
}





/*
 *   DESCRIPTION: Check and validate has been read data in the buffer, compute a length of data to be read from network socket,
 *	read from socket.
 *
 *   INPUTS:
 *	a_session:	A session context
 *
 *   OUTPUTS:
 *	NONE:
 *
 *   IMPLICITE OUTPUTS:
 *	a_session:	update data buffer and counters
 *
 *   RETURNS:
 *	T2R$__FULLMBAP:		Whole MBAP PDU in the buffer
 *	<condition code>
 */
static int	s_net_mbap_tx (
		T2R$_SESSION	*a_session
		)
{
int	l_rc;

	if ( !(1 & (l_rc = s_net_sendn(a_session))) )
		return	l_rc;

	return	STS$K_SUCCESS;
}





/*
 *   DESCRIPTION: A worker to serves single TCP-client:
 *	- to forward MODBUS-like request to serial device
 *	- read RTU answer from serial MODBUS-capable device
 *	- form and send MODBUS-like answer to remote clien

 *
 *   INPUTS:
 *	a_session:	A session context
 *
 *   OUTPUTS:
 *	NONE:
 *
 *   RETURNS:
 *	NONE
 */

static void *	s_net_session( void *a_arg)
{
int	l_rc;
struct pollfd l_pfd = {0};
T2R$_SESSION	*l_session;
struct timespec	l_now;
T2R$_SERIAL	*l_serial;
uint8_t		l_excode;

$DESCRIPTOR_S	(l_netbuf_dsc, (2*MODBUS$SZ_MAXPDU));
$DESCRIPTOR_S	(l_rtu_req_dsc, (2*MODBUS$SZ_MAXPDU));
$DESCRIPTOR_S	(l_rtu_resp_dsc, (2*MODBUS$SZ_MAXPDU));

	pthread_detach(pthread_self());						/* Nobody joins us: release the TCB at exit */

	l_session = a_arg;
	assert ( l_session );
	l_serial = l_session->target;
	assert ( l_serial );


	l_pfd.fd = l_session->sd;
	l_pfd.events = POLLIN;

	l_session->state = T2R$K_STATE_READY;
	l_session->netbuf_dsc = &l_netbuf_dsc;
	l_session->datalen = MODBUS$SZ_MINADU;
	clock_gettime(CLOCK_MONOTONIC_COARSE, &l_session->lastio_ts);


	while ( !g_exit_flag )
		{
		/*
		 * Check session timeout and expiration
		 */
		clock_gettime(CLOCK_MONOTONIC_COARSE, &l_now);

		if ( l_session->state == T2R$K_STATE_READY )				/* Check global session timeout for idle session */
			{
			if ( (l_session->lastio_ts.tv_sec + T2R$K_IDLE_TMO_SEC) < l_now.tv_sec)	/* 1200 secs for idle session */
				{
				$LOG(STS$K_WARN, "[#%d] No activity for past %d seconds", l_session->sd, T2R$K_IDLE_TMO_SEC);
				break;
				}
			}
		else if ( (l_session->lastio_ts.tv_sec + T2R$K_NET_TMO_SEC) < l_now.tv_sec)	/* 3 secs for read\write MBAP */
				{
				$LOG(STS$K_ERROR, "[#%d] No data read (%d octets) in  %d seconds",
				     l_session->sd, l_session->datalen, T2R$K_NET_TMO_SEC );
				break;
				}



		/*
		 * Do main work for session ...
		 */
		l_rc = STS$K_SUCCESS;

		switch (l_session->state )
			{
			case	T2R$K_STATE_READY:
				if ( l_pfd.revents & POLLIN )
					l_session->state = T2R$K_STATE_PDU_RECV;

				break;


			case	T2R$K_STATE_PDU_RECV:
				if ( l_pfd.revents & POLLIN )
					{
					if ( T2R$__FULLMBAP  == (l_rc = s_net_mbap_rx(l_session)) )
						{
						$MBAP_PRINT("Rcvd", l_netbuf_dsc.data, l_netbuf_dsc.len);

						l_session->state = T2R$K_STATE_PDU_ENQD;
						}
					else	break;
					}
				else	break;

				/* FALLTHRU */					/* A whole MBAP is in the buffer - process it now */
			case	T2R$K_STATE_PDU_ENQD:
				if ( !(1 & (l_rc = t2r$mbap_2_rtu_pdu (&l_netbuf_dsc, &l_rtu_req_dsc)) ) )
					break;


				if ( !(1 & (l_rc = t2r$tty_exec_req (l_serial, &l_rtu_req_dsc, &l_rtu_resp_dsc, &l_excode))) )
					break;


				if ( l_excode )
					$LOG(STS$K_WARN, "[#%d] Request processing failed --- MODBUS's exception: %d/%#x",
					     l_session->sd, l_excode, l_excode);

				if ( !(1 & (l_rc = t2r$rtu_pdu_2_mbap (&l_rtu_resp_dsc, &l_netbuf_dsc)) ) )
					break;



				l_session->datalen = 0;				/* Set "sent octets" to zero */
				l_pfd.events |= POLLOUT;			/* Enable checking network socket for output data */
				l_session->state = T2R$K_STATE_PDU_XMIT;	/* State transition to next state */

				/* FALLTHRU */					/* Try to send the answer immediately */
			case T2R$K_STATE_PDU_XMIT:

				$MBAP_PRINT("Sending", l_netbuf_dsc.data, l_netbuf_dsc.len);

				if ( l_session->datalen == l_netbuf_dsc.len )	/* All data is sent to remote TCP client ? */
					{
					$MBAP_PRINT("Sent", l_netbuf_dsc.data, l_netbuf_dsc.len);

					l_pfd.events &= ~POLLOUT;		/* we don't need to check on socket for output */
					l_session->state = T2R$K_STATE_READY;	/* Do transition to initial state */
					l_session->datalen = l_netbuf_dsc.len = 0;

					continue;
					}

				if ( !(1 & (l_rc = s_net_mbap_tx (l_session))) )
					break;
			}


		if ( !(1 & l_rc) )
			{
			$IFTRACE(g_trace, "[#%d] state: %d,  condition code: %#x", l_session->sd, l_session->state, l_rc);
			break;
			}


		/*
		 * Check state of network socket ...
		 */
		if ( 0 > (l_rc = poll(&l_pfd, 1, 3000 /* 3 secs */)) )
			{
			if ( errno == EINTR )
				continue;

			$LOG(STS$K_ERROR, "[#%d] poll()->%d, errno: %d", l_session->sd, l_rc, errno);
			break;							/* A persistent error would give a busy loop */
			}
		else if ( l_pfd.revents & (POLLERR | POLLHUP | POLLNVAL) )
			{
			$LOG(STS$K_ERROR, "[#%d] poll()->%d, .revents: %#x, errno: %d", l_session->sd, l_rc, l_pfd.revents, errno);
			break;
			}
		else if (l_rc)
			clock_gettime(CLOCK_MONOTONIC_COARSE, &l_session->lastio_ts);
		}



	$PUTMSG_FAO(T2R$__NETDISCN, l_session->sd, &l_session->sk.sin_addr,
		ntohs(l_session->sk.sin_port));


	close(l_session->sd);
	free( (void *) l_session);

	return	NULL;
}





/*
 *   DESCRIPTION: A thread routine of the connections dispatcher. Waits (by polling the whole
 *	table of the listening sockets) for incoming TCP connection requests, accepts them,
 *	allocates a session context and starts a detached per-session thread. The loop is
 *	terminated by <g_exit_flag> or by a persistent poll() error.
 *
 *   INPUTS:
 *	a_arg:		Unused, is required by the pthread API only
 *
 *   OUTPUTS:
 *	NONE
 *
 *   IMPLICITE INPUTS:
 *	s_pfd_lsnr, g_listeners, g_listeners_nr, g_exit_flag
 *
 *   RETURNS:
 *	NULL - is required by the pthread API only
 */
static void *	s_net_listener( void *a_arg)
{
int	l_rc, l_sd;
struct sockaddr_in	l_sk = {0};
socklen_t l_slen;
T2R$_SESSION	*l_session;
pthread_t	l_tid;

	while ( !g_exit_flag )
		{
		/*
		 * Wait for any new TCP-connection request on all ports ...
		 */
		if ( 0 > (l_rc = poll(s_pfd_lsnr, g_listeners_nr, 5000)) )
			{
			if ( errno == EINTR )
				continue;

			$LOG(STS$K_ERROR, "poll()->%d, errno: %d --- listener is aborted", l_rc, errno);
			break;								/* A persistent error would give a busy loop */
			}

		if ( !l_rc )
			continue;							/* Timeout - .revents are not updated */


		/*
		 * Run over all listeners and check for POLLIN (TCP-connection request)
		 */
		for (int i = 0; i < g_listeners_nr; i++ )
			{
			if ( s_pfd_lsnr[i].revents & POLLIN )				/* Is there any connection request ? */
				{							/* Accept TCP connection */
				l_slen = sizeof(l_sk);					/* accept() updates it - reset before every call */

				if ( 0 > (l_sd = accept(s_pfd_lsnr[i].fd, (struct sockaddr *) &l_sk, &l_slen)) )
					$LOG(STS$K_ERROR, "accept(%d)->%d, errno: %d", s_pfd_lsnr[i].fd, l_sd, errno);
				else	{						/* Create new session context */
					$PUTMSG_FAO(T2R$__NETCONN, l_sd, &l_sk.sin_addr, ntohs(l_sk.sin_port),
						s_pfd_lsnr[i].fd);

					if ( (l_rc = setsockopt(l_sd, IPPROTO_TCP, TCP_NODELAY, (char *) &s_one, sizeof(s_one))) )
						$LOG(STS$K_WARN, "setsockopt()->%d, errno=%d", l_rc, errno);


					if ( !(l_session = calloc(1, sizeof(T2R$_SESSION))) )
						{
						close(l_sd);
						$LOG(STS$K_ERROR, "calloc(%d octets)->NULL, errno: %d",
							(int) sizeof(T2R$_SESSION), errno);
						continue;
						}

					l_session->sd = l_sd;
					l_session->sk = l_sk;
					l_session->target = g_listeners[i].serial;

											/* Start dedicated thread for session */
					if ( (l_rc = pthread_create(&l_tid, NULL, s_net_session, l_session)) )
						{
						$LOG(STS$K_ERROR, "Cannot start network session thread, pthread_create()->%d, errno=%d", l_rc, errno);
						close(l_sd);
						free(l_session);
						}
					}
				}
			}
		}

	return	NULL;
}



/*
 *   DESCRIPTION: Stop the network subsystem: close all the has been opened listening sockets,
 *	then close all the has been opened serial devices. Is called at the exit path of the
 *	main routine.
 *
 *   INPUTS:
 *	NONE
 *
 *   OUTPUTS:
 *	NONE
 *
 *   IMPLICITE OUTPUTS:
 *	s_pfd_lsnr, g_serials
 *
 *   RETURNS:
 *	condition code
 */
int t2r$net_stop_listeners (void)
{
int	l_rc;

	for (int i = 0; i < g_listeners_nr; i++ )
		{
		if ( 0 > s_pfd_lsnr[i].fd )						/* This one has not been started */
			continue;

		close(s_pfd_lsnr[i].fd);
		s_pfd_lsnr[i].fd = -1;

		$LOG(STS$K_WARN, "[#%d] Listener " IPv4_BYTES_FMT ":%d --- is aborted", s_pfd_lsnr[i].fd,
					 IPv4_BYTES(g_listeners[i].sk.sin_addr.s_addr), ntohs(g_listeners[i].sk.sin_port) );
		}

	/*
	 * Serial devices are closed over the table of serials: a few listeners are allowed to share
	 * a single serial device, so a per-listener close would try to close it more than once.
	 */
	for (int i = 0; i < g_serials_nr; i++ )
		{
		if ( g_serials[i].state == T2R$K_STATE_IDLE )
			continue;

		if ( !(1 & (l_rc = t2r$tty_close (&g_serials[i]))) )
			$LOG(STS$K_ERROR, "Error close target <%s>", g_serials[i].devname);
		}

	return	STS$K_SUCCESS;
}


/*
 *   DESCRIPTION: Start the network subsystem: for every record of the listeners table open a
 *	TCP socket, bind it to the has been configured address:port, open the target serial
 *	device, publish the socket to the poll() array of the dispatcher and finally start the
 *	dispatcher thread. A listener with a dead target device is skipped with a diagnostic.
 *
 *   INPUTS:
 *	NONE
 *
 *   OUTPUTS:
 *	NONE
 *
 *   IMPLICITE INPUTS:
 *	g_listeners, g_listeners_nr
 *
 *   IMPLICITE OUTPUTS:
 *	s_pfd_lsnr
 *
 *   RETURNS:
 *	condition code; STS$K_ERROR - no one listener has been started
 */
int t2r$net_start_listeners (void)
{
int	l_rc = STS$K_ERROR, l_sd, l_one = 1, l_count;
T2R$_LISTENER	*l_listener;
const socklen_t l_slen = sizeof(struct sockaddr_in);
pthread_t	l_tid;


	for (int i = 0; i < T2R$K_MAX_LISTENERS; i++)				/* poll() ignores a negative descriptor */
		s_pfd_lsnr[i].fd = -1;

	l_count = 0;

	for (int i = 0; i < g_listeners_nr; i++)
		{
		l_listener = &g_listeners[i];


		if ( 0 > (l_sd = socket(AF_INET, ((l_listener->proto == IPPROTO_UDP) ? SOCK_DGRAM : SOCK_STREAM), 0)) )
				return	$LOG(STS$K_ERROR, "socket()->%d, errno=%d", l_sd, errno);

		if( 0 > setsockopt(l_sd, SOL_SOCKET, SO_REUSEADDR, (char *)&l_one, sizeof(l_one))  )
			$LOG(STS$K_WARN, "setsockopt(%d, SO_REUSEADDR), errno=%d", l_sd, errno);

		if( 0 > setsockopt(l_sd, SOL_SOCKET, SO_REUSEPORT, (char *)&l_one, sizeof(l_one))  )
			$LOG(STS$K_WARN, "setsockopt(%d, SO_REUSEPORT), errno=%d", l_sd, errno);

		if ( 0 > bind(l_sd, (struct sockaddr*) &l_listener->sk, l_slen) )
			{
			close(l_sd);
			$PUTMSG_FAO(T2R$__LSNRERR, &l_listener->sk.sin_addr, ntohs(l_listener->sk.sin_port),
				"bind()", errno);

			continue;
			}

		if ( 0 > (l_rc = listen(l_sd, l_listener->connlm)) )
			{
			close(l_sd);
			$PUTMSG_FAO(T2R$__LSNRERR, &l_listener->sk.sin_addr, ntohs(l_listener->sk.sin_port),
				"listen()", errno);

			continue;
			}

		/*
		 * A target device is opened BEFORE the socket is published to the poll() array:
		 * we do not want to accept clients for a leg which does not work.
		 */
		if ( !(1 & (l_rc = t2r$tty_open (l_listener->serial))) )
			{
			close(l_sd);							/* Don't leak the descriptor */
			$LOG(STS$K_ERROR, "Error open target <%s>", l_listener->serial->devname);

			continue;
			}


		l_listener->fd = l_sd;
		s_pfd_lsnr[i].fd = l_sd;
		s_pfd_lsnr[i].events = POLLIN;


		l_count++;

		$PUTMSG_FAO(T2R$__LSNRRDY, s_pfd_lsnr[i].fd, &l_listener->sk.sin_addr,
			ntohs(l_listener->sk.sin_port), l_listener->serial->devname);
		}


	if ( !l_count )
		return	$LOG(STS$K_ERROR, "No listeners has been started!");


	if ( (l_rc = pthread_create(&l_tid, NULL, s_net_listener, NULL)) )
		return	$LOG(STS$K_ERROR, "Cannot start network listener thread, pthread_create()->%d, errno=%d", l_rc, errno);



	return	STS$K_SUCCESS;
}
