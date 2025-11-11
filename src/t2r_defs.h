
/*
**++
**
**  FACILITY: A yet another gateway for MODBUS (TCP 2 RTU)
**
**  ENVIRONMENT: Linux
**
**  DESCRIPTION: Data Srtuctures and constant definitions is supposed to be used across all MODBUS-TCP2RTU's modules
**
**
**  AUTHORS: Ruslan R. (The BadAss Sysman) Laishev
**
**  CREATION DATE:  10-AUG-2025
**
**  MODIFICATION HISTORY:
**
**--
*/


#ifndef	__MODBUS_TCP2RTU_DEFS__
#define __MODBUS_TCP2RTU_DEFS__	1

#include	<netinet/in.h>
#include	<arpa/inet.h>
#include	<termios.h>
#include	<pthread.h>

#include	"utility_routines.h"					/* Starlet's Utility/General purpose routines */
#include	"t2r_modbus.h"




#ifdef __cplusplus
extern "C" {
#endif





#ifndef IPv4_BYTES
#define IPv4_BYTES_FMT "%u.%u.%u.%u"
#define IPv4_BYTES(addr) \
		(uint8_t) ((addr) & 0xFF), \
		(uint8_t) (((addr) >> 8) & 0xFF),\
		(uint8_t) (((addr) >> 16) & 0xFF),\
		(uint8_t) (((addr) >> 24) & 0xFF)
#endif



enum {
	T2R$K_STATE_IDLE = 0,

	T2R$K_STATE_READY,						/* Idle session context has been created */

	T2R$K_STATE_PDU_RECV,						/* Receiving whole MODBUS's PDU */
	T2R$K_STATE_PDU_ENQD,						/* Received MODBUS PDU is enqueued for processing */

	T2R$K_STATE_PDU_XMIT,						/* We have got an answer from RTU leg and sending answer MODBUS PDU */
	T2R$K_STATE_SHUT,						/* Session should be closed */

	T2R$K_STATE_EOL
};


#define		T2R$SZ_BUFF	2048
#define		T2R$K_MAX_LISTENERS	128
#define		T2R$K_MAX_SERIALS	128
#define		T2R$K_IDLE_TMO_SEC	1200				/* Global timeout for idle sessions */
#define		T2R$K_NET_TMO_SEC	300				/* A timeout for reading data from network socket */
#define		T2R$K_TTY_DEVNAME	64


#define		T2R$K_TS_FNCODE	(MODBUS$K_FN_READ_HOLDING_REGISTERS)
#define		T2R$K_TS_BASE_REG0	7777

typedef struct	t2r$_desc {
	int	sz,
		len;
	char	data[0];
} T2R$_DESC;

#define	$DESCRIPTOR_S(dsc_name, dsc_sz) struct {int sz; int len; char data[dsc_sz];} dsc_name;  dsc_name.sz = dsc_sz; dsc_name.len = 0;

enum {
	__SERIAL_RS485 = 0,
	__SERIAL_ADDTS
};

enum {
	T2R$M_SERIAL_RS485 = (1 << __SERIAL_RS485),			/* RS485 specific initialization */
	T2R$M_SERIAL_ADDTS = (1 << __SERIAL_ADDTS),			/* Add TimeStamp at end of response PDU */

};

typedef struct	t2r$_serial {
		int	state;						/* Session state, see T2R$K_STATE_* constants */
	pthread_mutex_t	lock;						/* Coordinate an access between multiple TCP clients */

		int	fd;						/* I/O descriptor for serial device */
	struct timespec						/* See <pdutmo> */
			inter_pdu_ts;

		char devname[T2R$K_TTY_DEVNAME + 1];			/* "COM1", "/dev/ttyS0" */


		int	flags;						/* Optional RS485 specific initialization */

		int	baud,						/* Bits-per-second rate of serial line */
			stopbits,
			databits,
			parity,
			flow;

		int	anstmo_msec,					/* A time to wait an answer for has been sent request */
			inter_pdu_usec,					/* A delay before and after PDU on RTU */
			onebyte_time_usec,				/* Estimated time to send byte over serial line in nanoseconds */
			ts_fncode,
			ts_base_reg0;

	struct termios	savedtios,
			tios;
} T2R$_SERIAL;



typedef struct	t2r$_listener {
		int	fd,						/* Network socket descriptor */
			proto,						/* IPPROTO_TCP or IPPROTO_UDP */
			iotmo,
			connlm;						/* Connection limit on this listener */
	struct sockaddr_in sk;						/* Socket address structure: TCP, 0.0.0.0:502 */
		char target[T2R$K_TTY_DEVNAME + 1];			/* "COM1", "/dev/ttyS0" */

	T2R$_SERIAL	*serial;					/* A context to has been defined serial device */

} T2R$_LISTENER;





typedef struct	t2r$__session {
		int	state;						/* Session state, see T2R$K_STATE_* constants */

	struct timespec	lastio_ts;					/* Timestamp of last I/O operation on the session */

		int	sd;						/* Network socket descriptor */
	struct sockaddr_in sk;						/* Socket address structure */

		void	*netbuf_dsc;					/* A descriptor of buffer for network I/O */
		int	datalen;					/* A size of data to be read from the network */


	T2R$_SERIAL	*target;
} T2R$_SESSION;


int	t2r$net_start_listeners (void);
int	t2r$net_stop_listeners (void);


#define	$MBAP_PRINT(a_pref,a_data,a_datalen)	t2r$mbap_print(__MODULE__, __FUNCTION__, __LINE__, a_pref, a_data, a_datalen)
void	t2r$mbap_print (const char *a__mod, const char *a__fi, const int a__li,
		const char *a_pref, const char a_data[], int a_datalen);


#define	$RTU_PRINT(a_pref,a_data,a_datalen,a_crc)	t2r$rtu_print(__MODULE__, __FUNCTION__, __LINE__, a_pref, a_data, a_datalen, a_crc)
void	t2r$rtu_print (const char *a__mod, const char *a__fi, const int a__li,
		const char *a_pref, const void *a_data, int a_datalen, uint16_t a_crc);

int	t2r$tty_open(T2R$_SERIAL *);
int	t2r$tty_close(T2R$_SERIAL *);
int	t2r$tty_exec_req(T2R$_SERIAL *, void *a_req_dsc, void *a_resp_dsc, uint8_t *a_excode);

#ifdef __cplusplus
}
#endif

#endif	/*	__MODBUS_TCP2RTU_DEFS__	*/
