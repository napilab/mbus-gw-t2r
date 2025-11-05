#define	__MODULE__	"T2R-MODBUS"

/*
**++
**
**  FACILITY: A yet another TCP to RTU gateway for MODBUS
**
**  ENVIRONMENT: Linux
**
**
**  DESCRIPTION: Auxiliary routines related to MODBUS
**
**  AUTHORS: Ruslan R. (The BadAss Sysman) Laishev
**
**  CREATION DATE:  29-SEP-2025
**
**  MODIFICATION HISTORY:
**
**
**--
*/


#include	<errno.h>
#include	<stdint.h>
#include	<endian.h>


#define		__FAC__	"T2R"
#define		__TFAC__ __FAC__ ": "					/* Special prefix for $TRACE			*/

#include	"utility_routines.h"
#include	"t2r_defs.h"
#include	"t2r_msgs.h"
#include	"t2r_modbus.h"


extern int	g_exit_flag, g_trace;

const unsigned short g_modbus_crc16_table[] = {
	  0x0000, 0xc0c1, 0xc181, 0x0140, 0xc301, 0x03c0, 0x0280, 0xc241,
	  0xc601, 0x06c0, 0x0780, 0xc741, 0x0500, 0xc5c1, 0xc481, 0x0440,
	  0xcc01, 0x0cc0, 0x0d80, 0xcd41, 0x0f00, 0xcfc1, 0xce81, 0x0e40,
	  0x0a00, 0xcac1, 0xcb81, 0x0b40, 0xc901, 0x09c0, 0x0880, 0xc841,
	  0xd801, 0x18c0, 0x1980, 0xd941, 0x1b00, 0xdbc1, 0xda81, 0x1a40,
	  0x1e00, 0xdec1, 0xdf81, 0x1f40, 0xdd01, 0x1dc0, 0x1c80, 0xdc41,
	  0x1400, 0xd4c1, 0xd581, 0x1540, 0xd701, 0x17c0, 0x1680, 0xd641,
	  0xd201, 0x12c0, 0x1380, 0xd341, 0x1100, 0xd1c1, 0xd081, 0x1040,
	  0xf001, 0x30c0, 0x3180, 0xf141, 0x3300, 0xf3c1, 0xf281, 0x3240,
	  0x3600, 0xf6c1, 0xf781, 0x3740, 0xf501, 0x35c0, 0x3480, 0xf441,
	  0x3c00, 0xfcc1, 0xfd81, 0x3d40, 0xff01, 0x3fc0, 0x3e80, 0xfe41,
	  0xfa01, 0x3ac0, 0x3b80, 0xfb41, 0x3900, 0xf9c1, 0xf881, 0x3840,
	  0x2800, 0xe8c1, 0xe981, 0x2940, 0xeb01, 0x2bc0, 0x2a80, 0xea41,
	  0xee01, 0x2ec0, 0x2f80, 0xef41, 0x2d00, 0xedc1, 0xec81, 0x2c40,
	  0xe401, 0x24c0, 0x2580, 0xe541, 0x2700, 0xe7c1, 0xe681, 0x2640,
	  0x2200, 0xe2c1, 0xe381, 0x2340, 0xe101, 0x21c0, 0x2080, 0xe041,
	  0xa001, 0x60c0, 0x6180, 0xa141, 0x6300, 0xa3c1, 0xa281, 0x6240,
	  0x6600, 0xa6c1, 0xa781, 0x6740, 0xa501, 0x65c0, 0x6480, 0xa441,
	  0x6c00, 0xacc1, 0xad81, 0x6d40, 0xaf01, 0x6fc0, 0x6e80, 0xae41,
	  0xaa01, 0x6ac0, 0x6b80, 0xab41, 0x6900, 0xa9c1, 0xa881, 0x6840,
	  0x7800, 0xb8c1, 0xb981, 0x7940, 0xbb01, 0x7bc0, 0x7a80, 0xba41,
	  0xbe01, 0x7ec0, 0x7f80, 0xbf41, 0x7d00, 0xbdc1, 0xbc81, 0x7c40,
	  0xb401, 0x74c0, 0x7580, 0xb541, 0x7700, 0xb7c1, 0xb681, 0x7640,
	  0x7200, 0xb2c1, 0xb381, 0x7340, 0xb101, 0x71c0, 0x7080, 0xb041,
	  0x5000, 0x90c1, 0x9181, 0x5140, 0x9301, 0x53c0, 0x5280, 0x9241,
	  0x9601, 0x56c0, 0x5780, 0x9741, 0x5500, 0x95c1, 0x9481, 0x5440,
	  0x9c01, 0x5cc0, 0x5d80, 0x9d41, 0x5f00, 0x9fc1, 0x9e81, 0x5e40,
	  0x5a00, 0x9ac1, 0x9b81, 0x5b40, 0x9901, 0x59c0, 0x5880, 0x9841,
	  0x8801, 0x48c0, 0x4980, 0x8941, 0x4b00, 0x8bc1, 0x8a81, 0x4a40,
	  0x4e00, 0x8ec1, 0x8f81, 0x4f40, 0x8d01, 0x4dc0, 0x4c80, 0x8c41,
	  0x4400, 0x84c1, 0x8581, 0x4540, 0x8701, 0x47c0, 0x4680, 0x8641,
	  0x8201, 0x42c0, 0x4380, 0x8341, 0x4100, 0x81c1, 0x8081, 0x4040
};



static const char *s_modbus_fn2str(
			int	a_fn
		)
{
	switch ( (a_fn & (~MODBUS$M_FN_EXCEPTION)) )
		{
		case	MODBUS$K_FN_READ_COILS:				return	"READ_COILS";
		case	MODBUS$K_FN_READ_DISCRETE_INPUTS:		return	"READ_DISCRETE_INPUTS";
		case	MODBUS$K_FN_READ_HOLDING_REGISTERS:		return	"READ_HOLDING_REGISTERS";
		case	MODBUS$K_FN_READ_INPUT_REGISTERS:		return	"READ_INPUT_REGISTERS";
		case	MODBUS$K_FN_WRITE_SINGLE_COIL:			return	"FN_WRITE_SINGLE_COIL";
		case	MODBUS$K_FN_WRITE_SINGLE_REGISTER:		return	"WRITE_SINGLE_REGISTER";
		case	MODBUS$K_FN_WRITE_MULTIPLE_COILS:		return	"WRITE_MULTIPLE_COILS";
		case	MODBUS$K_FN_WRITE_MULTIPLE_REGISTERS:		return	"WRITE_MULTIPLE_REGISTERS";
		case	MODBUS$K_FN_REPORT_SERVER_ID:			return	"REPORT_SERVER_ID";
		case	MODBUS$K_FN_READ_FILE_RECORD:			return	"READ_FILE_RECORD";
		case	MODBUS$K_FN_WRITE_FILE_RECORD:			return	"WRITE_FILE_RECORD";
		case	MODBUS$K_FN_MASK_WRITE_REGISTER:		return	"MASK_WRITE_REGISTER";
		case	MODBUS$K_FN_READ_WRITE_MULTIPLE_REGISTERS:	return	"READ_WRITE_MULTIPLE_REGISTERS";
		case	MODBUS$K_FN_READ_FIFO_QUEUE:			return	"READ_FIFO_QUEUE";
		}

	return	"UNKNOWN";
}


static const char *s_modbus_exc2str(
		int	a_exc
		)
{

	switch ( a_exc )
		{
		case	MODBUS$K_EXCEPTION_NOERROR:			return	"NONE";

		case	MODBUS$K_EXCEPTION_ILLEGAL_FUNCTION:		return	"LLEGAL_FUNCTION";
		case	MODBUS$K_EXCEPTION_ILLEGAL_DATA_ADDRESS:	return	"ILLEGAL_DATA_ADDRESS";
		case	MODBUS$K_EXCEPTION_ILLEGAL_DATA_VALUE:		return	"ILLEGAL_DATA_VALUE";
		case	MODBUS$K_EXCEPTION_SERVER_DEVICE_FAILURE:	return	"SERVER_DEVICE_FAILURE";
		case	MODBUS$K_EXCEPTION_ACKNOWLEDGE:			return	"ACKNOWLEDGE";
		case	MODBUS$K_EXCEPTION_SERVER_DEVICE_BUSY:		return	"SERVER_DEVICE_BUSY";
		case	MODBUS$K_EXCEPTION_NEGATIVE_ACKNOWLEDGE:	return	"NEGATIVE_ACKNOWLEDGE";
		case	MODBUS$K_EXCEPTION_MEMORY_PARITY_ERROR:		return	"MEMORY_PARITY_ERROR";
		case	MODBUS$K_EXCEPTION_GW_NO_PATH:			return	"GW_NO_PATH";
		case	MODBUS$K_EXCEPTION_TGT_DEV_NOANSWRD:		return	"TGT_DEV_NOANSWRD";

		}

       return	"UNKNOWN";
}





void	t2r$mbap_print (
		const char	*a__mod,
		const char	*a__fi,
		const int	a__li,
		const char	*a_pref,
		const void	*a_data,
			int	a_datalen
		)
{
MODBUS_ADU_T *l_mbap = (MODBUS_ADU_T *) a_data;
uint16_t	l_txid, l_proto, l_len;
MODBUS_PDU_T	*l_pdu;
char	l_buf[MODBUS$SZ_MAXPDU * 2] = {0};

	l_txid  = ntohs(l_mbap->txid);
	l_proto =  ntohs(l_mbap->proto);
	l_len =  ntohs(l_mbap->len);
	l_pdu = (MODBUS_PDU_T *) &l_mbap->unit;

	__util$bin2hex (l_pdu, l_buf, l_len);


	__util$trace(g_trace, "%s MODBUS ADU %d octets [TXID: %d(%#x), PROTO: %d(%#x), LEN: %d(%#x) octets, UNIT: %d, "
		"%s: %d(%#x-%s), "
		"\n\tPDU Dump: %s]", a__mod,a__fi, a__li,
		a_pref, a_datalen,
		l_txid, l_txid, l_proto, l_proto, l_len, l_len, l_mbap->unit[0],
		(l_pdu->fncode > MODBUS$M_FN_EXCEPTION) ?  "EXC" : "FUN",
		l_pdu->fncode, l_pdu->fncode, s_modbus_fn2str(l_pdu->fncode),
		l_buf);
}




void	t2r$rtu_print (
		const char	*a__mod,
		const char	*a__fi,
		const int	a__li,
		const char	*a_pref,
		const void	*a_data,
			int	a_datalen,
		uint16_t	a_crc
		)
{
uint8_t		*l_data;
uint8_t		l_slave, l_fncode, l_excode = 0;
uint16_t	l_crc = 0, l_crc_check = 0;
char	l_buf[MODBUS$SZ_MAXPDU * 2] = {0};


	l_data = (uint8_t *) a_data;

	l_slave = *(l_data);
	l_fncode = *(l_data + 1);

	if ( l_fncode > MODBUS$M_FN_EXCEPTION )
		{
		l_excode = *(l_data + 2);
		l_fncode &= ~MODBUS$M_FN_EXCEPTION;
		}

	if ( !a_crc )
		{
		l_crc = *((uint16_t *) (l_data + a_datalen - 2));
		l_crc_check = s_modbus_crc_calculate (a_data, a_datalen - 2);
		}
	else	l_crc = l_crc_check;

	__util$bin2hex (a_data, l_buf, a_datalen);

	if ( !l_excode )
		__util$trace(g_trace, "%s RTU PDU %d octets [SLAVE: %d, FUN: %d(%#x-%s), CRC: %#04x(calculated: %#04x), "
			"\n\tPDU Dump: %s]", a__mod,a__fi, a__li,
			a_pref, a_datalen,
			l_slave,
			l_fncode, l_fncode, s_modbus_fn2str(l_fncode),
			l_crc, l_crc_check,
			l_buf);
	else	__util$trace(g_trace, "%s RTU PDU %d octets [SLAVE: %d, FUN: %d(%#x-%s), ERR: %d(%#x-%s), CRC: %#04x(calculated: %#04x), "
			 "\n\tPDU Dump: %s]", a__mod,a__fi, a__li,
			a_pref, a_datalen,
			l_slave,
			l_fncode, l_fncode, s_modbus_fn2str(l_fncode),
			l_excode, l_excode, s_modbus_exc2str(l_excode),
			l_crc, l_crc_check,
			l_buf);

}


/*
 *   DESCRIPTION: Transforms  MBAP-like (MODBUS Application Protocol) request has been received frop TCP-leg
 *	to PDU is supposed to be sent to target device over RTU-leg.
 *
 *   INPUTS:
 *	a_src:		A buffer with MODBAS ADU
 *	a_srcsz:	A size of MBAP ADU
 *
 *   OUTPUTS:
 *	a_dst:		A buffer to accept RTU-like PDU
 *	a_dstsz:	A size of the buffer (MAX RTU PDU at least)
 *	a_dstlen:	A size of the RTU PDU
 *
 *   RETURNS;
 *	condition code
 */
int t2r$mbap_2_rtu_pdu (
		const void	*a_src_dsc,
			void	*a_dst_dsc
		)
{
const T2R$_DESC	*l_src_dsc = a_src_dsc;
T2R$_DESC	*l_dst_dsc = a_dst_dsc;
const MODBUS_ADU_T *l_mbap = (MODBUS_ADU_T *) l_src_dsc->data;
uint16_t	l_len;



	if ( (l_len = ntohs(l_mbap->len)) > l_dst_dsc->sz )
		return	$LOG(STS$K_ERROR, "No room for output data (%d > %d octets)", l_len, l_dst_dsc->sz );


	memcpy(l_dst_dsc->data, &l_mbap->unit, l_len);				/* Copy request part of MBAP to RTU PUD buffer*/
	l_dst_dsc->len = l_len;							/* Adjust data length in the output buffer */

	return	STS$K_SUCCESS;
}



/*
 *   DESCRIPTION: Put RTU PDU to the MBAP-like ADU after MBAP header part.
 *
 *   INPUTS:
 *	a_src:		A buffer with MODBUS RTU PDU
 *
 *   OUTPUTS:
 *	a_dst:		A buffer to accept MBAP-like ADU
 *
 *   RETURNS;
 *	condition code
 */
int t2r$rtu_pdu_2_mbap (
		const void	*a_src_dsc,
			void	*a_dst_dsc
		)
{
const T2R$_DESC	*l_src_dsc = a_src_dsc;
T2R$_DESC	*l_dst_dsc = a_dst_dsc;
MODBUS_ADU_T *l_mbap = (MODBUS_ADU_T *) l_dst_dsc->data;


	if ( l_src_dsc->len > (l_dst_dsc->sz - MODBUS$SZ_MINMBAP) )
		return	$LOG(STS$K_ERROR, "No room for output data (%d > %d octets)", l_src_dsc->len,  (l_dst_dsc->sz - MODBUS$SZ_MINMBAP));

	memcpy(l_mbap->unit, l_src_dsc->data, l_src_dsc->len);			/* Copy RTU PDU part into the MBAP ADU */
	l_mbap->len = htons(l_src_dsc->len);					/* Correct .len field of the MBAP header */

	l_dst_dsc->len  = MODBUS$SZ_MBAPHDR + l_src_dsc->len;			/* Adjust data length in the output buffer */

	return	STS$K_SUCCESS;
}





/*
 *   DESCRIPTION: Convert MODBUS PDU (in binary representative) to ASCII format:
 *	- got octet string: 0x01 0x03 0x00 0x00 0x00 0x01 0xFB
 *	- out ASCII string: ':'_"010300000001FB"_"0D0A" (concatenation symbol "_" - is for readability purpose)
 *
 *   INPUTS:
 *	a_src:
 *
 *   OUTPUTS:
 *	a_dst:
 *
 *   RETURNS:
 *	<condition code>
 */
int t2r$ascii_pdu2rtu (
		const void	*a_src_dsc,
			void	*a_dst_dsc
		)
{
const T2R$_DESC	*l_src_dsc = a_src_dsc;
T2R$_DESC	*l_dst_dsc = a_dst_dsc;
int	l_len;
char	*l_dst;

	l_len = l_src_dsc->len * 2;							/* Compute length of hex string */


	if ( (l_len + 1 + 2) > l_dst_dsc->sz  )						/* Is there space for ':'_<hex_string>_<CR><LF> ? */
		return	STS$K_ERROR;

	l_dst = l_dst_dsc->data;

	*(l_dst++) = ';';								/* Prefix of HEX data */
	__util$bin2hex (l_src_dsc->data, l_dst, l_src_dsc->len);			/* Convert octet string to hex string */
	l_dst += (l_src_dsc->len * 2);							/* Point <l_dst> to end of "ascii" data */

	*(l_dst++) = '\r';								/* Final <CR><LF> p[air */
	*(l_dst++) = '\n';

	l_dst_dsc->len = l_len + 1 + 2;							/* Adjust output data length */

	return	STS$K_SUCCESS;
}




/*
 *   DESCRIPTION: Convert MODBUS ASCII string to MODBUS PDU
 *	- get ASCII string: ':010300000001FB0D0A"
 *	- out octet string: 0x01 0x03 0x00 0x00 0x00 0x01 0xFB (remove leading ':' and <CR><LF> sequence)
 *
 *   INPUTS:
 *	a_src:
 *	a_srclen:
 *	a_dstsz:
 *
 *   OUTPUTS:
 *	a_dst:
 *	a_retlen
 *
 *   RETURNS:
 *	<errno>
 */
int t2r$rtu_pdu2ascii (
		const void	*a_src_dsc,
			void	*a_dst_dsc
		)
{
const T2R$_DESC	*l_src_dsc = a_src_dsc;
T2R$_DESC	*l_dst_dsc = a_dst_dsc;
int l_len;
const char	*l_src;


	/* Sanity checks for correct framing  */
	if ( l_src_dsc->len < 3 )							/* Too short ? */
		return	$LOG(STS$K_ERROR, "Tooshort request (%d < %d octets)", l_src_dsc->len, 3);

	l_src = l_src_dsc->data;

	if ( *l_src != ':' )								/* Check for prefix ':' */
		return	STS$K_ERROR;

	if ( (*(l_src + l_src_dsc->len - 1) != '\r') || (*(l_src + l_src_dsc->len - 1) != '\n') )	/* Invalid termination sequence */
		return	STS$K_ERROR;



	l_len = l_src_dsc->len - 1 - 2;							/* Compute a length of hex part of the MODBUS PDU */
	l_src++;									/* Advance pointer to hex part */
	__util$hex2bin (l_src, l_dst_dsc->data, l_len);

	l_dst_dsc->len = l_len / 2;

	return	STS$K_SUCCESS;
}




