#define	__MODULE__	"T2R-TTY"
#define	__IDENT__	"X.00-06"
#define	__REV__		"00.06.00"

/*
**++
**
**  FACILITY: A yet another TCP to RTU gateway for MODBUS
**
**  ENVIRONMENT: Linux
**
**
**  DESCRIPTION: This module covers TTY related functionality. Serial line open and initialization, read\write and timeout processing.
**	Send MODBUS PDU to serial port and read with CRC checking.
**
**  AUTHORS: StarLet Squad and Ruslan R. Laishev (AKA: BadAss sysman)
**
**  CREATION DATE:  26-SEP-2025
**
**  MODIFICATION HISTORY:
**
**	25-AUG-2026	RRL	X.00-04 / REV: 00.04.00 - Audit fixes:
**				s_tty_rtu_rx(): rewritten - honest t3.5 end-of-frame criteria restarted on
**				every read chunk, an overall deadline, EAGAIN/EINTR are not fatal anymore,
**				CLOCK_MONOTONIC instead of the _COARSE one, an upper frame length check,
**				the CRC trailer is assembled octet-by-octet;
**				s_tty_rtu_tx(): tcflush() of a stale input before TX, tcdrain() after TX,
**				the CRC trailer is sent octet-by-octet, fixed diagnostic arguments;
**				s_tty_open(): TIOCSRS485 (was TIOCGRS485 - the RS485 mode was never set);
**				s_add_ts_to_pdu(): fixed 64->4x16 bits packing shifts (48/32/16/0, were
**				overlapping 24/16/8/0), the lambda is replaced by s_put_be16() routine,
**				the timezone offset is reported in signed minutes now;
**				t2r$tty_open(): the READY state is set on successful open only;
**				t2r$tty_exec_req(): fixed swapped $LOG arguments (SIGSEGV);
**				fixed an undeclared <tspeed> in s_tty_speed2bits();
**				removed dead code: s_tty_delay(), th_func_t, s_pfd_lsnr[], <linux/i2c.h>.
**
**	25-AUG-2026	RRL	X.00-04ECO02 / REV: 00.04.02 - Commenting only, no functional change:
**				every routine has been supplied by the standard DESCRIPTION/INPUTS/
**				OUTPUTS/RETURNS header block with the detailed parameters description.
**
**	25-AUG-2026	RRL	X.00-06 / REV: 00.06.00 - The critical (for the operational troubleshooting)
**				diagnostics are signalled via $PUTMSG_FAO by the coded catalogue messages
**				now: DEVOPNERR, NOANSWER, FRAMETMO, BADFRAME, CRC16ERR, EXCRPT.
**
**--
*/
#include	<time.h>
#include	<stdio.h>
#include	<stdlib.h>
#include	<errno.h>
#include	<unistd.h>
#include	<poll.h>
#include	<termios.h>
#include	<fcntl.h>
#include	<termios.h>
#include	<sys/uio.h>
#include	<linux/serial.h>
#include	<sys/ioctl.h>


#define		__FAC__	"T2R"
#define		__TFAC__ __FAC__ ": "					/* Special prefix for $TRACE			*/

#include	"utility_routines.h"
#include	"t2r_defs.h"
#include	"t2r_msgs.h"
#include	"t2r_modbus.h"


extern int	g_exit_flag, g_trace;



/*
 *   DESCRIPTION: Get an exclusive access to the serial device: a MODBUS RTU line is strictly a
 *	half duplex request-answer bus, so only one network session at a time is allowed to talk
 *	to it. A timed lock is used (13 seconds, 3 attempts) to detect a stuck holder instead of
 *	hanging forever.
 *
 *   INPUTS:
 *	a_serial:	A context of the serial line, .lock - the guarding mutex
 *
 *   OUTPUTS:
 *	NONE
 *
 *   RETURNS:
 *	condition code; STS$K_ERROR - the lock has not been taken after all the attempts
 */
static inline int s_tty_lock (
		T2R$_SERIAL	*a_serial
		)
{
struct timespec l_now, l_tmo = {.tv_sec = 13}, l_etime;
int	l_rc;

	for (int i = 0; i < 3; i++)
		{
		if ( (l_rc = clock_gettime(CLOCK_REALTIME_COARSE, &l_now)) )
			return	$LOG(STS$K_ERROR, "clock_gettime->%d, errno: %d", l_rc, errno);

		__util$add_time (&l_now, &l_tmo, &l_etime);

		if ( !(l_rc = pthread_mutex_timedlock(&a_serial->lock, &l_etime)) )
			{
			//$IFTRACE(g_trace, "Got exclusive access to <%s> ...", a_serial->devname);
			return	STS$K_SUCCESS;
			}

		$LOG(STS$K_WARN, "pthread_mutex_timedlock(<%s>)->%d, errno=%d", a_serial->devname, l_rc, errno);
		}


	return	$LOG(STS$K_ERROR, "Cannot get exclusive access for <%s> after %d attempts", a_serial->devname, 3);
}


/*
 *   DESCRIPTION: Release the exclusive access to the serial device which has been taken by
 *	s_tty_lock().
 *
 *   INPUTS:
 *	a_serial:	A context of the serial line, .lock - the guarding mutex
 *
 *   OUTPUTS:
 *	NONE
 *
 *   RETURNS:
 *	condition code
 */
static inline int s_tty_unlock (
		T2R$_SERIAL	*a_serial
		)
{
int	l_rc;

	if ( (l_rc = pthread_mutex_unlock(&a_serial->lock)) )
		return	$LOG(STS$K_ERROR, "pthread_mutex_unlock(<%s>)->%d, errno: %d", a_serial->devname, l_rc, errno);

	return	STS$K_SUCCESS;
}



/*
 *   DESCRIPTION: Check and validate data in the buffer to be sent;
 *	- compute CRC16
 *	- send PDU + CRC16 over serial line
 *	- do delay in 3.5 characters
 *
 *   INPUTS:
 *	a_serial:	A context of the serial line
 *	a_req_pdu:	A buffer with the PDU RTU to be sent
 *
 *   OUTPUTS:
 *	NONE
 *
 *   RETURNS:
 *	condition status
 */
static int	s_tty_rtu_tx (
	const T2R$_SERIAL	*a_serial,
		T2R$_DESC	*a_req_pdu
		)
{
int	l_rc, l_len;
uint16_t l_crc;
uint8_t	l_crcbuf[2];
enum {	IOV$K_PDU, IOV$K_CRC, IOV$K_MAX};
struct iovec	l_iov[IOV$K_MAX];
struct timespec l_inter_pdu_ts;


	if ( !a_req_pdu->len )
		return	$LOG(STS$K_WARN, "[#%d:<%s>] No data to send", a_serial->fd, a_serial->devname),
				STS$K_SUCCESS;

	l_len = 0;									/* Total lenght of data to be sent */

	l_iov[IOV$K_PDU].iov_base = a_req_pdu->data;
	l_len += l_iov[IOV$K_PDU].iov_len = a_req_pdu->len;

	l_crc = s_modbus_crc_calculate (l_iov[IOV$K_PDU].iov_base, l_iov[IOV$K_PDU].iov_len);

	l_crcbuf[0] = (uint8_t) (l_crc & 0xFF);					/* Lo octet goes first on the wire */
	l_crcbuf[1] = (uint8_t) (l_crc >> 8);

	l_iov[IOV$K_CRC].iov_base = l_crcbuf;
	l_len += l_iov[IOV$K_CRC].iov_len = sizeof(l_crcbuf);

	/*
	 * Drop a garbage which could be left in the input FIFO by a previous timed out exchange:
	 * otherwise it will be glued to a head of the coming answer and will break the CRC of a
	 * perfectly correct frame.
	 */
	if ( 0 > (l_rc = tcflush(a_serial->fd, TCIFLUSH)) )
		$LOG(STS$K_WARN, "[#%d:<%s>] tcflush()->%d, errno: %d", a_serial->fd, a_serial->devname, l_rc, errno);


											/* Predelay ... */
	for ( l_inter_pdu_ts = a_serial->inter_pdu_ts; nanosleep(&l_inter_pdu_ts,  &l_inter_pdu_ts); );


	if ( 0 > (l_rc = writev(a_serial->fd, l_iov,  IOV$K_MAX)) )
		l_rc = $LOG(STS$K_ERROR, "[#%d:<%s>] Xmit of %d octets failed, writev()->%d, errno: %d",
				a_serial->fd, a_serial->devname, l_len, l_rc, errno);
	else if ( l_rc != l_len )
		l_rc = $LOG(STS$K_ERROR, "[#%d:<%s>] Sent %d octets (from %d), errno: %d",
				a_serial->fd, a_serial->devname, l_rc, l_len, errno);
	else	{
		/*
		 * Wait until the UART has really shifted out the last octet. On a half duplex RS-485
		 * line without a hardware direction control we would otherwise switch to the receiving
		 * before the frame has been transmitted and would read our own request back.
		 */
		if ( 0 > (l_rc = tcdrain(a_serial->fd)) )
			l_rc = $LOG(STS$K_ERROR, "[#%d:<%s>] tcdrain()->%d, errno: %d",
					a_serial->fd, a_serial->devname, l_rc, errno);
		else	l_rc = STS$K_SUCCESS;
		}


											/* Postdelay ... */
	for ( l_inter_pdu_ts = a_serial->inter_pdu_ts; nanosleep(&l_inter_pdu_ts,  &l_inter_pdu_ts); );

	$RTU_PRINT("Sent", a_req_pdu->data, a_req_pdu->len, l_crc);

	return	l_rc;
}


/*
 *   DESCRIPTION: Read a whole RTU frame from the serial line, check a framing and the CRC16.
 *
 *	1. Wait for a first octet of the answer by using a long timeout: a target device is allowed
 *	   to take a time to process the request.
 *	2. Accumulate octets until the line keeps silence for the t3.5 interval - this is the
 *	   end-of-frame criteria of the MODBUS over Serial Line specification. The interval is
 *	   restarted on every successfully read chunk of the data.
 *	3. A whole frame must be collected within the answer timeout, otherwise the frame is
 *	   considered as a broken one.
 *
 *   INPUTS:
 *	a_serial:	A context of the serial line
 *	a_req_pdu:	A descriptor of the request PDU - is used to match the slave and the function code
 *
 *   OUTPUTS:
 *	a_resp_pdu:	A descriptor to accept the response PDU, the CRC16 trailer is stripped off
 *
 *   RETURNS:
 *	condition code
 */
static int	s_tty_rtu_rx (
	const T2R$_SERIAL	*a_serial,
		const T2R$_DESC	*a_req_pdu,
		T2R$_DESC	*a_resp_pdu
		)
{
int	l_rc, l_tmo_msec, l_space;
uint16_t	l_crc, l_crc_rcvd;
struct pollfd	l_pfd = {.fd = a_serial->fd, .events = POLLIN};
struct timespec	l_now_ts, l_silence_ts, l_deadline_ts;
MODBUS_PDU_T	*l_req_pdu, *l_resp_pdu;
uint8_t	*l_data;

	a_resp_pdu->len = 0;

	/*
	 * Wait for a first octet of the answer
	 */
	if ( 0 > (l_rc = poll(&l_pfd, 1, a_serial->anstmo_msec)) )
		{
		if ( errno != EINTR )
			return	$LOG(STS$K_ERROR, "[#%d:<%s>] poll()->%d, errno: %d", a_serial->fd, a_serial->devname,
					l_rc, errno);
		}
	else if ( !l_rc )
		return	$PUTMSG_FAO(T2R$__NOANSWER, a_serial->fd, a_serial->devname, a_serial->anstmo_msec);

	/*
	 * An upper limit for the whole frame: we never stay here longer than the answer timeout.
	 */
	clock_gettime(CLOCK_MONOTONIC, &l_now_ts);

	l_deadline_ts.tv_sec = a_serial->anstmo_msec / 1000;
	l_deadline_ts.tv_nsec = (a_serial->anstmo_msec % 1000) * 1000000L;
	__util$add_time (&l_now_ts, &l_deadline_ts, &l_deadline_ts);

	/*
	 * The t3.5 silent interval - it is restarted on every successfully read chunk of the data.
	 */
	__util$add_time (&l_now_ts, &a_serial->inter_pdu_ts, &l_silence_ts);

	l_tmo_msec = 1 + (a_serial->inter_pdu_usec / 1000);			/* At least 1 msec - poll() granularity */

	while ( !g_exit_flag )
		{
		clock_gettime(CLOCK_MONOTONIC, &l_now_ts);

		if ( a_resp_pdu->len && (0 < __util$cmp_time (&l_now_ts, &l_silence_ts)) )
			break;							/* t3.5 of silence - the frame is complete */

		if ( 0 < __util$cmp_time (&l_now_ts, &l_deadline_ts) )
			{
			$PUTMSG_FAO(T2R$__FRAMETMO, a_serial->fd, a_serial->devname, a_serial->anstmo_msec,
				a_resp_pdu->len);
			break;
			}

		if ( 0 >= (l_space = a_resp_pdu->sz - a_resp_pdu->len) )
			{
			$LOG(STS$K_WARN, "[#%d:<%s>] Input buffer is full (%d octets) --- stop reading",
				a_serial->fd, a_serial->devname, a_resp_pdu->len);
			break;
			}

		if ( 0 > (l_rc = poll(&l_pfd, 1, l_tmo_msec)) )
			{
			if ( errno == EINTR )
				continue;

			return	$LOG(STS$K_ERROR, "[#%d:<%s>] poll()->%d, errno: %d", a_serial->fd, a_serial->devname,
					l_rc, errno);
			}

		if ( !l_rc )
			continue;						/* Nothing yet - let the t3.5 check to decide */

		if ( 0 > (l_rc = read(a_serial->fd, a_resp_pdu->data + a_resp_pdu->len, l_space)) )
			{
									/* The port is opened with O_NDELAY: these three
									   errno-s are a normal flow, not a failure	*/
			if ( (errno == EAGAIN) || (errno == EWOULDBLOCK) || (errno == EINTR) )
				continue;

			return	$LOG(STS$K_ERROR, "[#%d:<%s>] read()->%d, errno: %d", a_serial->fd, a_serial->devname,
					l_rc, errno);
			}

		if ( !l_rc )
			continue;

		a_resp_pdu->len += l_rc;					/* Adjust data counter in the input buffer */

		clock_gettime(CLOCK_MONOTONIC, &l_now_ts);			/* Restart the t3.5 silent interval */
		__util$add_time (&l_now_ts, &a_serial->inter_pdu_ts, &l_silence_ts);
		}

	/*
	 * Validate a framing of the has been received data
	 */
	if ( !a_resp_pdu->len )
		return	$LOG(STS$K_ERROR, "[#%d:<%s>] No data has been received", a_serial->fd, a_serial->devname);

	if ( (a_resp_pdu->len < MODBUS$SZ_MINRTU) || (a_resp_pdu->len > MODBUS$SZ_MAXPDU) )
		return	$PUTMSG_FAO(T2R$__BADFRAME, a_serial->fd, a_serial->devname, a_resp_pdu->len,
				MODBUS$SZ_MINRTU, MODBUS$SZ_MAXPDU);

	$RTU_PRINT("Rcvd", a_resp_pdu->data, a_resp_pdu->len, 0);

	/*
	 * Check the CRC16 trailer: the low octet goes first on the wire, so the value is assembled
	 * octet-by-octet - it is neither alignment nor byte order dependent.
	 */
	l_data = (uint8_t *) a_resp_pdu->data;

	l_crc_rcvd = (uint16_t) l_data[a_resp_pdu->len - 2]
		| ((uint16_t) l_data[a_resp_pdu->len - 1] << 8);

	l_crc = s_modbus_crc_calculate (l_data, a_resp_pdu->len - 2);

	if ( l_crc != l_crc_rcvd )
		return	$PUTMSG_FAO(T2R$__CRC16ERR, a_serial->fd, a_serial->devname, l_crc_rcvd, l_crc);

	/*
	 * Check that the answer belongs to the has been sent request
	 */
	l_req_pdu = (MODBUS_PDU_T *) a_req_pdu->data;
	l_resp_pdu = (MODBUS_PDU_T *) a_resp_pdu->data;

	if ( (l_req_pdu->slave != l_resp_pdu->slave)
		|| (l_req_pdu->fncode != (l_resp_pdu->fncode & (~MODBUS$M_FN_EXCEPTION))) )
		return	$LOG(STS$K_WARN, "[#%d:<%s>] Slave: (REQ: %d vs RESP: %d) or Function (REQ: %d vs RESP: %d) --- mismatch",
				a_serial->fd, a_serial->devname, l_req_pdu->slave, l_resp_pdu->slave,
				l_req_pdu->fncode, l_resp_pdu->fncode);

	a_resp_pdu->len -= 2;							/* Reduce a length of data for CRC16 */

	return	STS$K_SUCCESS;
}


/*
 *   DESCRIPTION: Translate line rate to internal representative. In error case return <B9600>
 *
 *   INPUTS:
 *	a_speed:	A data speed in bauds
 *
 *   OUTPUTS:
 *	NONE
 *
 *   RETURNS:
 *	<line_speed>
 */
static	speed_t s_tty_speed2bits(int a_speed)
{
speed_t l_speed;

	switch (a_speed)
		{
		#if defined(B50)
		case 50:	l_speed = B50;		break;
		#endif

		#if defined(B75)
		case 75:	l_speed = B75;		break;
		#endif

		#if defined(B110)
		case 110:	l_speed = B110;		break;
		#endif

		#if defined(B134)
		case 134:	l_speed = B134;		break;
		#endif

		#if defined(B150)
		case 150:	l_speed = B150;		break;
		#endif

		#if defined(B200)
		case 200:	l_speed = B200;		break;
		#endif

		#if defined(B300)
		case 300:	l_speed = B300;		break;
		#endif

		#if defined(B600)
		case 600:	l_speed = B600;		break;
		#endif

		#if defined(B1200)
		case 1200:	l_speed = B1200;	break;
		#endif

		#if defined(B1800)
		case 1800:	l_speed = B1800;	break;
		#endif

		#if defined(B2400)
		case 2400:	l_speed = B2400;	break;
		#endif

		#if defined(B4800)
		case 4800:	l_speed = B4800;	break;
		#endif

		#if defined(B7200)
		case 7200:	l_speed = B7200;		break;
		#endif

		#if defined(B9600)
		case 9600:	l_speed = B9600;	break;
		#endif

		#if defined(B12000)
		case 12000:	l_speed = B12000;	break;
		#endif

		#if defined(B14400)
		case 14400:	l_speed = B14400;	break;
		#endif

		#if defined(B19200)
		case 19200:	l_speed = B19200;	break;
		#elif defined(EXTA)
		case 19200:	l_speed = EXTA;		break;
		#endif

		#if defined(B38400)
		case 38400:	l_speed = B38400;	break;
		#elif defined(EXTB)
		case 38400:	l_speed = EXTB;		break;
		#endif

		#if defined(B57600)
		case 57600:	l_speed = B57600;	break;
		#endif

		#if defined(B115200)
		case 115200:	l_speed = B115200;	break;
		#endif

		#if defined(B230400)
		case 230400:	l_speed = B230400;	break;
		#endif

		#if defined(B460800)
		case 460800:	l_speed = B460800;	break;
		#endif

		#if defined(B500000)
		case 500000:	l_speed = B500000;	break;
		#endif

		#if defined(B576000)
		case 576000:	l_speed = B576000;	break;
		#endif

		#if defined(B921600)
		case 921600:	l_speed = B921600;	break;
		#endif

		#if defined(B1000000)
		case 1000000:	l_speed = B1000000;	break;
		#endif

		#if defined(B1152000)
		case 1152000:	l_speed = B1152000;	break;
		#endif

		#if defined(B1500000)
		case 1500000:	l_speed = B1500000;	break;
		#endif

		#if defined(B2000000)
		case 2000000:	l_speed = B2000000;	break;
		#endif

		#if defined(B2500000)
		case 2500000:	l_speed = B2500000;	break;
		#endif

		#if defined(B3000000)
		case 3000000:	l_speed = B3000000;	break;
		#endif

		#if defined(B3500000)
		case 3500000:	l_speed = B3500000;	break;
		#endif

		#if defined(B4000000)
		case 4000000:	l_speed = B4000000;	break;
		#endif

		default:
			$LOG(STS$K_WARN, "Cannot translate speed %d baud to internal representative, set 9600", a_speed);
			l_speed = B9600;
		}

	return l_speed;
}

/*
 *   DESCRIPTION: Close the serial device: restore the terminal attributes which has been saved
 *	at the open time, close the descriptor. A not opened device is silently ignored.
 *
 *   INPUTS:
 *	a_serial:	A context of the serial line; .fd - descriptor, .savedtios - the terminal
 *			attributes as they were before s_tty_open()
 *
 *   OUTPUTS:
 *	a_serial:	.fd is set to -1
 *
 *   RETURNS:
 *	condition code
 */
static int s_tty_close (
	T2R$_SERIAL	*a_serial
		)
{
int	l_rc;

	if (a_serial->fd < 0)
		return	STS$K_SUCCESS;

	if ( !(l_rc = isatty(a_serial->fd)) )
		return	$LOG(STS$K_WARN, "isatty(%d)->%d --- invalid I/O descriptor for <%s>", a_serial->fd, l_rc, a_serial->devname);

	if ( 0 > (l_rc = tcsetattr(a_serial->fd, TCSAFLUSH, &a_serial->savedtios)))
		$LOG(STS$K_ERROR, "Error tcsetattr(%s)->%d, errno: %d", a_serial->devname, l_rc, errno);

	close(a_serial->fd);
	a_serial->fd = -1;


	return	STS$K_SUCCESS;
}





/*
 *   DESCRIPTION: Open and set up a serial port for the RTU communications: raw 8-bit I/O, the
 *	has been configured rate/data bits/parity/stop bits, an optional RS-485 mode.
 *	Stolen and adopted from LIBMODBUS\MODBUS-RTU.C.
 *
 *   INPUTS:
 *	a_serial:	A context of the serial line; .devname and the line parameters (.baud,
 *			.databits, .parity, .stopbits, .flags) must be filled by the caller
 *
 *   OUTPUTS:
 *	a_serial:	.fd - a descriptor of the has been opened device (-1 on a failure),
 *			.savedtios - the original terminal attributes to be restored at close
 *
 *   RETURNS:
 *	condition code
 */
static int s_tty_open (
			       T2R$_SERIAL	*a_serial
			       )
{
speed_t l_speed = -1;
int	l_flags = 0, l_rc;

	l_flags = O_RDWR | O_NOCTTY | O_NDELAY | O_EXCL;

#ifdef O_CLOEXEC
	l_flags |= O_CLOEXEC;
#endif

	if ( 0 > (a_serial->fd = open(a_serial->devname, l_flags)) )
		return	$PUTMSG_FAO(T2R$__DEVOPNERR, a_serial->devname, errno);

	memset(&a_serial->savedtios, 0, sizeof(struct termios));

	if ( (l_rc = tcgetattr(a_serial->fd , &a_serial->savedtios)) )
		return	close(a_serial->fd), a_serial->fd = -1,
			$LOG(STS$K_ERROR, "Error tcgetattr(%s)->%d, errno: %d", a_serial->devname, l_rc, errno);

	a_serial->tios = a_serial->savedtios;						/* Make local copy */



	/*
	 * Set the baud rate
	 */
	l_speed = s_tty_speed2bits (a_serial->baud);					/* Translate bauds to internal representative */

	if ( 0 > ( l_rc = cfsetispeed(&a_serial->tios, l_speed)) )			/* Set line rate */
		return	close(a_serial->fd ), a_serial->fd = -1,
			$LOG(STS$K_ERROR, "Error cfsetispeed(%s, %d(%#x))->%d, errno: %d",
				a_serial->devname, a_serial->baud, l_speed, l_rc, errno);

	if ( 0 > ( l_rc = cfsetospeed(&a_serial->tios, l_speed)) )			/* Set line rate */
		return	close(a_serial->fd ), a_serial->fd = -1,
			$LOG(STS$K_ERROR, "Error cfsetospeed(%s, %d(%#x))->%d, errno: %d",
				a_serial->devname, a_serial->baud, l_speed, l_rc, errno);


	/* C_CFLAG      Control options
	   CLOCAL       Local line - do not change "owner" of port
	   CREAD        Enable receiver
	*/
	a_serial->tios.c_cflag |= (CREAD | CLOCAL);

	/* CSIZE, HUPCL, CRTSCTS (hardware flow control) */

	/* Set data bits (5, 6, 7, 8 bits)
	CSIZE        Bit mask for data bits
	*/
	a_serial->tios.c_cflag &= ~CSIZE;

	switch (a_serial->databits)							/* Translate databits to internal representative */
		{
		case 5:	a_serial->tios.c_cflag |= CS5;	break;
		case 6:	a_serial->tios.c_cflag |= CS6;	break;
		case 7:	a_serial->tios.c_cflag |= CS7;	break;

		case 8:
		default:
		a_serial->tios.c_cflag |= CS8;	break;
		}

	/* Stop bit (1 or 2) */
	if (a_serial->stopbits == 1)
		a_serial->tios.c_cflag &=~ CSTOPB;	/* 1 */
	else	a_serial->tios.c_cflag |= CSTOPB;	/* 2 */


	/* PARENB       Enable parity bit
	   PARODD       Use odd parity instead of even */
	if (a_serial->parity == 'N')
		{ /* None */ a_serial->tios.c_cflag &=~ PARENB;   }
	else if (a_serial->parity == 'E')
		{/* Even */
		a_serial->tios.c_cflag |= PARENB;
		a_serial->tios.c_cflag &=~ PARODD;
		}
	else	{ /* Odd */
		a_serial->tios.c_cflag |= PARENB;
		a_serial->tios.c_cflag |= PARODD;
		}

	if (a_serial->parity == 'N')
		a_serial->tios.c_iflag &= ~INPCK;
	else	a_serial->tios.c_iflag |= INPCK;


	/* Raw input */
	a_serial->tios.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);

	/* Software flow control is disabled */
	a_serial->tios.c_iflag &= ~(IXON | IXOFF | IXANY);

	/* Raw output */
	a_serial->tios.c_oflag &=~ OPOST;

	/* Unused because we use open with the NDELAY option */
	a_serial->tios.c_cc[VMIN] = 0;
	a_serial->tios.c_cc[VTIME] = 0;

	if ( tcsetattr(a_serial->fd, TCSANOW, &a_serial->tios) < 0 )
		return	close(a_serial->fd), a_serial->fd = -1,
			$LOG(STS$K_ERROR, "tcsetattr <%s>, errno: %d", a_serial->devname, errno);


#ifdef HAVE_TIOCRS485
	if (a_serial->flags & T2R$M_SERIAL_RS485)
		{
		struct serial_rs485 rs485conf = {0};

		$IFTRACE(g_trace, "Trying to enable RS-485 support for %s", a_serial->devname);

		if ( 0 > (l_rc = ioctl(a_serial->fd, TIOCGRS485, &rs485conf)) )
			{
			a_serial->flags &=  ~T2R$M_SERIAL_RS485;
			$LOG(STS$K_WARN, "ioctl(<%s>, TIOCGRS485)->%d, errno: %d --- disabled RS485 support", a_serial->devname, l_rc, errno);
			}
		else	{
			rs485conf.flags |= SER_RS485_ENABLED;

			if ( 0 > (l_rc = ioctl(a_serial->fd, TIOCGRS485, &rs485conf)) )
				{
				a_serial->flags &=  ~T2R$M_SERIAL_RS485;
				$LOG(STS$K_WARN, "ioctl(<%s>, TIOCGRS485)->%d, errno: %d --- disabled RS485 support", a_serial->devname, l_rc, errno);
				}
			else	$LOG(STS$K_SUCCESS, "Enabled RS-485 support for %s", a_serial->devname);
			}

		}
#endif /* HAVE_TIOCRS485 */

	return	STS$K_SUCCESS;
}








/*
 *   DESCRIPTION: Form a MODBUS exception answer PDU for the has been failed request: the slave
 *	address is copied from the request, the function code is marked by the exception flag
 *	(bit 7), the exception code is put into the data part.
 *
 *   INPUTS:
 *	a_req_dsc:	A descriptor of the request PDU
 *	a_excode:	A MODBUS exception code (MODBUS$K_EXCEPTION_* constants)
 *
 *   OUTPUTS:
 *	a_resp_dsc:	A descriptor to accept the has been formed exception PDU (3 octets)
 *
 *   RETURNS:
 *	condition code
 */
static inline int s_make_exception_resp (
		     void	*a_req_dsc,
		     void	*a_resp_dsc,
		uint8_t		a_excode
		)
{
T2R$_DESC	*l_req_dsc = a_req_dsc, *l_resp_dsc = a_resp_dsc;
MODBUS_PDU_T	*l_req_pdu = (MODBUS_PDU_T *) l_req_dsc->data;
MODBUS_EXC_T	*l_resp_pdu = (MODBUS_EXC_T *) l_resp_dsc->data;

	l_resp_pdu->slave = l_req_pdu->slave;
	l_resp_pdu->fncode = l_req_pdu->fncode | MODBUS$M_FN_EXCEPTION;		/* Set error indicator */
	l_resp_pdu->excode = a_excode;						/* Put error code */

	l_resp_dsc->len = 3;

	return	STS$K_SUCCESS;
}








/*
 *   DESCRIPTION: Put a 16 bits word into the buffer in the Network Byte Order (Big Endian),
 *	advance the buffer pointer to the next position.
 *
 *   INPUTS:
 *	a_buf:		An address of the current position in the output buffer
 *	a_word16:	A value to be stored
 *
 *   OUTPUTS:
 *	NONE
 *
 *   RETURNS:
 *	An address of the next position in the output buffer
 */
static inline uint16_t * s_put_be16 (
		uint16_t	*a_buf,
		uint16_t	a_word16
		)
{
	*(a_buf) = htons(a_word16);

	return	(a_buf + 1);
}


/*
 *   DESCRIPTION: Form an answer PDU with the local Time Stamp - a "pseudo device" feature.
 *	Depending on the has been requested quantity of registers the answer is:
 *	4 registers - seconds (64 bits, MSW first), +4 - nanoseconds (64 bits, MSW first),
 *	+1 - the local timezone offset in signed minutes.
 *
 *   INPUTS:
 *	a_req_dsc:	A descriptor of the request PDU
 *
 *   OUTPUTS:
 *	a_resp_dsc:	A descriptor to accept the answer PDU
 *
 *   RETURNS:
 *	condition code
 */
static int s_add_ts_to_pdu (
		void	*a_req_dsc,
		void	*a_resp_dsc
		)
{
T2R$_DESC *l_req_dsc = a_req_dsc, *l_resp_dsc = a_resp_dsc;
MODBUS_REQ_T	*l_req_pdu;
MODBUS_RESP_T	*l_resp_pdu;
int	l_nr_regs;
struct timespec l_now;
struct tm l_tm = {0};
uint16_t	*l_valptr;

	l_req_pdu = (MODBUS_REQ_T *) l_req_dsc->data;
	l_resp_pdu = (MODBUS_RESP_T *) l_resp_dsc->data;

	l_nr_regs = ntohs(l_req_pdu->qty);

	if ( (l_nr_regs < 4) || (l_nr_regs > (4 + 4 + 1) ))
	     return	STS$K_ERROR;


	l_resp_dsc->len = MODBUS$SZ_PDUHDR;						/* Preset PDU length */
	l_resp_pdu->slave = l_req_pdu->slave;						/* Copy Slave and Function code from original request */
	l_resp_pdu->fncode = l_req_pdu->fncode;
	l_resp_pdu->bc = 0;

	l_valptr = l_resp_pdu->val;							/* Set <l_valptr> to PDU's data part */


	clock_gettime(CLOCK_REALTIME_COARSE, &l_now);

	if (  l_nr_regs >= 4 )
		{
		/*
		 * A 64 bits count of seconds is packed into 4x16 bits registers, the most significant
		 * word goes first: R0 = bits 63..48, R1 = 47..32, R2 = 31..16, R3 = 15..0
		 */
		l_valptr = s_put_be16(l_valptr, (uint16_t) (l_now.tv_sec >> 48));	/* R0 */
		l_valptr = s_put_be16(l_valptr, (uint16_t) (l_now.tv_sec >> 32));	/* R1 */
		l_valptr = s_put_be16(l_valptr, (uint16_t) (l_now.tv_sec >> 16));	/* R2 */
		l_valptr = s_put_be16(l_valptr, (uint16_t) (l_now.tv_sec));		/* R3 */

		l_resp_dsc->len += (4 * 2);						/* Adjust size of data to 4xbe16 */
		l_resp_pdu->bc += (4 * 2);
		}

	if (  l_nr_regs >= (4 + 4) )
		{
		/* A 64 bits count of nanoseconds is packed by the same rule: R4 = 63..48 ... R7 = 15..0 */
		l_valptr = s_put_be16(l_valptr, (uint16_t) ((int64_t) l_now.tv_nsec >> 48));	/* R4 */
		l_valptr = s_put_be16(l_valptr, (uint16_t) ((int64_t) l_now.tv_nsec >> 32));	/* R5 */
		l_valptr = s_put_be16(l_valptr, (uint16_t) ((int64_t) l_now.tv_nsec >> 16));	/* R6 */
		l_valptr = s_put_be16(l_valptr, (uint16_t) ((int64_t) l_now.tv_nsec));		/* R7 */

		l_resp_dsc->len += (4 * 2);						/* Adjust size of data to 4xbe16 */
		l_resp_pdu->bc += (4 * 2);
		}

	if (  l_nr_regs == (4 + 4 + 1) )
		{
		localtime_r(&l_now.tv_sec, &l_tm);

		/*
		 * The offset is reported in MINUTES to fit the +-840 range into a signed 16 bits value:
		 * an offset in seconds (up to +-50400) does not fit into int16_t.
		 */
		l_valptr = s_put_be16(l_valptr, (uint16_t) (int16_t) (l_tm.tm_gmtoff / 60));	/* R8 */

		l_resp_dsc->len += (1 * 2);						/* Adjust size of data to 1xbe16 */
		l_resp_pdu->bc += (1 * 2);
		}


	return	STS$K_SUCCESS;
}




/*
 *   DESCRIPTION: Get exclusive lock for serial device, check and do if need initialization.
 *
 *   INPUTS:
 *	a_serial:	A context for serial comminication line
 *
 *   OUTPUTS:
 *	a_serial:
 *
 *   RETURNS:
 *	condition code
 */
int	t2r$tty_open (
			       T2R$_SERIAL	*a_serial
			       )
{
int	l_rc;

	if ( !(1 & s_tty_lock (a_serial)) )
		return	$LOG(STS$K_ERROR, "Device <%s> --- cannot be initialized", a_serial->devname);

	if ( a_serial->state > T2R$K_STATE_IDLE )
		l_rc = $LOG(STS$K_INFO, "Device <%s> --- has been initialized", a_serial->devname);
	else if ( 1 & (l_rc = s_tty_open (a_serial)) )					/* The state is changed ONLY on success */
		{
		a_serial->state = T2R$K_STATE_READY;
		$PUTMSG_FAO(T2R$__DEVREADY, a_serial->devname, a_serial->baud, a_serial->anstmo_msec,
			a_serial->inter_pdu_usec);
		}

	s_tty_unlock (a_serial);

	return	l_rc;
}



/*
 *   DESCRIPTION: Get exclusive lock for serial device, check and do if need initialization.
 *
 *   INPUTS:
 *	a_serial:	A context for serial comminication line
 *
 *   OUTPUTS:
 *	a_serial:
 *
 *   RETURNS:
 *	condition code
 */
int	t2r$tty_close (
			       T2R$_SERIAL	*a_serial
			       )
{
int	l_rc;

	if ( !(1 & s_tty_lock (a_serial)) )
		return	$LOG(STS$K_ERROR, "[#%d:<%s>] --- close failed", a_serial->fd, a_serial->devname);

	if ( !a_serial->state )
		l_rc = $LOG(STS$K_WARN, "Device <%s>  has not been initialized", a_serial->devname);
	else	l_rc = s_tty_close (a_serial), a_serial->state = T2R$K_STATE_IDLE;

	s_tty_unlock (a_serial);

	return	l_rc;
}



/*
 *   DESCRIPTION: Drop a has been left garbage from the serial line: flush the kernel FIFOs,
 *	then drain the line until it keeps silence for the inter-PDU interval.
 *
 *   INPUTS:
 *	a_serial:	A context of the serial line
 *
 *   OUTPUTS:
 *	NONE
 *
 *   RETURNS:
 *	condition code
 */
static int	s_tty_flush (
	const T2R$_SERIAL	*a_serial
		)
{
int	l_rc;
struct pollfd	l_pfd = {.fd = a_serial->fd, .events = POLLIN};
struct timespec l_now_ts, l_end_of_pdu_ts;
char	l_buf[MODBUS$SZ_MAXPDU];


	tcflush(a_serial->fd, TCIOFLUSH);

	/* Compute a time limit for draining of the line garbage */
	clock_gettime(CLOCK_MONOTONIC, &l_end_of_pdu_ts);
	__util$add_time (&l_end_of_pdu_ts, &a_serial->inter_pdu_ts, &l_end_of_pdu_ts);

	while ( !g_exit_flag )
		{
		/*
		 * Get current time and check: did we reach a time limit to read whole PDU ?
		 *
		 * 	0	- time1 == time2
		 *	0 >	- time1 < time2
		 *	0 <	- time1 > time2
		 */
		clock_gettime(CLOCK_MONOTONIC, &l_now_ts);

		if ( 0 < (l_rc = __util$cmp_time (&l_now_ts, &l_end_of_pdu_ts)) )
			break;


		if ( 0 > (l_rc = poll(&l_pfd, 1, 1 + a_serial->inter_pdu_usec / 1000)) )
			$LOG(STS$K_WARN, "[#%d:<%s>] poll()->%d, errno: %d", a_serial->fd, a_serial->devname, l_rc, errno);
		else	if ( !l_rc)
			continue;


		if ( 0 > (l_rc = read(a_serial->fd, l_buf, sizeof(l_buf))) )
			{
			if ( (errno == EAGAIN) || (errno == EWOULDBLOCK) || (errno == EINTR) )
				continue;

			$LOG(STS$K_WARN, "[#%d:<%s>] read()->%d, errno: %d", a_serial->fd, a_serial->devname, l_rc, errno);
			break;
			}
		}

	return	STS$K_SUCCESS;
}




/*
 *   DESCRIPTION: Execute a single MODBUS request-answer transaction on the RTU leg. The serial
 *	device is taken for an exclusive use for the whole transaction. A request to the has been
 *	configured Time Stamp pseudo device (unit/function match) is answered locally without
 *	touching the line. On an I/O error the line is flushed and a MODBUS exception code is
 *	returned to be delivered to the network requester.
 *
 *   INPUTS:
 *	a_serial:	A context of the serial line
 *	a_req_dsc:	A descriptor of the request PDU (without MBAP and without CRC)
 *
 *   OUTPUTS:
 *	a_resp_dsc:	A descriptor to accept the answer PDU (the CRC trailer is stripped off)
 *	a_excode:	A MODBUS exception code to be reported to the requester, 0 - no exception
 *
 *   RETURNS:
 *	condition code
 */
int	t2r$tty_exec_req (
		T2R$_SERIAL	*a_serial,
			void	*a_req_dsc,
			void	*a_resp_dsc,
			uint8_t	*a_excode
			)
{
int	l_rc, l_rcv;
T2R$_DESC	*l_req_dsc = a_req_dsc, *l_resp_dsc = a_resp_dsc;
MODBUS_REQ_T	*l_req_pdu;

	*a_excode = 0;

	l_req_pdu = (MODBUS_REQ_T *) l_req_dsc->data;

	/* Special hook to process request of local TS */
	if ( ( a_serial->flags & T2R$M_SERIAL_ADDTS )
	     && ( l_req_pdu->slave == a_serial->ts_unit_nr)
	     && ( l_req_pdu->fncode == a_serial->ts_fncode)
	     && ( ntohs(l_req_pdu->sa) == a_serial->ts_base_reg0) )
		{
		if ( !(1 & (l_rc = s_add_ts_to_pdu (a_req_dsc, a_resp_dsc))) )
			{
			l_rc = s_make_exception_resp(a_req_dsc, a_resp_dsc, *a_excode = l_rcv = MODBUS$K_EXCEPTION_ILLEGAL_DATA_ADDRESS);

			$PUTMSG_FAO(T2R$__EXCRPT, a_serial->fd, a_serial->devname, l_rcv, t2r$modbus_exc2str (l_rcv));
			}

		return	l_rc;
		}


	if ( !(1 & s_tty_lock (a_serial)) )
		return	$LOG(STS$K_ERROR, "[#%d:<%s>] --- cannot be allocated for exclusive I/O", a_serial->fd, a_serial->devname);


	if ( 1 & (l_rc = s_tty_rtu_tx (a_serial, l_req_dsc)) )
		l_rc = s_tty_rtu_rx (a_serial, l_req_dsc, l_resp_dsc);


	if ( !(1 & l_rc) )
		{
		s_tty_flush (a_serial);
		$IFTRACE(g_trace, "[#%d:<%s>] --- do flush on I/O error", a_serial->fd, a_serial->devname);
		}

	s_tty_unlock (a_serial);

	if ( !(1 & l_rc) )
		{
		l_rc = s_make_exception_resp(a_req_dsc, a_resp_dsc, *a_excode = l_rcv = MODBUS$K_EXCEPTION_SERVER_DEVICE_FAILURE);

		$PUTMSG_FAO(T2R$__EXCRPT, a_serial->fd, a_serial->devname, l_rcv, t2r$modbus_exc2str (l_rcv));
		}

	return	l_rc;
}
