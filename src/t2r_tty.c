#define	__MODULE__	"T2R-TTY"

/*
**++
**
**  FACILITY: A yet another TCP to RTU gateway for MODBUS
**
**  ENVIRONMENT: Linux
**
**
**  DESCRIPTION: This coule cover TTY related functionality. Serial line open and initialization, read\write and timeout processing.
**	Send MODBUS PDU to serial port and read with CRC checking.
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
#include	<termios.h>
#include	<fcntl.h>
#include	<termios.h>
#include	<sys/uio.h>
#include	<linux/serial.h>
#include	<sys/ioctl.h>
#include	<linux/i2c.h>


#define		__FAC__	"T2R"
#define		__TFAC__ __FAC__ ": "					/* Special prefix for $TRACE			*/

#include	"utility_routines.h"
#include	"t2r_defs.h"
#include	"t2r_msgs.h"
#include	"t2r_modbus.h"


typedef void * (*th_func_t)(void *);					/* To eliminate pthread_create warning */




extern int	g_exit_flag, g_trace;

extern T2R$_SERIAL	g_serials[];
extern	int		g_serials_nr;

static struct pollfd	s_pfd_lsnr[T2R$K_MAX_SERIALS] = {-1};



static inline int s_tty_lock (
		T2R$_SERIAL	*a_serial
		)
{
struct timespec l_now, l_tmo = {.tv_sec = 13}, l_etime;
int	l_rc;

	for (int i = 0; i < 3; i++)
		{
		if ( l_rc = clock_gettime(CLOCK_REALTIME_COARSE, &l_now) )
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


static inline int s_tty_unlock (
		T2R$_SERIAL	*a_serial
		)
{
int	l_rc;

		l_rc = pthread_mutex_unlock(&a_serial->lock);
		//$IFTRACE(g_trace, "Release lock for <%s> (errno: %d)", a_serial->devname, l_rc ? errno: 0);

		return	STS$K_SUCCESS;
}



/*
 *   DESCRIPTION: Do delay calling thread for specified microseconds.
 *
 *   INPUTS:
 *	a_usec:		A delay in microseconds
 *
 *   OUTPUTS:
 *	NONE
 *
 *   RETURNS:
 *	NONE
 */
static inline void s_tty_delay(int usec)
{
struct	timespec	l_delta = {0}, l_rem = {0};
int	l_rc;

	l_delta.tv_sec = 1;
	l_delta.tv_nsec = 3L * 1024L;

	//while ( (!(l_rc = nanosleep(&l_delta, &l_delta))) && (l_delta.tv_sec || l_delta.tv_nsec));
	l_rc = nanosleep(&l_delta, &l_rem);

	return;

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

	l_iov[IOV$K_CRC].iov_base = &l_crc;
	l_len += l_iov[IOV$K_CRC].iov_len = sizeof(l_crc);


											/* Predelay ... */
	for ( l_inter_pdu_ts = a_serial->inter_pdu_ts; nanosleep(&l_inter_pdu_ts,  &l_inter_pdu_ts); );


	if ( 0 > (l_rc = writev(a_serial->fd, l_iov,  IOV$K_MAX)) )
		l_rc = $LOG(STS$K_ERROR, "[#%d:<%s>] Xmit of %d octets failed, errno: %d", a_serial->fd, a_serial->devname, l_len, errno);
	else if ( l_rc != l_len )
		l_rc = $LOG(STS$K_ERROR, "[#%d:<%s>] Sent %d octets (from %d), errno: %d", a_serial->fd, a_serial->devname, l_len, errno);
	else	l_rc = STS$K_SUCCESS;


											/* Postdelay ... */
	for ( l_inter_pdu_ts = a_serial->inter_pdu_ts; nanosleep(&l_inter_pdu_ts,  &l_inter_pdu_ts); );

	if ( g_trace )
		$RTU_PRINT("Sent", a_req_pdu->data, a_req_pdu->len, l_crc);

	return	l_rc;
}


/*
 *   DESCRIPTION: Read data from serial line, check that whole ADU has been received, validate CRC.
 *	1. Wait for POLLIN by using a long timeout - we expect that device can take a time for processing request
 *	2. Reading bytes using two timeouts:
 *		- first one: for whole PDU
 *		- second for reading single byte
 */
static int	s_tty_rtu_rx (
	const T2R$_SERIAL	*a_serial,
		const T2R$_DESC	*a_req_pdu,
		T2R$_DESC	*a_resp_pdu
		)
{
int	l_rc;
uint16_t l_crc, l_crc_check;
struct pollfd	l_pfd = {.fd = a_serial->fd, .events = POLLIN};
struct timespec l_now_ts, l_end_of_pdu_ts;
MODBUS_PDU_T	*l_req_pdu, *l_resp_pdu;

	a_resp_pdu->len = 0;

	if ( 0 > (l_rc = poll(&l_pfd, 1, a_serial->anstmo_msec)) )			/* Wait for first (at least) byte of answer  */
		$LOG(STS$K_WARN, "[#%d:<%s>] Did not get an answer in %d msecs --- abort I/O operation", a_serial->fd, a_serial->devname,
		     a_serial->anstmo_msec);


	/* Compute a timeout for reading whole RTU PDU from serial device*/
	clock_gettime(CLOCK_MONOTONIC_COARSE, &l_end_of_pdu_ts);
	__util$add_time (&l_end_of_pdu_ts, &a_serial->inter_pdu_ts, &l_end_of_pdu_ts);


	//$IFTRACE(g_trace, "Start reading PDU ...");

	while ( !g_exit_flag )
		{
		/*
		 * Get current time and check: did we reach a time limit to read whole PDU ?
		 *
		 * 	0	- time1 == time2
		 *	0 >	- time1 < time2
		 *	0 <	- time1 > time2
		 */
		clock_gettime(CLOCK_MONOTONIC_COARSE, &l_now_ts);

		if ( 0 < (l_rc = __util$cmp_time (&l_now_ts, &l_end_of_pdu_ts)) )
			break;


		if ( 0 > (l_rc = poll(&l_pfd, 1, 1 + a_serial->onebyte_time_usec /1024)) )
			{
			$LOG(STS$K_WARN, "[#%d:<%s>] poll()->%d, errno: %d", a_serial->fd, a_serial->devname, l_rc, errno);
			}


		if ( 0 > (l_rc = read(a_serial->fd, a_resp_pdu->data + a_resp_pdu->len, a_resp_pdu->sz - a_resp_pdu->len)) )
			{
			$LOG(STS$K_WARN, "[#%d:<%s>] read()->%d, errno: %d", a_serial->fd, a_serial->devname, l_rc, errno);
			break;
			}
		else if ( l_rc )
			{
			a_resp_pdu->len += l_rc;			/* Adjust data counter in the input buffer */
			}
		}


	if ( !a_resp_pdu->len )
		return	$LOG(STS$K_ERROR, "[#%d:<%s>] No data has been received", a_serial->fd, a_serial->devname);


	if ( a_resp_pdu->len < MODBUS$SZ_MINRTU )
		return	$LOG(STS$K_ERROR, "[#%d:<%s>] RTU data too short (%d < %d octets)", a_serial->fd, a_serial->devname, a_resp_pdu->len, MODBUS$SZ_MINRTU);


	if ( g_trace )
		$RTU_PRINT("Rcvd", a_resp_pdu->data, a_resp_pdu->len, 0);


	l_crc = s_modbus_crc_calculate (a_resp_pdu->data, a_resp_pdu->len - 2);	/* Compute CRC for data w/o last 2 octets */
	l_crc_check = * ((uint16_t *) (a_resp_pdu->data + a_resp_pdu->len - 2));

	if ( l_crc != l_crc_check )
		return	$LOG(STS$K_ERROR, "[#%d:<%s>] RTU data --- CRC16 check error", a_serial->fd, a_serial->devname);


	l_req_pdu = (MODBUS_PDU_T *) a_req_pdu->data;
	l_resp_pdu = (MODBUS_PDU_T *) a_resp_pdu->data;

	if ( (l_req_pdu->slave != l_resp_pdu->slave) || (l_req_pdu->fncode != (l_resp_pdu->fncode & (~MODBUS$M_FN_EXCEPTION))) )
		return	$LOG(STS$K_WARN, "[#%d:<%s>] Slave: (REQ: %d vs RESP: %d) or Function (REQ: %d vs RESP: %d) --- mismatch",
			     a_serial->fd, a_serial->devname, l_req_pdu->slave, l_resp_pdu->slave, l_req_pdu->fncode, l_resp_pdu->fncode);


	a_resp_pdu->len -= 2;								/* Reduce a length of data for CRC16 */

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
		case 7200:	tspeed = B7200;		break;
		#endif

		#if defined(B9600)
		case 9600:	l_speed = B9600;	break;
		#endif

		#if defined(B12000)
		case 12000:	tspeed = B12000;	break;
		#endif

		#if defined(B14400)
		case 14400:	tspeed = B14400;	break;
		#endif

		#if defined(B19200)
		case 19200:	l_speed = B19200;	break;
		#elif defined(EXTA)
		case 19200:	tspeed = EXTA;		break;
		#endif

		#if defined(B38400)
		case 38400:	l_speed = B38400;	break;
		#elif defined(EXTB)
		case 38400:	tspeed = EXTB;		break;
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
 *   DESCRIPTION: Sets up a serial port for RTU communications
 *	Stolen and adopted from LIBMODBUS\MODBUS-RTU.C.
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
		return	$LOG(STS$K_ERROR, "Can't open the device <%s>, errno: %d", a_serial->devname, errno);

	memset(&a_serial->savedtios, 0, sizeof(struct termios));

	if ( l_rc = tcgetattr(a_serial->fd , &a_serial->savedtios) )
		return	close(a_serial->fd ), $LOG(STS$K_ERROR, "Error tcgetattr(%s)->%d, errno: %d", a_serial->devname, l_rc, errno);

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

	uint16_t *la_put_be16(uint16_t *a_buf, uint16_t a_word16)		/* Lyambda function to put 16 bit work to buffer as a BE\NBO */
		{
		*(a_buf) = htons(a_word16);
		return	(a_buf + 1);
		}



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
		/* 64bit of seconds pack into 4x16bit in NBO */
		l_valptr = la_put_be16(l_valptr, l_now.tv_sec >> 24);			/* R0 */
		l_valptr = la_put_be16(l_valptr, (l_now.tv_sec>> 16) & 0xffFF);		/* R1 */
		l_valptr = la_put_be16(l_valptr, (l_now.tv_sec>> 8) & 0xffFF);		/* R2 */
		l_valptr = la_put_be16(l_valptr, (l_now.tv_sec & 0xffFF) );		/* R3 */

		l_resp_dsc->len += (4 * 2);						/* Adjust size of data to 4xbe16 */
		l_resp_pdu->bc += (4 * 2);
		}

	if (  l_nr_regs >= (4 + 4) )
		{
		/* 64bit of nanoseconds pack into 4x16bit in NBO */
		l_valptr = la_put_be16(l_valptr, l_now.tv_nsec >> 24);			/* R4 */
		l_valptr = la_put_be16(l_valptr, (l_now.tv_nsec>> 16) & 0xffFF);	/* R5 */
		l_valptr = la_put_be16(l_valptr, (l_now.tv_nsec>> 8) & 0xffFF);		/* R6 */
		l_valptr = la_put_be16(l_valptr, (l_now.tv_nsec & 0xffFF) );		/* R7 */

		l_resp_dsc->len += (4 * 2);						/* Adjust size of data to 4xbe16 */
		l_resp_pdu->bc += (4 * 2);
		}

	if (  l_nr_regs == (4 + 4 + 1) )
		{
		localtime_r(&l_now.tv_sec, &l_tm);

		l_valptr = la_put_be16(l_valptr, l_tm.tm_gmtoff);			/* R8 */

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
		l_rc = $LOG(STS$K_INFO, "Device <%s>  --- has been initialized", a_serial->devname);
	else	l_rc = s_tty_open (a_serial), a_serial->state = T2R$K_STATE_READY;

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
 *   DESCRIPTION: Read data from serial line, check that whole ADU has been received, validate CRC.
 *	1. Wait for POLLIN by using a long timeout - we expect that device can take a time for processing request
 *	2. Reading bytes using two timeouts:
 *		- first one: for whole PDU
 *		- second for reading single byte
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

	/* Compute a timeout for reading whole RTU PDU from serial device*/
	clock_gettime(CLOCK_MONOTONIC_COARSE, &l_end_of_pdu_ts);
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
		clock_gettime(CLOCK_MONOTONIC_COARSE, &l_now_ts);

		if ( 0 < (l_rc = __util$cmp_time (&l_now_ts, &l_end_of_pdu_ts)) )
			break;


		if ( 0 > (l_rc = poll(&l_pfd, 1, 1 + a_serial->inter_pdu_usec /1024)) )
			$LOG(STS$K_WARN, "[#%d:<%s>] poll()->%d, errno: %d", a_serial->fd, a_serial->devname, l_rc, errno);
		else	if ( !l_rc)
			continue;


		if ( 0 > (l_rc = read(a_serial->fd, l_buf, sizeof(l_buf))) )
			{
			$LOG(STS$K_WARN, "[#%d:<%s>] read()->%d, errno: %d", a_serial->fd, a_serial->devname, l_rc, errno);
			break;
			}
		}
}




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
	     && ( l_req_pdu->fncode == a_serial->ts_fncode)
	     && ( ntohs(l_req_pdu->sa) == a_serial->ts_base_reg0) )
		{
		if ( !(1 & (l_rc = s_add_ts_to_pdu (a_req_dsc, a_resp_dsc))) )
			{
			l_rc = s_make_exception_resp(a_req_dsc, a_resp_dsc, *a_excode = l_rcv = MODBUS$K_EXCEPTION_ILLEGAL_DATA_ADDRESS);

			$LOG(STS$K_WARN, "[#%d:<%s>] --- report error (%d/%#x/%s) to requester", a_serial->fd, a_serial->devname,
				  l_rcv, l_rcv, t2r$modbus_exc2str (l_rcv));
			}

		return	l_rc;
		}


	if ( !(1 & s_tty_lock (a_serial)) )
		return	$LOG(STS$K_ERROR, "[#%d:<%s>] --- cannot be allocated for exclusive I/O", a_serial->devname, a_serial->fd);


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

		$LOG(STS$K_WARN, "[#%d:<%s>] --- report error (%d/%#x/%s) to requester", a_serial->fd, a_serial->devname,
		     l_rcv, l_rcv, t2r$modbus_exc2str (l_rcv));
		}

	return	l_rc;
}
