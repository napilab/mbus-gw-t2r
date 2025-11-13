#define	__MODULE__	"T2R-NET"

/*
**++
**
**  FACILITY: A yet another TCP to RTU gateway for MODBUS
**
**  ENVIRONMENT: Linux
**
**  DESCRIPTION: A set of routines related to network I/O
**
**  AUTHORS: Ruslan R. (The BadAss Sysman) Laishev
**
**  CREATION DATE:  26-SEP-2025
**
**  MODIFICATION HISTORY:
**
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
static struct pollfd	s_pfd_lsnr[T2R$K_MAX_LISTENERS] = {-1};

static const	int	s_one = 1;


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

	a_session->datalen = l_netbuf_dsc->len ?  a_session->datalen : MODBUS$SZ_MINMBAP;

	if ( !(1 & (l_rc = s_net_recvn(a_session))) )
		return	l_rc;



	l_mbaph = (MODBUS_ADU_T *) l_netbuf_dsc->data;
	l_len = ntohs(l_mbaph->len);							/* Get length of the MBAP data part */
	l_len -= 1;									/* Unit field is already there */

	if ( l_len > MODBUS$SZ_MAXPDU )
		return	$LOG(STS$K_ERROR, "[#%d] --- MODBUS PDU is too long (%d > %d octets)",
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
T2R$_SESSION	*l_session = a_arg;
struct timespec	l_now;
T2R$_SERIAL	*l_serial;
uint8_t		l_excode;

$DESCRIPTOR_S	(l_netbuf_dsc, (2*MODBUS$SZ_MAXPDU));
$DESCRIPTOR_S	(l_rtu_req_dsc, (2*MODBUS$SZ_MAXPDU));
$DESCRIPTOR_S	(l_rtu_resp_dsc, (2*MODBUS$SZ_MAXPDU));

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

	$LOG(STS$K_INFO, "[#%d] Start session for " IPv4_BYTES_FMT ":%d ....", l_session->sd,
		 IPv4_BYTES(l_session->sk.sin_addr.s_addr), ntohs(l_session->sk.sin_port));



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
						//continue;
						}
					else	break;
					}
				else	break;


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
			$LOG(STS$K_ERROR, "[#%d] poll()->%d, errno: %d", l_session->sd, l_rc, errno);
		else if ( l_pfd.revents & (POLLERR | POLLHUP | POLLNVAL) )
			{
			$LOG(STS$K_ERROR, "[#%d] poll()->%d, .revents: %#x, errno: %d", l_session->sd, l_rc, l_pfd.revents, errno);
			break;
			}
		else if (l_rc)
			clock_gettime(CLOCK_MONOTONIC_COARSE, &l_session->lastio_ts);
		}



	$LOG(STS$K_INFO, "[#%d] Close session for " IPv4_BYTES_FMT ":%d", l_session->sd,
		 IPv4_BYTES(l_session->sk.sin_addr.s_addr), ntohs(l_session->sk.sin_port));


	close(l_session->sd);
	free( (void *) l_session);

}





void *	s_net_listener( void *a_arg)
{
int	l_rc, l_sd;
struct sockaddr_in	l_sk = {0};
socklen_t l_slen = sizeof(struct sockaddr);
T2R$_SESSION	*l_session;
pthread_t	l_tid;

	while ( !g_exit_flag )
		{
		/*
		 * Wait for any new TCP-connection request on all ports ...
		 */
		if ( 0 > (l_rc = poll(s_pfd_lsnr, g_listeners_nr, 5000)) )
			$LOG(STS$K_ERROR, "poll()->%d, errno: %d", l_rc, errno);


		/*
		 * Run over all listeners and check for POLLIN (TCP-connection request)
		 */
		for (int i = 0; i < g_listeners_nr; i++ )
			{
			if ( s_pfd_lsnr[i].revents & POLLIN )				/* Is there any connection request ? */
				{							/* Accept TCP connection */
				if ( 0 > (l_sd = accept(s_pfd_lsnr[i].fd, (struct sockaddr *)&l_sk, &l_slen)) < 0)
					$LOG(STS$K_ERROR, "accept(%d)->%d, errno: %d", s_pfd_lsnr[i].fd, l_rc, errno);
				else	{						/* Create new session context */
					$IFTRACE(g_trace, "[#%d] Accept connection from " IPv4_BYTES_FMT ":%d on SD: #%d", s_pfd_lsnr[i].fd,
						 IPv4_BYTES(l_sk.sin_addr.s_addr), ntohs(l_sk.sin_port), l_sd);

					if ( l_rc = setsockopt(l_sd, IPPROTO_TCP, TCP_NODELAY, (char *) &s_one, sizeof(s_one)) )
						$LOG(STS$K_WARN, "setsockopt()->%d, errno=%d", l_rc, errno);


					l_session = calloc(1, sizeof(T2R$_SESSION));

					l_session->sd = l_sd;
					l_session->sk = l_sk;
					l_session->target = g_listeners[i].serial;

											/* Start dedicated thread for session */
					if ( l_rc = pthread_create(&l_tid, NULL, s_net_session, l_session) )
						{
						$LOG(STS$K_ERROR, "Cannot start network session thread, pthread_create()->%d, errno=%d", l_rc, errno);
						free(l_session);
						}
					}
				}
			}
		}

}



int t2r$net_stop_listeners (void)
{
int	l_rc;

	for (int i = 0; i < g_listeners_nr; i++ )
		{
		close(s_pfd_lsnr[i].fd);
		$LOG(STS$K_WARN, "[#%d] Listener " IPv4_BYTES_FMT ":%d --- is aborted", s_pfd_lsnr[i].fd,
					 IPv4_BYTES(g_listeners[i].sk.sin_addr.s_addr), ntohs(g_listeners[i].sk.sin_port) );


		if ( !(1 & (l_rc = t2r$tty_close (g_listeners[i].serial))) )
			$LOG(STS$K_ERROR, "Error close  target <%s>", g_listeners[i].serial->devname);
		}

	return	STS$K_SUCCESS;
}


int t2r$net_start_listeners (void)
{
int	l_rc, l_sd, l_one = 1, l_count;
T2R$_LISTENER	*l_listener;
const socklen_t l_slen = sizeof(struct sockaddr);
pthread_t	l_tid;


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
			$LOG(STS$K_ERROR, "bind(%d, " IPv4_BYTES_FMT  ":%d), errno=%d", l_sd,
			     IPv4_BYTES(l_listener->sk.sin_addr.s_addr), ntohs(l_listener->sk.sin_port), errno);

			continue;
			}

		if ( 0 > (l_rc = listen(l_sd, l_listener->connlm)) )
			{
			close(l_sd);
			$LOG(STS$K_ERROR, "listen(%d)->%d, errno: %d",l_sd, l_rc, errno);

			continue;
			}


		l_listener->fd = l_sd;
		s_pfd_lsnr[i].fd = l_sd;
		s_pfd_lsnr[i].events = POLLIN;


		if ( !(1 & (l_rc = t2r$tty_open (l_listener->serial))) )
			{
			$LOG(STS$K_ERROR, "Error open target <%s>", l_listener->serial->devname);

			continue;
			}


		l_count++;

		$LOG(STS$K_SUCCESS, "[#%d] Listener [" IPv4_BYTES_FMT ":%d, Target: <#%d:%s>] --- initialized", s_pfd_lsnr[i].fd,
					 IPv4_BYTES(g_listeners[i].sk.sin_addr.s_addr), ntohs(g_listeners[i].sk.sin_port),
					l_listener->serial->fd, l_listener->serial->devname);
		}


	if ( !l_count )
		return	$LOG(STS$K_ERROR, "No listeners has been started!");


	if ( l_rc = pthread_create(&l_tid, NULL, s_net_listener, NULL) )
		return	$LOG(STS$K_ERROR, "Cannot start network listener thread, pthread_create()->%d, errno=%d", l_rc, errno);



	return	STS$K_SUCCESS;
}
