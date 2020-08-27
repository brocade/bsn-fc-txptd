#ifndef __FPIN_ELS_H__
#define __FPIN_ELS_H__

#include <pthread.h>
#include <endian.h>
#include <arpa/inet.h>
#include <time.h>

#include "list.h"

/* max ELS frame Size */
#define FC_PAYLOAD_MAXLEN   2048
#define SYS_PATH_LEN		512

#define ELS_CMD_FPIN 0x16

#define MARGINAL_CHECKER_WAIT_TIME 30

/*
 * This data is read from FC frame, which has a mixture of
 * both 32 and 64 bit data. Using wwn_t as 64-bit is causing
 * an offset of 32 bits when used with be64toh. Hence, using
 * wwn_t as two 32-bit words and using ntohl instead.
 */
typedef struct wwn {
	uint32_t	words[2];
} wwn_t;

struct els_marginal_list {
	uint16_t host_num;
	uint16_t length;
	char payload[FC_PAYLOAD_MAXLEN];
	struct list_head els_frame;
};


/* --- FPIN --- */

/* Notification Descriptor Tags */
typedef enum fpin_notification_descriptor_tag {
	eFPIN_NOTIFICATION_DESCRIPTOR_UNASSIGNED				= 0x00000000,
	eFPIN_NOTIFICATION_DESCRIPTOR_REGISTER_TAG				= 0x00020000,
	eFPIN_NOTIFICATION_DESCRIPTOR_LINK_INTEGRITY_TAG		= 0x00020001,
	eFPIN_NOTIFICATION_DESCRIPTOR_DELIVERY_TAG				= 0x00020002,
	eFPIN_NOTIFICATION_DESCRIPTOR_CONGESTION_TAG			= 0x00020003,
	eFPIN_NOTIFICATION_DESCRIPTOR_TRANS_DELAY_TAG			= 0x00020004
} fpin_notification_descriptor_tag_e;

/* Congestion Notification Event Types */
typedef enum fpin_congestion_notification_event_type {
	eFPIN_CONGESTION_NOTIFICATION_EVENT_TYPE_NONE			= 0x00,
	eFPIN_CONGESTION_NOTIFICATION_EVENT_TYPE_LOST_CREDIT	= 0x01,
	eFPIN_CONGESTION_NOTIFICATION_EVENT_TYPE_CREDIT_STALL	= 0x02,
	eFPIN_CONGESTION_NOTIFICATION_EVENT_TYPE_OVERSUBSCRIPTION = 0x03
} fpin_congestion_notification_event_type_e;

/* Link Integrity Notification Event Types (16bit so no enum) */
#define FPIN_LINK_INTEGRITY_EVENT_TYPE_UNKNOWN			0x0000
#define FPIN_LINK_INTEGRITY_EVENT_TYPE_LINK_FAILURE		0x0001
#define FPIN_LINK_INTEGRITY_EVENT_TYPE_LOSS_OF_SYNC		0x0002
#define FPIN_LINK_INTEGRITY_EVENT_TYPE_LOSS_OF_SIGNAL	0x0003
#define FPIN_LINK_INTEGRITY_EVENT_TYPE_PRIMITIVE_ERROR	0x0004
#define FPIN_LINK_INTEGRITY_EVENT_TYPE_ITW				0x0005
#define FPIN_LINK_INTEGRITY_EVENT_TYPE_CRC				0x0006
#define FPIN_LINK_INTEGRITY_EVENT_TYPE_DEV_SPECIFIC		0x0007

/*
 * Generic FPIN Descriptor
 * -----------------------
 * This structure is used in cases where the Descriptor details are not
 * immediately known. Thus, only the Tag and Length fields are defined
 * so that the Descriptor parser may examine and categorize via said fields.
 *
 * Copied the definition as-is from the sender side. (i.e. FOS switch)
 */
typedef struct fpin_descriptor_header {
	fpin_notification_descriptor_tag_e		tag;			/* Unspecified tag; may be unknown */
	uint32_t								length;			/* Length in bytes */
} fpin_descriptor_header_t;

/* Notification Port List */
typedef struct fpin_notification_port_list {
	uint32_t            count;								/* Number of port names in 'port_name_list' */
	wwn_t               port_name_list[0];					/* List of N_Port Names (Port WWN) */
} fpin_notification_port_list_t;

typedef struct fpin_link_integrity_notification {
	fpin_descriptor_header_t			header;
	wwn_t                               detecting_port_wwn;	/* Detecting F/N_Port Name (Port WWN) */
	wwn_t                               attached_port_wwn;	/* Attached F/N_Port Name (Port WWN) */
	uint16_t							event_type;			/* FPIN_LINK_INTEGRITY_EVENT_TYPE_* */
	uint16_t							event_modifier;		/* Set to 0 normally */
	uint32_t							event_threshold;	/* threshold time */
	uint32_t							event_count;		/* threshold count */
	fpin_notification_port_list_t       port_list;			/* Event data (Port List) */
} fpin_link_integrity_notification_t;

/* FPIN ELS Header */
typedef struct fpin_els_header {
	uint32_t	cmd;			/* ELS Command Code */
	uint32_t	length;			/* Frame Length */
} fpin_els_header_t;

typedef struct fpin_link_integrity_request_els {
	fpin_els_header_t					els_header;
	fpin_link_integrity_notification_t	linkIntegrityDesc;	/* Link Integrity Descriptor */
} fpin_link_integrity_request_els_t;

/* FPIN Payload received from HBA driver */
typedef struct fpin_payload {
	uint16_t host_num;
	uint16_t length; //2048 for now
	char payload[0];
} fpin_payload_t;

/* FPIN ELS Handler functions */
void *fpin_els_li_consumer();
void *fpin_li_marginal_checker();
int fpin_handle_els_frame(fpin_payload_t *fpin_payload);
#endif
