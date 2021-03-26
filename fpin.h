#ifndef __FPIN_H__
#define __FPIN_H__

#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <malloc.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <asm/types.h>
#include <linux/netlink.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <poll.h>
#include <limits.h>
#include <dirent.h>
#include <libudev.h>
#include <libdevmapper.h>
#include <mpath_cmd.h>
#include <sysexits.h>
#include <syslog.h>
#include <fcntl.h>
#include <unistd.h>
#include <scsi/scsi_netlink.h>
#include <scsi/scsi_netlink_fc.h>
#include "fpin_els.h"

#ifdef FPIN_DEBUG
#define FPIN_DLOG(fmt...) syslog(LOG_DEBUG, fmt);
#define FPIN_ILOG(fmt...) syslog(LOG_INFO, fmt);
#define FPIN_ELOG(fmt...) syslog(LOG_ERR, fmt);
#define FPIN_CLOG(fmt...) syslog(LOG_CRIT, fmt);
#else
#define FPIN_DLOG(fmt...)
#define FPIN_ILOG(fmt...)
#define FPIN_ELOG(fmt...)
#define FPIN_CLOG(fmt...)
#endif



/* Linked List to store sd and dm mapping */
#define DM_PARAMS_SIZE	4096
#define CMD_LEN			192
#define DEV_NAME_LEN	128
#define TGT_NAME_LEN	64
#define DEV_NODE_LEN	32
#define WWN_LEN			32
#define HOST_NAME_LEN	32
#define PORT_ID_LEN		8
#define UUID_LEN		128
#define FILE_PATH_LEN	576		// SYS_PATH_LEN + Filename
#define DEV_STATUS_LEN	64
#define FCH_EVT_LINKUP 0x2
#define FCH_EVT_LINK_FPIN 0x501
#define FCH_EVT_RSCN 0x5

struct impacted_devs
{
	char dev_node[DEV_NAME_LEN];
	char dev_name[DEV_NAME_LEN];
	char dev_serial_id[UUID_LEN];
	char p_wwn[WWN_LEN];
	struct list_head dev_list_head;
};

struct dm_devs
{
	char dm_name[DEV_NAME_LEN];
	char dm_uuid[UUID_LEN];
	struct list_head dm_list_head;
};

struct targets
{
	char target[TGT_NAME_LEN];
	char p_wwn[WWN_LEN];
	struct list_head target_head;
};

/* Structure to store WWNs of HBA port and affected PWWNs */
struct impacted_port_wwns
{
	char impacted_port_wwn[WWN_LEN];
	struct list_head impacted_port_wwn_head;
};

/* For 1 hba_wwn, we will have a list of impacted
 * port WWNs.
 */
struct wwn_list
{
	uint32_t host_num;
	struct list_head impacted_ports_wwn_head;
};
/* Structure to store the marginal devices info */
struct marginal_dev_list
{
	char dev_name[DEV_NAME_LEN];
	uint32_t host_num;
	struct list_head marginal_dev_list_head;
};

/* ELS frame Handling functions */
int fpin_fetch_dm_lun_data(struct wwn_list *list,
			struct list_head *dm_list_head,
			struct list_head *impacted_dev_list_head, struct udev *udev);
void fpin_dm_marginal_path(uint32_t host_num, struct list_head *dm_list_head,
				struct list_head *impacted_dev_list_head);

int fpin_populate_dm_lun(struct list_head *dm_list_head,
			struct list_head *impacted_dev_list_head,
			struct udev *udev, struct list_head *target_head);

/* Target Related Functions */
int fpin_dm_insert_target(struct list_head *tgt_head, const char *target, const char *port_wwn);
int fpin_dm_find_target(struct list_head *tgt_head, const char *target, char *port_wwn);
void fpin_dm_display_target(struct list_head *tgt_head);
void fpin_dm_free_target(struct list_head *tgt_head);
int fpin_dm_populate_target(struct wwn_list *list,
		 struct list_head *tgt_list, struct udev *udev);
void fpin_dm_free_dev(struct  list_head *sd_head);
void fpin_free_dm(struct list_head *dm_head);

/* WWN Related Functions */
int fpin_els_wwn_exists(struct wwn_list *list, const char *port_wwn_buf);
void fpin_els_free_wwn_list(struct wwn_list *list);
void fpin_unset_marginal_dev(uint32_t host_num, struct list_head *tgt_head);

extern struct list_head fpin_li_marginal_dev_list_head;
#endif
