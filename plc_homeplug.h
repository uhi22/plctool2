/* basiert auf https://github.com/qca/open-plc-utils/blob/master/mme/mme.h etc */


#ifndef PLC_HOMEPLUG_HEADER
#define PLC_HOMEPLUG_HEADER

/*====================================================================*
 *   system header files;
 *--------------------------------------------------------------------*/

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>

/*====================================================================*
 *   manage cross-platform structure packing;
 *--------------------------------------------------------------------*/

#ifndef __packed
#ifdef __GNUC__
#define __packed __attribute__ ((packed))
#else
#define __packed
#endif
#endif



/*====================================================================*
 *   custom header files;
 *--------------------------------------------------------------------*/

#include "endian.h"


/* MAC configuration */
#if 0
 #define MY_DEST_MAC0	0xFF
 #define MY_DEST_MAC1	0xFF
 #define MY_DEST_MAC2	0xFF
 #define MY_DEST_MAC3	0xFF
 #define MY_DEST_MAC4	0xFF
 #define MY_DEST_MAC5	0xFF
#else
 /* Devolo umgebaut für unterwegs */
 #define MY_DEST_MAC0	0xbc
 #define MY_DEST_MAC1	0xf2
 #define MY_DEST_MAC2	0xaf
 #define MY_DEST_MAC3	0x0b
 #define MY_DEST_MAC4	0x8e
 #define MY_DEST_MAC5	0x26
#endif

/* From slac.h */
/*
 * The following two constants control whether or not the PEV or EVSE
 * change AVLN on SLAC protocol cycle; The recommended setting is PEV
 * changes with each pass and the EVSE does not;
 */

#define SLAC_AVLN_PEV 1
#define SLAC_AVLN_EVSE 0

#define SLAC_APPLICATION_PEV_EVSE 0x00

#define SLAC_SECURITY_NONE 0x00
#define SLAC_SECURITY_PUBLIC_KEY 0x01

#define SLAC_RUNID_LEN 8
#define SLAC_UNIQUE_ID_LEN 17
#define SLAC_RND_LEN 16
#define SLAC_NID_LEN 7
#define SLAC_NMK_LEN 16

#define SLAC_MSOUNDS 10
#define SLAC_TIMETOSOUND 6
#define SLAC_TIMEOUT 1000
#define SLAC_APPLICATION_TYPE 0
#define SLAC_SECURITY_TYPE 0
#define SLAC_RESPONSE_TYPE 0
#define SLAC_MSOUND_TARGET "FF:FF:FF:FF:FF:FF"
#define SLAC_FORWARD_STATION "00:00:00:00:00:00"
#define SLAC_GROUPS 58

#define SLAC_LIMIT 40
#define SLAC_PAUSE 20
#define SLAC_SETTLETIME 10
#define SLAC_CHARGETIME 2
#define SLAC_FLAGS 0

#define SLAC_SILENCE   (1 << 0)
#define SLAC_VERBOSE   (1 << 1)
#define SLAC_SESSION   (1 << 2)
#define SLAC_COMPARE   (1 << 3)
#define SLAC_SOUNDONLY (1 << 4)

#define SLAC_CM_SETKEY_KEYTYPE 0x01
#define SLAC_CM_SETKEY_PID 0x04
#define SLAC_CM_SETKEY_PRN 0x00
#define SLAC_CM_SETKEY_PMN 0x00
#define SLAC_CM_SETKEY_CCO 0x00
#define SLAC_CM_SETKEY_EKS 0x01


/*====================================================================*
 *   HomePlug AV Constants;
 *--------------------------------------------------------------------*/

#define ETH_P_HPAV 0x88E1 /* Protocol identifier on Ethernet Frame */

#define HOMEPLUG_MMV 0x01 /* Version */
#define HOMEPLUG_MMTYPE 0x0000

/*====================================================================*
 * HomePlug Management Message Ranges for Information Only;
 *--------------------------------------------------------------------*/

#define CC_MMTYPE_MIN 0x0000
#define CC_MMTYPE_MAX 0x1FFF
#define CP_MMTYPE_MIN 0x2000
#define CP_MMTYPE_MAX 0x3FFF
#define NN_MMTYPE_MIN 0x4000
#define NN_MMTYPE_MAX 0x5FFF
#define CM_MMTYPE_MIN 0x6000
#define CM_MMTYPE_MAX 0x7FFF
#define MS_MMTYPE_MIN 0x8000
#define MS_MMTYPE_MAX 0x9FFF
#define VS_MMTYPE_MIN 0xA000
#define VS_MMTYPE_MAX 0xBFFF
#define HA_MMTYPE_MIN 0xC000
#define HA_MMTYPE_MAX 0xFFFF

/*====================================================================*
 * HomePlug AV MMEs have 4 variants indicated by the 2 MMTYPE LSBs;
 *--------------------------------------------------------------------*/

#define MMTYPE_CC 0x0000
#define MMTYPE_CP 0x2000
#define MMTYPE_NN 0x4000
#define MMTYPE_CM 0x6000
#define MMTYPE_MS 0x8000
#define MMTYPE_VS 0xA000
#define MMTYPE_XX 0xC000

#ifndef IHPAPI_HEADER
#define MMTYPE_REQ 0x0000
#define MMTYPE_CNF 0x0001
#define MMTYPE_IND 0x0002
#define MMTYPE_RSP 0x0003
#define MMTYPE_MODE  (MMTYPE_REQ|MMTYPE_CNF|MMTYPE_IND|MMTYPE_RSP)
#define MMTYPE_MASK ~(MMTYPE_REQ|MMTYPE_CNF|MMTYPE_IND|MMTYPE_RSP)
#endif

/*====================================================================*
 * HomePlug AV Management Message Types;
 *--------------------------------------------------------------------*/

#define CC_CCO_APPOINT 0x0000
#define CC_BACKUP_APPOINT 0x0004
#define CC_LINK_INFO 0x0008
#define CC_HANDOVER 0x000C
#define CC_HANDOVER_INFO 0x0010
#define CC_DISCOVER_LIST 0x0014
#define CC_LINK_NEW 0x0018
#define CC_LINK_MOD 0x001C
#define CC_LINK_SQZ 0x0020
#define CC_LINK_REL 0x0024
#define CC_DETECT_REPORT 0x0028
#define CC_WHO_RU 0x002C
#define CC_ASSOC 0x0030
#define CC_LEAVE 0x0034
#define CC_SET_TEI_MAP 0x0038
#define CC_RELAY 0x003C
#define CC_BEACON_RELIABILITY 0x0040
#define CC_ALLOC_MOVE 0x0044
#define CC_ACCESS_NEW 0x0048
#define CC_ACCESS_REL 0x004C
#define CC_DCPPC 0x0050
#define CC_HP1_DET 0x0054
#define CC_BLE_UPDATE 0x0058
#define CP_PROXY_APPOINT 0x2000
#define PH_PROXY_APPOINT 0x2004
#define CP_PROXY_WAKE 0x2008
#define NN_INL 0x4000
#define NN_NEW_NET 0x4004
#define NN_ADD_ALLOC 0x4008
#define NN_REL_ALLOC 0x400C
#define NN_REL_NET 0x4010
#define CM_ASSOCIATED_STA 0x6000
#define CM_ENCRYPTED_PAYLOAD 0x6004
#define CM_SET_KEY 0x6008
#define CM_GET_KEY 0x600C
#define CM_SC_JOIN 0x6010
#define CM_CHAN_EST 0x6014
#define CM_TM_UPDATE 0x6018
#define CM_AMP_MAP 0x601C
#define CM_BRG_INFO 0x6020
#define CM_CONN_NEW 0x6024
#define CM_CONN_REL 0x6028
#define CM_CONN_MOD 0x602C
#define CM_CONN_INFO 0x6030
#define CM_STA_CAP 0x6034
#define CM_NW_INFO 0x6038
#define CM_GET_BEACON 0x603C
#define CM_HFID 0x6040
#define CM_MME_ERROR 0x6044
#define CM_NW_STATS 0x6048
#define CM_SLAC_PARAM 0x6064
#define CM_START_ATTEN_CHAR 0x6068
#define CM_ATTEN_CHAR 0x606C
#define CM_PKCS_CERT 0x6070
#define CM_MNBC_SOUND 0x6074
#define CM_VALIDATE 0x6078
#define CM_SLAC_MATCH 0x607C
#define CM_SLAC_USER_DATA 0x6080
#define CM_ATTEN_PROFILE 0x6084

#define CM_GET_DEVICE_SW_VERSION 0xa000 /* seems to be not official, the fritzbox uses this */



/*====================================================================*
 *
 *--------------------------------------------------------------------*/



/*====================================================================*
 *   Ethernet, HomePlug and Qualcomm Frame headers;
 *--------------------------------------------------------------------*/

#ifndef __GNUC__
#pragma pack (push, 1)
#endif

/*
typedef struct __packed ethernet_hdr

{
	uint8_t ODA [ETHER_ADDR_LEN];
	uint8_t OSA [ETHER_ADDR_LEN];
	uint16_t MTYPE;
}

ethernet_hdr;
*/
 
typedef struct __packed homeplug_hdr

{
	uint8_t MMV;
	uint16_t MMTYPE;
}
homeplug_hdr;

typedef struct __packed homeplug_fmi

{
	uint8_t MMV;
	uint16_t MMTYPE;
	uint8_t FMSN;
	uint8_t FMID;
}
homeplug_fmi;

#ifndef __GNUC__
#pragma pack (pop)
#endif

/*====================================================================*
 *   Composite message formats;
 *--------------------------------------------------------------------*/

#ifndef __GNUC__
#pragma pack (push, 1)
#endif

/*
typedef struct __packed message

{
	struct ethernet_hdr ethernet;
	uint8_t content [ETHERMTU];
}
MESSAGE;
*/

/*
typedef struct __packed homeplug

{
	struct ethernet_hdr ethernet;
	struct homeplug_fmi homeplug;
	uint8_t content [ETHERMTU - sizeof (struct homeplug_fmi)];
}
HOMEPLUG;
*/


typedef struct __packed cm_set_key_request
	{
		struct ethhdr ethernet;
		struct homeplug_fmi homeplug;
		uint8_t KEYTYPE;
		uint32_t MYNOUNCE;
		uint32_t YOURNOUNCE;
		uint8_t PID;
		uint16_t PRN;
		uint8_t PMN;
		uint8_t CCOCAP;
		uint8_t NID [SLAC_NID_LEN];
		uint8_t NEWEKS;
		uint8_t NEWKEY [SLAC_NMK_LEN];
		/*uint8_t RSVD [3];*/ /* Todo: warum 3 reseved hier? Ok, padding, aber
		                     das fuhrt dazu, dass beim CM_SET_KEY am Ende
		                     drei 0er angehängt werden, was nicht ISO-konform
		                     ist. */
	}
	cm_set_key_request;

typedef struct __packed cm_set_key_confirm
	{
		struct ethhdr ethernet;
		struct homeplug_fmi homeplug;
		uint8_t RESULT;
		uint32_t MYNOUNCE;
		uint32_t YOURNOUNCE;
		uint8_t PID;
		uint16_t PRN;
		uint8_t PMN;
		uint8_t CCOCAP;
		uint8_t RSVD [27];
	}
	cm_set_key_confirm;


/* according to HomeplugAV2.1 spec table 11-89 */
#define HOMEPLUG_KEYTYPE_NMK 1 /* AES-128 */

typedef struct __packed cm_get_key_request
	{   /* structure according to homeplugAV2.1 specification */
		struct ethhdr ethernet;
		struct homeplug_fmi homeplug;
		uint8_t RequestType; /* 0=direct */
		uint8_t RequestedKeyType; /* only "NMK" is permitted over the H1 interface */
		uint8_t NID [SLAC_NID_LEN];
		uint32_t MYNOUNCE;
		uint8_t PID;
		uint16_t PRN;
		uint8_t PMN;
		/*uint8_t HASHKEY [...];*/ /* variable length, only applicable for hash keys */
	}
	cm_get_key_request;

typedef struct __packed cm_get_key_confirm
	{   /* structure according to homeplugAV2.1 specification */
		struct ethhdr ethernet;
		struct homeplug_fmi homeplug;
		uint8_t RESULT;
		uint8_t RequestedKeyType;
		uint32_t MYNOUNCE;
		uint32_t YOURNOUNCE;
		uint8_t NID [SLAC_NID_LEN];
		uint8_t EKS;
		uint8_t PID;
		uint16_t PRN;
		uint8_t PMN;
		uint8_t KEY [16]; /* variable length */
	}
	cm_get_key_confirm;


#ifndef __GNUC__
#pragma pack (push, 1)
#endif

typedef struct __packed cm_slac_match_confirm
{
	struct ethhdr ethernet;
	struct homeplug_fmi homeplug;
	uint8_t APPLICATION_TYPE;
	uint8_t SECURITY_TYPE;
	uint16_t MVFLength;
	struct __packed
	{
		uint8_t PEV_ID [SLAC_UNIQUE_ID_LEN];
		uint8_t PEV_MAC [ETHER_ADDR_LEN];
		uint8_t EVSE_ID [SLAC_UNIQUE_ID_LEN];
		uint8_t EVSE_MAC [ETHER_ADDR_LEN];
		uint8_t RunID [SLAC_RUNID_LEN];
		uint8_t RSVD1 [8];
		uint8_t NID [SLAC_NID_LEN];
		uint8_t RSVD2;
		uint8_t NMK [SLAC_NMK_LEN];
	}
	MatchVarField;
}
cm_slac_match_confirm;

#ifndef __GNUC__
#pragma pack (pop)
#endif


#ifndef __GNUC__
#pragma pack (pop)
#endif

/*====================================================================*
 *   functions;
 *--------------------------------------------------------------------*/


/*====================================================================*
 *   functions;
 *--------------------------------------------------------------------*/


/*====================================================================*
 *   header encode functions;
 *--------------------------------------------------------------------*/

signed EthernetHeader (void * memory, const uint8_t peer [], const uint8_t host [], uint16_t protocol);
signed HomePlugHeader (struct homeplug_hdr *, uint8_t MMV, uint16_t MMTYPE);
signed HomePlugHeader1 (struct homeplug_fmi *, uint8_t MMV, uint16_t MMTYPE);


/*====================================================================*
 *
 *--------------------------------------------------------------------*/



#endif

