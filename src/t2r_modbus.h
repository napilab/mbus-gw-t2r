/*
**++
**
**  FACILITY:  MODBUS over TCP
**
**  ABSTRACT: This story is based on the :
**      - "MODBUS Application Protocol Specification V1.1b3"
**      - "MODBUS Messaging on TCP/IP Implementation Guide V1.0b"
**
**  AUTHORS: Ruslan R. Laishev (RRL)
**
**  CREATION DATE:  26-NOV-2023
**
**  MODIFICATION HISTORY:
**
**      19-JAN-2024     RRL     Added ADU\MBAP definitions.
**
**      29-JAN-2024     RRL     Replaced fucking "__attribute__((aligned(1))) " by "pack (push, 1)"
**
**--
*/

#ifndef __MODBUSoT_H__
#define __MODBUSoT_H__

#include      <endian.h>
#include      <linux/types.h>



#define MODBUS$SZ_PAYLOAD       253                             /* Max size of the payload part of the MODBUS's packet */
#define MODBUS$SZ_MAXPDU        256                             /* Max size of the MODBUS's PDU */
#define MODBUS$K_STARTADDR_MIN  0
#define MODBUS$K_STARTADDR_MAX  0xFFFF
#define MODBUS$K_MAXWORDS       96                              /* 250 octets */
#define MODBUS$K_MAXBITS        2000                            /* 250 octets */
#define MODBUS$K_MAXOCTETS      125                             /* 250 octets */

#define	MODBUS$SZ_MINMBAP	(2+2+2+1)			/* A minimal size of MODBUS Application Protocol */
#define	MODBUS$SZ_MINADU	(MODBUS$SZ_MINMBAP)

#define	MODBUS$SZ_MINRTU	(1+1+1+2)			/* A minimal size of RTU response */
								/* Slave Address + Function + Byte Count + CRC16 */





#pragma pack (push, 1)

typedef struct modbus_pdu_t {
	unsigned char   slave;
	unsigned char   fncode;
	char            data[0];
} MODBUS_PDU_T;

typedef struct modbus_req_t {
	unsigned char   slave;
	unsigned char   fncode;
	unsigned short  sa;
	unsigned short  qty;
	char            data[0];
} MODBUS_REQ_T;


typedef struct modbus_resp_t {
	unsigned char   slave;
	unsigned char   fncode;
	unsigned short  val[0];
} MODBUS_RESP_T;


typedef struct modbus_exc_t {
	unsigned char   slave;
	unsigned char   fncode;
	unsigned char   excode;
} MODBUS_EXC_T;

typedef struct modbus_adu_t {				/* Application Data Unit */
	//struct {                                              /* MBAP - MODBUS Application Protocol header */
	__be16          txid;                                   /* Transaction Identifier */
	__be16          proto;                                  /* Protocol --//-- */
	__be16          len;                                    /* The length field is a byte count of the following fields, including the Unit Identifier and data fields. */
	__u8            unit[1];				/* Identification of a remote slave connected on a serial line or on other buses. */
	char            data[0];				/* Payload placeholder */
} MODBUS_ADU_T;
#pragma pack (pop)

#define	MODBUS$SZ_MBAPHDR	(2+2+2)				/* Size of MBAP Header: txid + proto + len */
#define	MODBUS$SZ_PDUHDR	(2+2)				/* Size of PDU Header: slave + fncode */


typedef enum {
	  MODBUS$K_EVT_NEW_CONNECTION,
	  MODBUS$K_EVT_CONNECTION_END,
	  MODBUS$K_EVT_MESSAGE_START,
	  MODBUS$K_EVT_MESSAGE_END,
	  MODBUS$K_EVT_READ_COILS_REQUEST,
	  MODBUS$K_EVT_READ_COILS_RESPONSE,
	  MODBUS$K_EVT_READ_DISCRETE_INPUTS_REQUEST,
	  MODBUS$K_EVT_READ_DISCRETE_INPUTS_RESPONSE,
	  MODBUS$K_EVT_READ_HOLDING_REGISTERS_REQUEST,
	  MODBUS$K_EVT_READ_HOLDING_REGISTERS_RESPONSE,
	  MODBUS$K_EVT_READ_INPUT_REGISTERS_REQUEST,
	  MODBUS$K_EVT_READ_INPUT_REGISTERS_RESPONSE,
	  MODBUS$K_EVT_WRITE_SINGLE_COIL_REQUEST,
	  MODBUS$K_EVT_WRITE_SINGLE_COIL_RESPONSE,
	  MODBUS$K_EVT_WRITE_SINGLE_REGISTER_REQUEST,
	  MODBUS$K_EVT_WRITE_SINGLE_REGISTER_RESPONSE,
	  MODBUS$K_EVT_WRITE_MULTIPLE_COILS_REQUEST,
	  MODBUS$K_EVT_WRITE_MULTIPLE_COILS_RESPONSE,
	  MODBUS$K_EVT_WRITE_MULTIPLE_REGISTERS_REQUEST,
	  MODBUS$K_EVT_WRITE_MULTIPLE_REGISTERS_RESPONSE,
	  MODBUS$K_EVT_REPORT_SERVER_ID_REQUEST,
	  MODBUS$K_EVT_REPORT_SERVER_ID_RESPONSE,
	  MODBUS$K_EVT_READ_FILE_RECORD_REQUEST,
	  MODBUS$K_EVT_READ_FILE_RECORD_RESPONSE,
	  MODBUS$K_EVT_WRITE_FILE_RECORD_REQUEST,
	  MODBUS$K_EVT_WRITE_FILE_RECORD_RESPONSE,
	  MODBUS$K_EVT_MASK_WRITE_REGISTER_REQUEST,
	  MODBUS$K_EVT_MASK_WRITE_REGISTER_RESPONSE,
	  MODBUS$K_EVT_READ_WRITE_MULTIPLE_REGISTERS_REQUEST,
	  MODBUS$K_EVT_READ_WRITE_MULTIPLE_REGISTERS_RESPONSE,
	  MODBUS$K_EVT_READ_FIFO_QUEUE_REQUEST,
	  MODBUS$K_EVT_READ_FIFO_QUEUE_RESPONSE,
	  MODBUS$K_EVT_EXCEPTION_RESPONSE,
	  MODBUS$K_EVT_USER_DEFINED_FUNCTION,
	  MODBUS$K_EVT_INVALID,


	  MODBUS$K_EVT_MAX                                      /* End-of-list marker */
 } MODBUS$_EVENT;




#define MODBUS$M_FN_EXCEPTION   0x80                            /* exception-function_code = [1 byte] MODBUS function code + 0x80 */

 typedef enum {
	MODBUS$K_FN_READ_COILS = 0x01,
	MODBUS$K_FN_READ_DISCRETE_INPUTS = 0x02,
	MODBUS$K_FN_READ_HOLDING_REGISTERS = 0x03,
	MODBUS$K_FN_READ_INPUT_REGISTERS = 0x04,
	MODBUS$K_FN_WRITE_SINGLE_COIL = 0x05,
	MODBUS$K_FN_WRITE_SINGLE_REGISTER = 0x06,
	MODBUS$K_FN_WRITE_MULTIPLE_COILS = 0x0F,
	MODBUS$K_FN_WRITE_MULTIPLE_REGISTERS = 0x10,
	MODBUS$K_FN_REPORT_SERVER_ID = 0x11,
	MODBUS$K_FN_READ_FILE_RECORD = 0x14,
	MODBUS$K_FN_WRITE_FILE_RECORD = 0x15,
	MODBUS$K_FN_MASK_WRITE_REGISTER = 0x16,
	MODBUS$K_FN_READ_WRITE_MULTIPLE_REGISTERS = 0x17,
	MODBUS$K_FN_READ_FIFO_QUEUE = 0x18,


	MODBUS$K_FN_MAX                                         /* End-of-list marker */
 } MODBUS$_FN;


 typedef enum {
	MODBUS$K_EXCEPTION_NOERROR = 0,
	MODBUS$K_EXCEPTION_ILLEGAL_FUNCTION = 1,
	MODBUS$K_EXCEPTION_ILLEGAL_DATA_ADDRESS = 2,
	MODBUS$K_EXCEPTION_ILLEGAL_DATA_VALUE = 3,
	MODBUS$K_EXCEPTION_SERVER_DEVICE_FAILURE = 4,
	MODBUS$K_EXCEPTION_ACKNOWLEDGE = 5,
	MODBUS$K_EXCEPTION_SERVER_DEVICE_BUSY = 6,
	MODBUS$K_EXCEPTION_NEGATIVE_ACKNOWLEDGE = 7,
	MODBUS$K_EXCEPTION_MEMORY_PARITY_ERROR = 8,
	MODBUS$K_EXCEPTION_GW_NO_PATH = 10,
	MODBUS$K_EXCEPTION_TGT_DEV_NOANSWRD = 11,

	MODBUS$K_EXCEPTION_MAX                                  /* End-of-list marker */
} MODBUS$_EXC;



/*
 * Calculate Modbus frame CRC (Cyclical Redundancy Checking) value
 * Parameters: FRAME - address of the frame,
 *             LEN   - frame length;
 * Return: calculated CRC value
 */
static inline unsigned short s_modbus_crc_calculate(
		const unsigned char	*a_frame,
			unsigned int	a_len
			)
{
unsigned short l_crc = 0xffff;
extern	const unsigned short g_modbus_crc16_table[];

	if ( a_len > MODBUS$SZ_MAXPDU )
		return	0;

	while (a_len--)
		l_crc = (unsigned short)(l_crc >> 8) ^ g_modbus_crc16_table[(l_crc ^ *a_frame++) & 0xff];

	return l_crc;
}

/*
 * Check CRC of MODBUS frame
 * Parameters: FRAME - address of the frame,
 *             LEN   - frame length;
 * Return: 0 if CRC failed,
 *         non-zero otherwise
 */
static inline int s_modbus_crc_correct (
		unsigned char *a_frame,
		unsigned int a_len)
{
	return (!s_modbus_crc_calculate(a_frame, a_len));
}



int t2r$mbap_2_rtu_pdu (const void *a_src_dsc, void *a_dst_dsc);
int t2r$rtu_pdu_2_mbap (const void *a_src_dsc, void *a_dst_dsc);


#endif          /* __MODBUSoT_H__ */
