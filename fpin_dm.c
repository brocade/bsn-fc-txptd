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


pthread_cond_t fpin_li_marginal_dev_cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t fpin_li_marginal_dev_mutex = PTHREAD_MUTEX_INITIALIZER;

static int fpin_set_rport_marginal(int host_no, char *p_wwn)
{
	struct udev *udev = NULL;
	struct udev_enumerate *enumerate = NULL;
	struct udev_list_entry *devices = NULL, *dev_list_entry = NULL;
	struct udev_device *dev = NULL;
	int i = 0, ret = 0;
	char rport_host_buf[DEV_NODE_LEN];

	udev = udev_new();
	if (!udev) {
		FPIN_ELOG("Can't create udev\n");
		return(-1);
	}
	/* Create a list of the devices in the 'fc_remote_ports' subsystem. */
	enumerate = udev_enumerate_new(udev);
	if (enumerate == NULL) {
		FPIN_ELOG("Could not enumerate udev for fc_trans\n");
		udev_unref(udev);
		return (-EBADF);
	}

	ret = udev_enumerate_add_match_subsystem(enumerate, "fc_remote_ports");
	if (ret < 0) {
		FPIN_ELOG("Could not enumerate fc_tx sub with ret %d\n", ret);
		goto error;
	}

	ret = udev_enumerate_scan_devices(enumerate);
	if (ret < 0) {
		FPIN_ELOG("Could not scan block subsystem with ret %d\n", ret);
		goto error;
	}

	devices = udev_enumerate_get_list_entry(enumerate);
	if (devices == NULL) {
		FPIN_ELOG("NO devices found under block subsystem\n");
		ret = -ENODEV;
		goto error;
	}

	/* For each item enumerated, find if the target matches.
	 * fetch port_name and compare it with the PWWN in ELS.
	 * If it matches set the rport port_state to marginal
	 */
	snprintf(rport_host_buf, DEV_NODE_LEN, "%s%d", "rport-", host_no);

	udev_list_entry_foreach(dev_list_entry, devices) {
		const char *dir_path_buf, *target_buf, *port_wwn_buf, *port_state;
		char rport_name[DEV_NODE_LEN], *temp;
		int len;

		dir_path_buf = udev_list_entry_get_name(dev_list_entry);
		if (dir_path_buf == NULL) {
			FPIN_ELOG("Failed to get syspath for targets\n");
			continue;
		}

		dev = udev_device_new_from_syspath(udev, dir_path_buf);
		if (dev == NULL) {
			FPIN_ELOG("Failed to get device struct from path %s, err %d\n",
					dir_path_buf, errno);
			continue;
		}
		target_buf = udev_device_get_sysname(dev);
		if (target_buf == NULL) {
			FPIN_ELOG("Unable to get tgt sysname from host %s\n",
					rport_host_buf);
			udev_device_unref(dev);
			continue;
		}
		/* As the target_buf o/p is like rport-2:0-6 i.e
		 * rport-<hostno>:<channel>-<busno> and we need to get the
		 * rport-<hostnumber> info so we are extracting the rport-<hostno>
		 * from the target_buf and compare it with rport_host_buf
		 */
		temp = strchr(target_buf, ':');
		len = temp - target_buf;
		memset(rport_name, '\0', DEV_NODE_LEN);
		strncpy(rport_name, target_buf, len);

		FPIN_DLOG("Got Port Target_buf as %s :%s %d len %d\n",
			target_buf, rport_host_buf, sizeof(rport_host_buf), len);
		if (strcmp(rport_name, rport_host_buf) == 0) {
			port_wwn_buf = udev_device_get_sysattr_value(dev, "port_name");
			if (port_wwn_buf == NULL) {
				FPIN_ELOG("Could not get tgt WWN for %s\n", rport_host_buf);
				udev_device_unref(dev);
				continue;
			}
			port_state = udev_device_get_sysattr_value(dev, "port_state");
			if (port_state == NULL) {
				FPIN_ELOG("Could not get tgt WWN for %s\n", rport_host_buf);
				udev_device_unref(dev);
				continue;
			}
			if (!strcmp(port_wwn_buf, p_wwn)) {
				FPIN_DLOG("Got tgt WWN as %s ::: portstate: %s\n",
						port_wwn_buf, port_state);
				ret = udev_device_set_sysattr_value(dev, "port_state", "Marginal");
				if (ret >= 0) {
					FPIN_ILOG("set rport port state to marginal succeded\n");
				} else {
					FPIN_ILOG("set rport port state to marginal failed %d \n", ret);
					udev_device_unref(dev);
					goto error;
				}

			}
		}
		udev_device_unref(dev);
	}
error:
	/* Free the enumerator object */
	udev_enumerate_unref(enumerate);
	udev_unref(udev);
	return ret;

}

/*
 * Function:
 * 	fpin_insert_dm(struct list_head *dm_list_head, char *dm_name,
 * 			char *uid_name)
 *
 * Inputs:
 * 	1. Pointer to the Linked list head containing dm name and node.
 * 	2. The dm name which id of form mpath* (mpatha/mpathb etc)
 * 	3. The DM node name in /dev
 *
 * Description:
 * 	This function inserts the dm name (which is in form of sd*) and
 * 	device Mapper Name (dm-*) into the Linked list which is later used to
 * 	fail the path using multipath daemon.
 */
int
fpin_insert_dm(struct list_head *dm_list_head, const char *dm_name,
			const char *uid_name) {
	struct dm_devs *new_node = NULL;
	char *uid_ptr = NULL;
	int dm_name_len = 0, uid_name_len = 0;

	dm_name_len = strlen(dm_name);
	uid_name_len = strlen(uid_name);

	if ((dm_name_len > (DEV_NAME_LEN - 1)) ||
		(uid_name_len > (UUID_LEN -1 ))) {
		FPIN_ELOG("Failed to add %s, params exceed buffer length "
		"dm_name %d, uid_name %d\n", dm_name,
			dm_name_len, uid_name_len);
		return (-EINVAL);
	}

	/* Create a node */
	new_node = (struct dm_devs *) malloc(sizeof(struct dm_devs));
	if (new_node != NULL) {
		/* Set values in new node */
		strncpy(new_node->dm_name, dm_name, DEV_NAME_LEN);
		/*
		 * Checking with only a '-' as this function is onvoked only
		 * if uid_name has mpath- string in it.
		 */
		uid_ptr = strchr(uid_name, '-');
		if (uid_ptr != NULL) {
			uid_ptr++;
			/* No need to NULL check as UUID name cannot be blank */
			strncpy(new_node->dm_uuid, uid_ptr, UUID_LEN);
		} else {
			FPIN_ELOG("Failed to fetch dm_uuid for %s\n", dm_name);
			free(new_node);
			return (-EBADF);
		}

		FPIN_ILOG("Inserted %s : %s into dm list\n",
			new_node->dm_name, new_node->dm_uuid);
		list_add_tail(&(new_node->dm_list_head), dm_list_head);
	} else {
		FPIN_ELOG("Failed to add %s, OOM\n", dm_name);
		return -ENOMEM;
	}

	return (0);
}

/*
 * Function:
 * 	fpin_insert_sd(struct list_head *list_head, char *dev_name,
 * 					char *sd_node, char *serial_id)
 *
 * Inputs:
 * 	1. Pointer to the Linked list head containing dev name and node.
 * 	2. The dev name which is of form sd* (mpatha/mpathb etc)
 * 	3. The DM node name which is of the form MAJOR:MINOR
 *
 * Description:
 * 	This function inserts the dev name (which is in form of sd*) and
 * 	serial ID into the Linked list which is later used to
 * 	fail the path using multipath daemon.
 */
static int
fpin_insert_sd(struct list_head *impacted_dev_list_head, const char *dev_name,
			char *sd_node, const char *serial_id, char *port_wwn)
{
	struct impacted_devs *new_node = NULL;
	int dev_name_len = 0, sd_node_len = 0, serial_id_len = 0, sd_path_len = 0;

	dev_name_len = strlen(dev_name);
	sd_node_len = strlen(sd_node);
	serial_id_len = strlen(serial_id);

	if ((dev_name_len > (DEV_NAME_LEN - 1)) ||
		(sd_node_len > (DEV_NAME_LEN - 1)) ||
		(serial_id_len > (UUID_LEN - 1))) {
		FPIN_ELOG("Failed to add %s : %s, params exceed buffer length "
			"dev_name_len %d, node_len %d, serial_id_len %d\n",
			dev_name, sd_node, dev_name_len, sd_node_len, serial_id_len);
		return (-EINVAL);
	}

	/* Create a node */
	new_node = (struct impacted_devs*)
				malloc(sizeof(struct impacted_devs));
	if (new_node != NULL) {
		/* Set values in new node */
		strncpy(new_node->dev_name, dev_name, DEV_NAME_LEN);
		strncpy(new_node->dev_node, sd_node, DEV_NAME_LEN);
		strncpy(new_node->dev_serial_id, serial_id, UUID_LEN);
		strncpy(new_node->p_wwn, port_wwn, WWN_LEN);
		FPIN_ILOG("Inserted %s : %s : %s : p_wwn %s :into sd list\n",
			new_node->dev_name, new_node->dev_node,
			new_node->dev_serial_id, new_node->p_wwn);
		list_add_tail(&(new_node->dev_list_head), impacted_dev_list_head);
	} else {
		FPIN_ELOG("Failed to add %s : %s, OOM\n",
				dev_name, sd_node);
		return -ENOMEM;
	}

	return (0);
}

/*
 * Function:
 *	fpin_dm_insert_target
 *
 * Inputs:
 * 	1. Pointer to the Linked list which contains list of targets impacted.
 * 	2. The target id to be inserted to the above list.
 *
 * Description:
 * 	This function inserts the target name to a list of impacted targets.
 */
int
fpin_dm_insert_target(struct list_head *tgt_list_head, const char *target,
			const char *port_wwn) {

	struct targets *new_node = NULL;
	int tgt_name_len = 0;

	tgt_name_len = strlen(target);

	if (tgt_name_len > (TGT_NAME_LEN - 1)) {
		FPIN_ELOG("Failed to insert tgt %s, buffer length exceeded "
			"tgt_name_len %d\n", target, tgt_name_len);
		return (-EINVAL);
	}

	/* Create a node */
	new_node = (struct targets *) malloc(sizeof(struct targets));
	if (new_node != NULL) {
		/* Set values in new node */
		strncpy(new_node->target, target, TGT_NAME_LEN);
		strncpy(new_node->p_wwn, port_wwn, WWN_LEN);
		FPIN_ILOG("Inserted target %s and p_wwn into target list\n",
			new_node->target, new_node->p_wwn);
		list_add_tail(&(new_node->target_head), tgt_list_head);
	} else {
		FPIN_CLOG("Failed to insert target %s, OOM\n", target);
		return -ENOMEM;
	}

	return (0);
}

void
fpin_display_dm_list(struct list_head *list_head) {
	struct dm_devs *temp = NULL;
	if (list_empty(list_head)) {
		FPIN_DLOG("DM list is empty, not failing any sd\n");
	} else {
		list_for_each_entry(temp, list_head, dm_list_head) {
			FPIN_DLOG("Contains: dm_name: %s\n", temp->dm_name);
		}
	}
}

void
fpin_display_impacted_dev_list(struct list_head *list_head) {
	struct impacted_devs *temp = NULL;
	if (list_empty(list_head)) {
		FPIN_DLOG("DM list is empty, not failing any sd\n");
	} else {
		list_for_each_entry(temp, list_head, dev_list_head) {
			FPIN_DLOG("Contains: dev_name: %s, dev_node: %s\n",
				temp->dev_name, temp->dev_node);
		}
	}
}

/*
 * Function:
 * 	fpin_fetch_dm_for_sd
 *
 * Inputs:
 * 	1. List of all multipath DMs in the host.
 * 	2. The serial ID of the device to be mapped with UUID of DM.
 * 	3. Pointer to the memory where the impacted DM name (mpath*) will be stored.
 *
 * Returns:
 * 	1: If the corresponding holder of device is found., -1 Otherwise.
 *
 * Description:
 * 	This function gets the DM name in (mpath*) format, for the device whose
 * serial ID is passed. The DM name is stored in impacted_dm parameter, which
 * will be passed to dm_get_status of device mapper to get the number of active
 * paths for the DM.
 */
int
fpin_fetch_dm_for_sd(struct list_head *dm_head,
				char *dev_serial_id, char **impacted_dm) {
	struct dm_devs *dm = NULL;

	if (list_empty(dm_head)) {
		FPIN_ELOG("DM list is empty, returning -1\n");
		return (-1);
	} else {
		list_for_each_entry(dm, dm_head, dm_list_head) {
			if (strncmp(dev_serial_id, dm->dm_uuid, UUID_LEN) == 0) {
				*impacted_dm = dm->dm_name;
				FPIN_DLOG("Found impacted dm %s\n", *impacted_dm);
				return (1);
				break;
			}
		}
	}

	return (0);
}

int dm_get_status(const char *name, char *outstatus)
{
        int r = 1;
        struct dm_task *dmt;
        uint64_t start, length;
        char *target_type = NULL;
        char *status = NULL;

        if (!(dmt = dm_task_create(DM_DEVICE_STATUS)))
                return 1;

        if (!dm_task_set_name(dmt, name))
                goto out;

        dm_task_no_open_count(dmt);

        if (!dm_task_run(dmt))
                goto out;

        /* Fetch 1st target */
        dm_get_next_target(dmt, NULL, &start, &length,
                           &target_type, &status);
        if (!status) {
                FPIN_ELOG("get null status.\n");
                goto out;
        }

        if (snprintf(outstatus, DM_PARAMS_SIZE, "%s", status) <= DM_PARAMS_SIZE)
                r = 0;
out:
        if (r)
                FPIN_ELOG("%s: error getting map status string", name);

        dm_task_destroy(dmt);
        return r;
}

int send_packet(int fd, const char *buf)
{
        if (mpath_send_cmd(fd, buf) < 0)
                return -errno;
        return 0;
}

/*
 * receive a packet in length prefix format
 */
int recv_packet(int fd, char **buf, unsigned int timeout)
{
	int ret = mpath_recv_reply(fd, buf, timeout);
	if (ret != 0)
		return -errno;
	return 0;
}

/*
 * Function:
 * 	fpin_set_marginal_state
 *
 * Inputs:
 * 	Cmd:cmd that needs to be passed to dm
 * Description:
 * 	This will set/unset marginal state of a device
 */

static int  fpin_set_marginal_state(char* cmd){
	int ret = -1, fd = -1;
	char *reply = NULL;

	fd = mpath_connect();
	if (fd < 0) {
		FPIN_CLOG("Not any devices, mpath_connect failed with %d\n", fd);
		return fd;
	}

	FPIN_DLOG("CMD %s\n", cmd);
	if ((ret = send_packet(fd, cmd)) != 0) {
		FPIN_ELOG("send_packet failed with %d for cmd %s\n", ret, cmd);
		FPIN_ELOG("\nRecheck the path state by running"
			 "cmd: multipathd show paths format \n");
		goto error;
	}

	ret = recv_packet(fd, &reply, DEFAULT_REPLY_TIMEOUT);
	if (ret < 0) {
		if (ret == -ETIMEDOUT) {
			FPIN_ELOG("timeout receiving packet for cmd %s\n", cmd);
		} else {
			FPIN_ELOG("error %d receiving packet for cmd %s \n",
					ret, cmd);
		}
		FPIN_ELOG("\nRecheck the path state by running"
			 "cmd: multipathd show paths format \n");
	} else {
		if (strncmp(reply,"ok\n", 3) == 0) {
			FPIN_ILOG("Successfully set state %s\n",cmd);
		} else if ((strncmp(reply, "fail\n", 5) == 0) ||
				(strncmp(reply, "timeout\n", 8) == 0)) {
			FPIN_ELOG("Unable to set  state %s, reason %s",cmd,
					reply);
			ret = -EINVAL;
		}

	}
	error:
	mpath_disconnect(fd);
	return ret;

}

/*
 * Function:
 * 	fpin_unset_marginal_dev
 *
 * Inputs:
 * 	host_num:Host number
 * 	dev_name:device name.
 * Description:
 * 	Adds the marginal devices into the list
 */
void
fpin_unset_marginal_dev(uint32_t host_num, struct list_head *tgt_head) {
	struct marginal_dev_list *tmp_marg = NULL;
	struct list_head *current_node = NULL;
	struct list_head *temp = NULL;
	char cmd[CMD_LEN];
	int ret = 0;

	pthread_mutex_lock(&fpin_li_marginal_dev_mutex);
	if (list_empty(tgt_head)) {
		FPIN_ILOG("Marginal List is empty\n");
	} else {
		list_for_each_safe(current_node, temp, tgt_head) {
			tmp_marg = list_entry(current_node,
					struct marginal_dev_list,
					marginal_dev_list_head);

			FPIN_DLOG(" marginal dev: is %s %d\n", tmp_marg->dev_name, tmp_marg->host_num);
			if (tmp_marg->host_num != host_num)
				continue;
			snprintf(cmd, CMD_LEN, "path %s unsetmarginal", tmp_marg->dev_name);
			ret = fpin_set_marginal_state(cmd);
			if (ret <0)
				continue;
			list_del(current_node);
			free(tmp_marg);
		}
	}
	pthread_mutex_unlock(&fpin_li_marginal_dev_mutex);

}

/*
 * Function:
 * 	fpin_add_marginal_dev_info
 *
 * Inputs:
 * 	host_num:Host number
 * 	dev_name:device name.
 * Description:
 * 	Adds the marginal devices into the list
 */
static void
fpin_add_marginal_dev_info(uint32_t host_num, char *devname) {
	struct marginal_dev_list *newdev = NULL;

	newdev = (struct marginal_dev_list *) calloc(1,
					sizeof(struct marginal_dev_list));
	if (newdev != NULL) {
		newdev->host_num = host_num;
		strncpy(newdev->dev_name, devname, (DEV_NAME_LEN - 1));
		FPIN_DLOG("\n%s hostno %d devname %s\n",__func__,
					host_num, newdev->dev_name);
		pthread_mutex_lock(&fpin_li_marginal_dev_mutex);
		list_add_tail(&(newdev->marginal_dev_list_head),
					&fpin_li_marginal_dev_list_head);
		pthread_mutex_unlock(&fpin_li_marginal_dev_mutex);
	} else {
		FPIN_CLOG("\n Mem alloc failed.Failed to add marginal dev info"
			" Unset the marginal state manually after recovery"
			" for  hostno %d devname %s \n",
				host_num, devname);
	}
}

/*
 * Function:
 * 	fpin_dm_marginal_path
 *
 * Inputs:
 * 	dm_list_head:			List of all DMs in the host.
 * 	impacted_dev_list_head: List of all impacted devices, whose WWN was sent
 * 							as part of FPIN ELS frame.
 *
 * Description:
 * 	Uses Multipath daemon help to fail a path permanently unless manually
 * 	reinstated. Maps the impacted Devices to their corresponding holders/dms',
 * 	and fails the path only if there is at least one other active path present.
 */
void
fpin_dm_marginal_path(uint32_t host_num, struct list_head *dm_list_head,
			struct list_head *impacted_dev_list_head) {
	struct impacted_devs *temp = NULL;
	char *reply = NULL;
	char *impacted_dm = NULL;
	char cmd[CMD_LEN], dm_status[DM_PARAMS_SIZE];
	int ret = -1, fd = -1;

	if (list_empty(dm_list_head)) {
		FPIN_ELOG("DM list is empty, not failing any sd\n");
		return;
	}

	if (list_empty(impacted_dev_list_head)) {
		FPIN_ELOG("SD List is empty, not failing any sd\n");
		return;
	}
	list_for_each_entry(temp, impacted_dev_list_head, dev_list_head) {
		ret = fpin_fetch_dm_for_sd(dm_list_head,
				temp->dev_serial_id, &impacted_dm);
		if (ret <= 0) {
			FPIN_ELOG("Failed to fetch DM for sd %s\n", temp->dev_name);
			continue;
		}

		FPIN_CLOG("DM to fail is %s\n", impacted_dm);
		memset(dm_status, '\0', DM_PARAMS_SIZE);
		ret = dm_get_status(impacted_dm, dm_status);
		if (!ret) {
			/*
			 * set  the impacted Path in DM to marginal
			 */
			FPIN_ILOG("setting marginal state %s:%s %s p_wwn%s host_num %d\n",
					temp->dev_node, temp->dev_name, temp->dev_serial_id,
					temp->p_wwn, host_num);
			snprintf(cmd, CMD_LEN, "path %s setmarginal", temp->dev_name);
			ret = fpin_set_marginal_state(cmd);
			if (ret < 0)
				continue;
			else {
				ret = fpin_set_rport_marginal(host_num, temp->p_wwn);
				if (ret < 0)
					FPIN_ELOG("failed to set the rport state :%s\n", temp->p_wwn);

				fpin_add_marginal_dev_info(host_num, temp->dev_name);

			}

		}
	}
}

void
fpin_dm_display_target(struct list_head *tgt_head) {
	struct targets *temp = NULL;

	if (list_empty(tgt_head)) {
		FPIN_DLOG("Target List is empty\n");
	} else {
		list_for_each_entry(temp, tgt_head, target_head)
			FPIN_DLOG("Target is %s : p_wwn is %s:\n", temp->target, temp->p_wwn);
	}
}

int
fpin_dm_find_target(struct list_head *tgt_head, const char *target,
			char *port_wwn) {
	struct targets *temp = NULL;

	if (list_empty(tgt_head)) {
		FPIN_DLOG("Target List is empty, %s not found\n", target);
	} else {
		list_for_each_entry(temp, tgt_head, target_head) {
			/*
			 * Using strcmp instead of strncmp intentionally. Both the strings
			 * being compared are NULL terminated, one string is recieved from
			 * udev, while other is explicitly NULL terminated during creation.
			 * Preventing possiblity of temp->target (Ex. target6:0:1),
			 * matching with target6:0:10
			 */
			if (strcmp(temp->target, target) == 0) {
				FPIN_DLOG("Found Target %s\n", target);
				strncpy(port_wwn, temp->p_wwn, WWN_LEN);
				return (1);
			}
		}
	}

	return (0);
}

void
fpin_free_dm(struct list_head *dm_head) {
	struct list_head *current_node = NULL;
	struct list_head *temp = NULL;
	struct dm_devs *tmp_dm = NULL;
	if (list_empty(dm_head)) {
		FPIN_DLOG("List is empty, nothing to delete..\n");
		return;
	} else {
		list_for_each_safe(current_node, temp, dm_head) {
			tmp_dm = list_entry(current_node, struct dm_devs, dm_list_head);
			FPIN_DLOG("Free dm %s\n", tmp_dm->dm_name);
			list_del(current_node);
			free(tmp_dm);
		}
	}
}

void
fpin_dm_free_target(struct list_head *tgt_head) {
	struct list_head *current_node = NULL;
	struct list_head *temp = NULL;
	struct targets *tmp_tgt = NULL;
	if (list_empty(tgt_head)) {
		FPIN_DLOG("List is empty, nothing to delete..\n");
		return;
	} else {
		list_for_each_safe(current_node, temp, tgt_head) {
			tmp_tgt = list_entry(current_node, struct targets, target_head);
			FPIN_DLOG("Free target %s\n", tmp_tgt->target);
			list_del(current_node);
			free(tmp_tgt);
		}
	}
}

void
fpin_dm_free_dev(struct  list_head *sd_head) {
	struct list_head *current_node = NULL;
	struct list_head *temp = NULL;
	struct impacted_devs *tmp_sd = NULL;
	if (list_empty(sd_head)) {
		FPIN_DLOG("List is empty, nothing to delete..\n");
		return;
	} else {
		list_for_each_safe(current_node, temp, sd_head) {
			tmp_sd = list_entry(current_node, struct impacted_devs, dev_list_head);
			FPIN_DLOG("Free sd %s\n", tmp_sd->dev_name);
			list_del(current_node);
			free(tmp_sd);
		}
	}
}

/*
 * Function:
 *	fpin_fetch_dm_lun_data
 *
 * Inputs:
 * 	1. Pointer to the Linked list which contains list of port WWNs impacted.
 * 	2. Pointer to the Linked list which will be populated with impacted sd
 * 	   and dms. These sd* will be failed using multipathd.
 *
 * Description:
 * 	This function takes in the list of impacted WWNs as input and translates
 * 	them into sd* and dm-* which will be failed by multipathd.
 */
int
fpin_fetch_dm_lun_data(struct wwn_list *list, struct list_head *dm_list_head,
				struct list_head *impacted_dev_list_head, struct udev *udev) {
	struct list_head impacted_tgt_list_head;
	int ret = -1;

	FPIN_DLOG("Get DM Lun Data\n");
	INIT_LIST_HEAD(&impacted_tgt_list_head);
	/* Get Targets linked to the port on whichthe ELS frame was recieved */
	ret = fpin_dm_populate_target(list, &impacted_tgt_list_head, udev);
	if (ret <= 0) {
		FPIN_ELOG("No targets found, returning ret %d\n", ret);
		return (ret);
	}

	FPIN_DLOG("Display target\n");
	fpin_dm_display_target(&impacted_tgt_list_head);

	/* Get sd to dm mapping for populated targets */
	ret = fpin_populate_dm_lun(dm_list_head, impacted_dev_list_head, udev,
				&impacted_tgt_list_head);
	if (ret <= 0) {
		FPIN_ELOG("No sd found to fail, returning ret %d\n", ret);
		fpin_dm_free_target(&impacted_tgt_list_head);
		return (ret);
	}

	fpin_display_dm_list(dm_list_head);
	fpin_display_impacted_dev_list(impacted_dev_list_head);

	fpin_dm_free_target(&impacted_tgt_list_head);
	return (ret);
}

/*
 * Function:
 *	fpin_dm_populate_target
 *
 * Inputs:
 * 	1. Pointer to the Linked list which contains list of port WWNs impacted.
 * 	2. Pointer to the Linked list which will be populated with impacted targets.
 * 	3. Pointer to the udev structure used to parse sysfs classes.
 *
 * Description:
 * 	This function takes in the list of impacted WWNs as input and translates
 * 	them into target IDs. These target IDs are used to fetch sd* and dm-*
 * 	details, which will be failed by multipath daemon.
 */
int
fpin_dm_populate_target(struct wwn_list *list, struct list_head *tgt_list,
			struct udev *udev) {

	char host_buf[DEV_NODE_LEN];
	char *host_name_ptr = NULL;
	int target_count = 0, host_found = 0, wwn_exists = 0;
	struct udev_enumerate *enumerate = NULL;
	struct udev_list_entry *devices = NULL, *dev_list_entry = NULL;
	struct udev_device *dev = NULL, *parent_dev = NULL;
	int i = 0, ret = 0;

	/* Create a list of the devices in the 'fc_host' subsystem. */
	enumerate = udev_enumerate_new(udev);
	if (enumerate == NULL) {
		FPIN_ELOG("Could not enumerate udev\n");
		return (-EBADF);
	}

	ret = udev_enumerate_add_match_subsystem(enumerate, "fc_host");
	if (ret < 0) {
		FPIN_ELOG("Could not enumerate fc_host subsystem with ret %d\n", ret);
		udev_enumerate_unref(enumerate);
		return (ret);
	}
	ret = udev_enumerate_scan_devices(enumerate);
	if (ret < 0) {
		FPIN_ELOG("Could not scan fc_host subsystem with ret %d\n", ret);
		udev_enumerate_unref(enumerate);
		return (ret);
	}
	devices = udev_enumerate_get_list_entry(enumerate);
	if (devices == NULL) {
		FPIN_ELOG("NO devices found under fc_host subsystem\n");
		udev_enumerate_unref(enumerate);
		return(-ENODEV);
	}

	/* For each item enumerated, find if the target matches.
	 * fetch port_name and compare it with the HBA PWWN in ELS.
	 * Whichever matches, store the sys_name
	 */
	snprintf(host_buf, DEV_NODE_LEN, "%s%d", "host", list->host_num);
	udev_list_entry_foreach(dev_list_entry, devices) {
		const char *dir_path_buf;
		dir_path_buf = udev_list_entry_get_name(dev_list_entry);
		if (dir_path_buf == NULL) {
			FPIN_ELOG("Failed to get syspath for host %s\n", host_buf);
			continue;
		}

		FPIN_DLOG("Got Sys path as %s\n", dir_path_buf);
		host_name_ptr = strrchr(dir_path_buf, '/');
		if (host_name_ptr == NULL) {
			FPIN_ELOG("Failed to get host name from path %s\n", dir_path_buf);
			continue;
		}

		host_name_ptr++;

		FPIN_DLOG("Checking for host %s\n", host_buf);
		/* Both Strings are NULL terminated */
		if (strcmp(host_name_ptr, host_buf) == 0) {
			host_found = 1;
			break;
		}
	}

	/* Free the enumerator object */
	udev_enumerate_unref(enumerate);
	if (host_found == 0) {
		FPIN_ELOG("Could not find any host with %d\n", list->host_num);
		return (0);
	}

	FPIN_DLOG("Find targets visible to host %s\n", host_buf);

	/* Create a list of the devices in the 'fc_transport' subsystem. */
	enumerate = udev_enumerate_new(udev);
	if (enumerate == NULL) {
		FPIN_ELOG("Could not enumerate udev for fc_trans\n");
		return (-EBADF);
	}

	ret = udev_enumerate_add_match_subsystem(enumerate, "fc_transport");
	if (ret < 0) {
		FPIN_ELOG("Could not enumerate fc_tx sub with ret %d\n", ret);
		udev_enumerate_unref(enumerate);
		return (ret);
	}

	ret = udev_enumerate_scan_devices(enumerate);
	if (ret < 0) {
		FPIN_ELOG("Could not scan block subsystem with ret %d\n", ret);
		udev_enumerate_unref(enumerate);
		return (ret);
	}

	devices = udev_enumerate_get_list_entry(enumerate);
	if (devices == NULL) {
		FPIN_ELOG("NO devices found under block subsystem\n");
		udev_enumerate_unref(enumerate);
		return(-ENODEV);
	}

	/* For each item enumerated, find if the target matches.
	 * fetch port_name and compare it with the HBA PWWN in ELS.
	 * Whichever matches, store the sys_name
	 */
	udev_list_entry_foreach(dev_list_entry, devices) {
		const char *dir_path_buf, *target_buf, *port_wwn_buf;
		dir_path_buf = udev_list_entry_get_name(dev_list_entry);
		if (dir_path_buf == NULL) {
			FPIN_ELOG("Failed to get syspath for targets\n");
			continue;
		}

		dev = udev_device_new_from_syspath(udev, dir_path_buf);
		if (dev == NULL) {
			FPIN_ELOG("Failed to get device struct from path %s, err %d\n",
						dir_path_buf, errno);
			continue;
		}

		parent_dev = udev_device_get_parent_with_subsystem_devtype(
						dev, "scsi", "scsi_host");
		if (!parent_dev) {
			FPIN_ELOG("Unable to find parent device for %s\n",
							udev_device_get_sysname(dev));
			udev_device_unref(dev);
			continue;
		}

		target_buf = udev_device_get_sysname(dev);
		if (target_buf == NULL) {
			FPIN_ELOG("Unable to get tgt sysname from host %s\n", host_buf);
			udev_device_unref(dev);
			continue;
		}

		FPIN_DLOG("Got Port Target_buf as %s\n", target_buf);
		/*
		 * Intentionally using strcmp as both strings are confirmed
		 * to be NULL terminated. We do not want to match host1 with host10.
		 */
		if (strcmp(host_buf, udev_device_get_sysname(parent_dev)) == 0) {
			port_wwn_buf = udev_device_get_sysattr_value(dev, "port_name");
			if (port_wwn_buf == NULL) {
				FPIN_ELOG("Could not get tgt WWN for %s\n", host_buf);
				udev_device_unref(dev);
				continue;
			}

			FPIN_DLOG("Got tgt WWN as %s\n", port_wwn_buf);
			wwn_exists = fpin_els_wwn_exists(list, port_wwn_buf);
			if (wwn_exists) {
				FPIN_DLOG("Found a target %s %s\n", target_buf, port_wwn_buf);
				if ((fpin_dm_insert_target(tgt_list, target_buf, port_wwn_buf)) == 0)
					target_count++;
			}
		}

		udev_device_unref(dev);
	}

	/* Free the enumerator object */
	udev_enumerate_unref(enumerate);

	return (target_count);
}

/*
 * Function:
 *	fpin_populate_dm_lun
 *
 * Inputs:
 * 	1. Pointer to the Linked list which contains list of port WWNs impacted.
 * 	2. Pointer to the Linked list which will be populated with impacted sd
 * 	   and dms. These sd* will be failed using multipathd.
 *	3. Pointer to the list of impacted target IDs.
 *
 * Description:
 * 	This function takes in the list of impacted WWNs as input and translates
 * 	them into sd* and dm-* which will be failed by multipathd.
 */
int
fpin_populate_dm_lun(struct list_head *dm_list_head,
			struct list_head *impacted_dev_list_head,
			struct udev *udev, struct list_head *target_head) {
	char lun_buf[DEV_NODE_LEN];
	int wwn_exists = 0, dm_count = 0;
	int sd_count = 0, ret = 0;
	char port_wwn[WWN_LEN];
	struct udev_enumerate *enumerate = NULL;
	struct udev_list_entry *devices = NULL, *dev_list_entry = NULL;
	struct udev_device *dev = NULL, *parent_dev = NULL;
	int i = 0;

	/* Create a list of the devices in the 'block' subsystem. */
	enumerate = udev_enumerate_new(udev);
	if (enumerate == NULL) {
		FPIN_ELOG("Could not enumerate udev\n");
		return (-EBADF);
	}

	ret = udev_enumerate_add_match_subsystem(enumerate, "block");
	if (ret < 0) {
		FPIN_ELOG("Could not enumerate block subsystem with ret %d\n", ret);
		udev_enumerate_unref(enumerate);
		return (ret);
	}

	ret = udev_enumerate_scan_devices(enumerate);
	if (ret < 0) {
		FPIN_ELOG("Could not scan block subsystem with ret %d\n", ret);
		udev_enumerate_unref(enumerate);
		return (ret);
	}

	devices = udev_enumerate_get_list_entry(enumerate);
	if (devices == NULL) {
		FPIN_ELOG("NO devices found under block subsystem\n");
		udev_enumerate_unref(enumerate);
		return(-ENODEV);
	}

	/* For each item enumerated, find if the target matches.
	 * fetch port_name and compare it with the HBA PWWN in ELS.
	 * Whichever matches, store the sys_name
	 */
	FPIN_DLOG("Looping over Block...\n");
	udev_list_entry_foreach(dev_list_entry, devices) {
		const char *dir_path_buf, *dm_buf, *dev_buf;
		const char *target_buf, *uid_buf;
		dir_path_buf = udev_list_entry_get_name(dev_list_entry);
		dev = udev_device_new_from_syspath(udev, dir_path_buf);
		if (dev == NULL) {
			FPIN_ELOG("Failed to get device struct from path %s, err %d\n",
				dir_path_buf, errno);
			continue;
		}
		/* Handle DMs */
		dev_buf = udev_device_get_sysname(dev);
		FPIN_DLOG("Got dev_name as %s\n", dev_buf);
		if (strncmp("dm-", dev_buf, 3) == 0) {
			uid_buf = udev_device_get_property_value(dev, "DM_UUID");
			if (strncmp("mpath-", uid_buf, 6) == 0) {
				dm_buf = udev_device_get_property_value(dev, "DM_NAME");
				ret = fpin_insert_dm(dm_list_head, dm_buf, uid_buf);
				if (ret < 0) {
					FPIN_ELOG("Failed to Insert %s : %s\n", dev_buf, dm_buf);
				} else {
					dm_count++;
				}
			}

		} else if (strncmp("sd", dev_buf, 2) == 0) {
			/* Handle SDs */
			FPIN_DLOG("####Got syspath as %s\n", dir_path_buf);
			parent_dev = udev_device_get_parent_with_subsystem_devtype(
				dev, "scsi", "scsi_target");
			if (!parent_dev) {
				FPIN_ELOG("Unable to find parent device for %s\n",
					udev_device_get_sysname(dev));
				udev_device_unref(dev);
				continue;
			}

			target_buf = udev_device_get_sysname(parent_dev);
			FPIN_ILOG("###Got target_buf as %s\n", target_buf);

			if (fpin_dm_find_target(target_head, target_buf, port_wwn) != 0) {
				snprintf(lun_buf, sizeof(lun_buf), "%s:%s",
					udev_device_get_property_value(dev, "MAJOR"),
					udev_device_get_property_value(dev, "MINOR"));
				uid_buf = udev_device_get_property_value(dev, "ID_SERIAL");
				FPIN_ILOG("###Attempting %s, %s\n", lun_buf, uid_buf);
				ret = fpin_insert_sd(impacted_dev_list_head, dev_buf,
					lun_buf, uid_buf, port_wwn);
				if (ret < 0) {
					FPIN_ELOG("Failed to insert %s %s to sd list\n",
							dev_buf, lun_buf);
				} else {
					sd_count++;
				}
			}
		}

		udev_device_unref(dev);
	}

	udev_enumerate_unref(enumerate);

	if (dm_count <= 0) {
		if (sd_count > 0) {
			fpin_dm_free_dev(impacted_dev_list_head);
		}
		return(dm_count);
	} else if (sd_count <= 0) {
		fpin_free_dm(dm_list_head);
	}

	return (sd_count);
}
