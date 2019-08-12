/*
 * Copyright 2019 Broadcom. All rights reserved.
 * The term “Broadcom” refers to Broadcom Inc. and/or its subsidiaries.
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
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

#include "fpin.h"


pthread_cond_t fpin_li_cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t fpin_li_mutex = PTHREAD_MUTEX_INITIALIZER;
extern struct list_head    els_marginal_list_head;

/*
 * Function:
 * 	fpin_els_add_li_frame
 *
 * Input:
 * 	The Link Integrity Frame payload received from HBA driver.
 *
 * Description:
 * 	On Receiving the frame from HBA driver, insert the frame into link
 * 	integrity frame list which will be picked up later by consumer thread for
 * 	processing.
 */
int
fpin_els_add_li_frame(fpin_payload_t *fpin_payload) {
	struct els_marginal_list *els_mrg = NULL;
	els_mrg = malloc(sizeof(struct els_marginal_list));
	if (els_mrg != NULL) {
		els_mrg->host_num = fpin_payload->host_num;
		memcpy(els_mrg->payload, fpin_payload->payload, FC_PAYLOAD_MAXLEN);
		pthread_mutex_lock(&fpin_li_mutex);
		list_add_tail(&els_mrg->els_frame, &els_marginal_list_head);
		pthread_mutex_unlock(&fpin_li_mutex);
		pthread_cond_signal(&fpin_li_cond);
	} else {
		FPIN_CLOG("NO Memory to add frame payload\n");
		return (-ENOMEM);
	}

	return (0);
}

/*
 * Function:
 * 	fpin_els_insert_port_wwn
 *
 * Input:
 * 	struct wwn_list *list	: List containing impacted WWNs.
 * 	port_wwn_buf			: The WWN to be inserted into above list.
 *
 * Description:
 * 	This function inserts the Port WWN retrieved from FPIN ELS frame, recieved
 * 	from HBA driver. These WWNs are later used to find sd* and dm-* details.
 */

int
fpin_els_insert_port_wwn(struct wwn_list *list, char *port_wwn_buf)
{
	struct impacted_port_wwns *new_wwn = NULL;
	FPIN_ILOG("Inserting %s...\n", port_wwn_buf);

	 /* Create a node */
	new_wwn = (struct impacted_port_wwns *)malloc(sizeof(struct impacted_port_wwns));
	if (new_wwn == NULL) {
		FPIN_CLOG("No memory to assign pwwn %s\n", port_wwn_buf);
		return -ENOMEM;
	}

	memset(new_wwn->impacted_port_wwn, '\0', sizeof(new_wwn->impacted_port_wwn));
	snprintf(new_wwn->impacted_port_wwn, sizeof(new_wwn->impacted_port_wwn),
				"%s", port_wwn_buf);
	FPIN_ILOG(" Assigned  %s to new node\n", new_wwn->impacted_port_wwn);
	list_add_tail(&(new_wwn->impacted_port_wwn_head),
		&(list->impacted_ports_wwn_head));

	return (0);
}

/*
 * Function:
 *	fpin_els_wwn_exists 	
 *
 * Input:
 * 	struct wwn_list *list	: List containing impacted WWNs.
 * 	port_wwn_buf			: The WWN to be searched in the above list.
 *
 * Description:
 * 	This function searches the impacted WWN list for the WWN passed in
 * 	second argument. Returns 1 if found, 0 otherwise.
 */

int
fpin_els_wwn_exists(struct wwn_list *list, const char *port_wwn_buf) {
	struct impacted_port_wwns *temp = NULL;

	if (list_empty(&(list->impacted_ports_wwn_head))) {
        	FPIN_ELOG("List is empty, %s not found\n", port_wwn_buf);
	} else {
		list_for_each_entry(temp, &(list->impacted_ports_wwn_head),
								impacted_port_wwn_head) {
			FPIN_DLOG("Checking for %s and %s\n", temp->impacted_port_wwn,
						port_wwn_buf);
			if (strncmp(temp->impacted_port_wwn, port_wwn_buf,
						strlen(port_wwn_buf)) == 0) {
				FPIN_ILOG("breaking for %s %s\n", temp->impacted_port_wwn,
						port_wwn_buf);
				return (1);
			}
		}
	}

	return (0);
}

void
fpin_els_display_wwn(struct wwn_list *list) {
	struct impacted_port_wwns *temp = NULL;

	if (list_empty(&(list->impacted_ports_wwn_head))) {
		FPIN_ELOG("WWN List is empty\n");
	} else {
		list_for_each_entry(temp, &(list->impacted_ports_wwn_head),
				impacted_port_wwn_head)
			FPIN_ILOG("WWN Imapcted is %s\n", temp->impacted_port_wwn);
	}

	FPIN_ILOG("Host num recvd is %d\n", list->host_num);
}

void
fpin_els_free_wwn_list(struct wwn_list *list) {
	struct list_head *current_node = NULL;
	struct list_head *temp = NULL;
	struct impacted_port_wwns *temp_node = NULL;

	if (list_empty(&(list->impacted_ports_wwn_head))) {
		FPIN_ELOG("WWN List is empty\n");
	} else {
		list_for_each_safe(current_node, temp,
			&(list->impacted_ports_wwn_head)) {
			temp_node = list_entry(current_node, struct impacted_port_wwns,
							impacted_port_wwn_head);
			FPIN_DLOG("Free WWN %s\n", temp_node->impacted_port_wwn);
			list_del(current_node);
			free(temp_node);
		}
	}
}


/*
 * Function:
 *	fpin_els_extract_wwn
 *
 * Input:
 *	host_num				: The Host# of HBA port, where the ELS was received.
 *	L.I Notification Struct	: The Link Integrity struct with impacted WWN list.
 * 	struct wwn_list *list	: The list to be populated with impacted WWN.
 *
 * Description:
 * 	This function reads though the FPIN ELS recieved from HBA driver, to get and
 * 	populate the impacted WWN list. This list is used to find and fail the
 * 	impacted paths if an alternate path for the same device exists.
 */

int
fpin_els_extract_wwn(uint16_t host_num, fpin_link_integrity_notification_t *li,
						struct wwn_list *list) {
	char  port_wwn_buf[WWN_LEN];
	wwn_t *currentPortListOffset_p = NULL;
	uint32_t wwn_count = 0;
	int iter = 0, count = 0;

	/* Update the wwn to list */
	wwn_count = ntohl(li->port_list.count);
	FPIN_DLOG("Got wwn count as %d\n", wwn_count);
	list->host_num = host_num;

	currentPortListOffset_p = (wwn_t *)&(li->port_list.port_name_list);
	for (iter = 0; iter < wwn_count; iter++) {
		memset(port_wwn_buf, '\0', WWN_LEN);
		/*
		 * This data is read from FC frame, which has a mixture of
		 * both 32 and 64 bit data. Using wwn_t as 64-bit is causing
		 * an offset of 32 bits when used with be64toh. Hence, using
		 * wwn_t as two 32-bit words and using ntohl instead.
		 */
		snprintf(port_wwn_buf, WWN_LEN, "0x%08x%08x", 
			ntohl(currentPortListOffset_p->words[0]),
			ntohl(currentPortListOffset_p->words[1]));
		if (fpin_els_insert_port_wwn(list, port_wwn_buf) < 0) {
			/* 
			 * No point in adding more as we are out of memory, return
			 * the count of devices already added.
			 */
			return (count);
		}

		currentPortListOffset_p++;
		count++;
	}

	fpin_els_display_wwn(list);
	return (count);
}

/*
 * Function:
 *	fpin_process_els_frame
 *
 * Inputs:
 * 	1. The WWN of the HBA on which the ELS frame was received.
 *	2. The ELS frame to be processed. Could be FPIN frame or any other ELS frame
 *	in the future.
 *
 * Description:
 *	This function process the ELS frame recieved from HBA driver,
 *	and fails the impacted paths if an alternate path exists. This function
 *	does the following:
 *		1. Extarct the impacted device WWNs from the FPIN ELS frame.
 *		2. Get the target IDs of the devices from the WWNs extracted.
 *		3. Translate the target IDs into corresponding sd* and dm-*.
 *		4. Fail the sd* using multipath daemon, provided alternate paths exist.
 *		5. Free the resources allocated.
 */
int
fpin_process_els_frame(uint16_t host_num, char *fc_payload) {
	struct list_head dm_list_head, impacted_dev_list_head;
	struct udev *udev = NULL;
	fpin_link_integrity_request_els_t *fpin_req = NULL;
	fpin_link_integrity_notification_t *li = NULL;
	uint32_t els_cmd = 0;
	struct wwn_list list_of_wwn;
	int count = -1;

	els_cmd = *(uint32_t *)fc_payload;
	FPIN_ILOG("Got CMD while processing as 0x%x\n", els_cmd);
	switch(els_cmd) {
		case ELS_CMD_FPIN:
			fpin_req = (fpin_link_integrity_request_els_t *)fc_payload;
			INIT_LIST_HEAD(&list_of_wwn.impacted_ports_wwn_head);

			/* Get the WWNs recieved from HBA firmware through ELS frame */
			count = fpin_els_extract_wwn(host_num,
					&(fpin_req->linkIntegrityDesc), &list_of_wwn);
			if (count <= 0) {
				FPIN_ELOG("Could not find any WWNs, ret = %d\n", count);
				return count;
			}

			/* Get the list of paths to be failed from WWNs aquired above */
			INIT_LIST_HEAD(&dm_list_head);
			INIT_LIST_HEAD(&impacted_dev_list_head);
			udev = udev_new();
			if (!udev) {
				fpin_els_free_wwn_list(&list_of_wwn);
				FPIN_ELOG("Can't create udev\n");
				return(-1);
			}

			FPIN_DLOG("Got new udev Resource\n");
			count = fpin_fetch_dm_lun_data(&list_of_wwn,
					&dm_list_head, &impacted_dev_list_head, udev);

			udev_unref(udev);
			if (count <= 0) {
				FPIN_ELOG("Could not find any sd to fail, ret = %d\n", count);
				fpin_els_free_wwn_list(&list_of_wwn);
				return count;
			}


			/* Fail the paths using multipath daemon */
			fpin_dm_fail_path(&dm_list_head, &impacted_dev_list_head);

			/* Free the WWNs list extracted from ELS recieved */
			fpin_dm_free_dev(&impacted_dev_list_head);
			fpin_free_dm(&dm_list_head);
			fpin_els_free_wwn_list(&list_of_wwn);
			break;

		default:
			FPIN_ELOG("Invalid command received: 0x%x\n", els_cmd);
			break;
	}

	return (count);
}

/*
 * Function:
 *	fpin_handle_els_frame
 *
 * Inputs:
 *	The ELS frame to be processed.
 *
 * Description:
 *	This function process the FPIN ELS frame received from HBA driver,
 *	and push the frame to approriate frame list. Currently we have only FPIN
 *	LI frame list.
 */
int
fpin_handle_els_frame(fpin_payload_t *fpin_payload) {
	uint32_t els_cmd = 0;
	int ret = -1;

	els_cmd = *(uint32_t *)fpin_payload->payload;
	FPIN_ILOG("Got CMD in add as 0x%x\n", els_cmd);
	switch(els_cmd) {
		case ELS_CMD_MPD:
		case ELS_CMD_FPIN:
			/*Push the Payload to FPIN frame queue. */
			ret = fpin_els_add_li_frame(fpin_payload);
			if (ret != 0) {
				FPIN_ELOG("Failed to process LI frame with error %d\n", ret);
			}
			break;

		case ELS_CMD_CJN:
			/*Push the Payload to CJN frame queue. */
			break;
		default:
			FPIN_ELOG("Invalid command received: 0x%x\n", els_cmd);
			break;
	}

	return (ret);
}

/*
 * This is the FPIN ELS consumer thread. The thread sleeps on pthread cond
 * variable unless notified by fpin_fabric_notification_receiver thread.
 * This thread is only to process FPIN-LI ELS frames. A new thread and frame
 * list will be added if any more ELS frames types are to be supported.
 */
void *fpin_els_li_consumer() {
	struct list_head marginal_list_head;
	char payload[FC_PAYLOAD_MAXLEN];
	int ret = 0;
	uint16_t host_num;
	struct els_marginal_list *els_marg;

	INIT_LIST_HEAD(&marginal_list_head);

	for ( ; ; ) {
		pthread_mutex_lock(&fpin_li_mutex);
		if (list_empty(&els_marginal_list_head)) {
			pthread_cond_wait(&fpin_li_cond, &fpin_li_mutex);
		}

		if (!list_empty(&els_marginal_list_head)) {
			FPIN_DLOG("Invoke List splice tail\n");
			list_splice_tail_init(&els_marginal_list_head, &marginal_list_head);
			pthread_mutex_unlock(&fpin_li_mutex);
		} else {
			FPIN_DLOG("Spurious/INTR wakeup, continue\n");
			pthread_mutex_unlock(&fpin_li_mutex);
			continue;
		}

		while (!list_empty(&marginal_list_head)) {
			els_marg  = list_first_entry(&marginal_list_head,
							struct els_marginal_list, els_frame);
			host_num = els_marg->host_num;
			memcpy(payload, els_marg->payload, FC_PAYLOAD_MAXLEN);
			list_del(&els_marg->els_frame);
			free(els_marg);

			/* Now finally process FPIN LI ELS Frame */
			FPIN_ILOG("Got a new Payload buffer, processing it\n");
			ret = fpin_process_els_frame(host_num, payload);
			if (ret <= 0 ) {
				FPIN_ELOG("ELS frame processing failed with ret %d\n", ret);
			}
		}
	}
}
