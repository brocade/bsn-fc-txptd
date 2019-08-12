/*
 * Copyright 2019 Broadcom. All rights reserved.
 * The term “Broadcom” refers to Broadcom Inc. and/or its subsidiaries.
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see https://www.gnu.org/licenses/gpl-2.0.en.html.
 *
 * Authors:
 *      Ganesh Pai <ganesh.pai@broadcom.com>
 *      Muneendra Kumar <muneendra.kumar@broadcom.com>
 */

#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <pthread.h>
#include <errno.h>
#include "fpin.h"
#include <linux/sockios.h>
#include <linux/if.h>
#include <linux/if_arp.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <linux/ethtool.h>
#include <linux/if_vlan.h>


struct list_head els_marginal_list_head;
struct list_head fpin_li_marginal_list_head;
static int fcm_fc_socket;
#define DEF_RX_BUF_SIZE		4096

/* 
 * Listen for ELS frames from driver. on receiving the frame payload,
 * push the payload to a list, and notify the fpin_els_li_consumer thread to
 * process it. Once consumer thread is notified, return to listen for more ELS
 * frames from driver.
 */
void fpin_fabric_notification_receiver()
{
	int ret = -1; 
	int maxfd = 0;
	fd_set readfs;
	fpin_payload_t *fpin_payload = NULL;
	int fpin_payload_sz = sizeof(fpin_payload_t) + FC_PAYLOAD_MAXLEN;
	int fd, rc;
	struct fc_nl_event *fc_event = NULL;
	struct sockaddr_nl fc_local;
	unsigned char buf[DEF_RX_BUF_SIZE];
	size_t plen = 0;
	int offset =0;
	uint32_t els_cmd = 0;

	fd = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_SCSITRANSPORT);
	if (fd < 0) {
		FPIN_ELOG("fc socket error %d", fd);
		exit(EX_UNAVAILABLE);
	}
	memset(&fc_local, 0, sizeof(fc_local));
	fc_local.nl_family = AF_NETLINK;
	fc_local.nl_groups = ~0;
	fc_local.nl_pid = getpid();
	rc = bind(fd, (struct sockaddr *)&fc_local, sizeof(fc_local));
	if (rc == -1) {
		FPIN_ELOG("fc socket bind error %d\n", rc);
		close(fd);
		exit(EX_NOINPUT);
	}

	fpin_payload = calloc(1, fpin_payload_sz);
	if (fpin_payload == NULL) {
		FPIN_CLOG(" No Mem to alloc\n");
		exit(EX_IOERR);
	}

	for ( ; ; ) {
		FPIN_ILOG("Waiting for ELS...\n");
		ret = read(fd, buf, DEF_RX_BUF_SIZE);
		FPIN_DLOG("Got a new request\n");

		/* Push the frame to appropriate frame list */
		plen = NLMSG_PAYLOAD((struct nlmsghdr *)buf, 0);
		fc_event = (struct fc_nl_event *)NLMSG_DATA(buf);
		if (plen < sizeof(*fc_event)) {
			FPIN_ELOG("too short (%zu) to be an FC event", ret);
			continue;
		}

		memcpy(fpin_payload->payload, &(fc_event->event_data), fpin_payload_sz);
		els_cmd = *(uint32_t *)fpin_payload->payload;
		FPIN_ILOG("Got host no as %d, event 0x%x, len %d\n",
				fc_event->host_no, els_cmd, fc_event->event_datalen);
		fpin_payload->host_num = fc_event->host_no;
		fpin_payload->length = fc_event->event_datalen;
		fpin_handle_els_frame(fpin_payload);
	}

}

/*
 * FPIN daemon main(). Sleeps on read until an FPIn ELS frame is recieved from
 * HBA driver.
 */
int
main(int argc, char *argv[])
{

	int ret = -1;
	pthread_t fpin_consumer_thread_id;

	setlogmask (LOG_UPTO (LOG_INFO));
	openlog("FCTXPTD", LOG_PID, LOG_USER);
	INIT_LIST_HEAD(&els_marginal_list_head);
	INIT_LIST_HEAD(&fpin_li_marginal_list_head);

	/*
	 *	A thread to process notifications from FC fabric.
	 */
	ret = pthread_create(&fpin_consumer_thread_id, NULL,
				fpin_els_li_consumer, NULL);
	if (ret != 0) {
		FPIN_CLOG("pthread_create failed for receiver thread, err %d, %s\n",
				ret, strerror(errno));
		exit (ret);
	}

	/*
	 * Non returning function, waits on netlink socket to recieve FPIN frames 
	 * from HBA. This function returning back implies there is some error in
	 * recieving frames from HBA. So there is no need to keep the above threads
	 * alive.
	 */

	fpin_fabric_notification_receiver();
	exit (0);
}
