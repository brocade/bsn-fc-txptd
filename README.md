# bsn-fc-txptd
Fibre Channel Transport Services on Linux
###############################################
	The purpose of this daemon is to add FC network intelligence in host and
host intelligence in FC network. This daemon would interoperate with
Brocade FC fabric in order to improve the response time of the MPIO failovers.
In future, it can also collect the congestion related details and perform
workload analysis, and provide QOS at application level by interoperating with
application performance profiling software.
 
Prerequisites
	For this daemon to be fully functional, following devices are required.
	1. A Linux host with FC HBA attached.
	2. A Brocade FC network switch which sends the FPIN-LI ELS frame.
	3. The FC HBA driver which passes through the FPIN-LI ELS sent from switch
		to host.

Usage:
	This daemon cannot be launched without HBA driver support.
	Currently, the daemon suports only the marginal path failover.

Steps performed during daemon execution:
1.	The FC networking switch sends an FPIN-LI ELS frame,
	to the HBA port. This frame currently contains the port ID of the HBA port.

2.	The HBA driver passes the ELS frame up to the FCTXPTD daemon.

3.	The daemon, on receiveing the ELS frame, extracts the HBA port ID in the
	frame and translates it to Port WWN. This is done by parsing the 
	/sys/class/fc_host/host* directory, where one directory is created per HBA
	port to the host. This host directory contains the Port ID, Port WWN,
	symbolic link to target LUNs etc.

4.	Port WWNs of the targets to be failed is populated in a list. These WWNs are
	sent by FC networking switch through FPIN-LI ELS frame.

5.	Once the WWNs are populated above, the daemon populates all the target IDs
	which are visible from the HBA port on which the ELS frame was received.
	They will be in the form of targetx:x:x.

6.	With the list of targets populated, the daemon parses through the 
	/sys/class/fc_transport directory, where all the targets which are visible
	through HBA ports are stored. Only the targets in the list populated in
	point 4, is parsed to get the sd* and dm-* information. A list of sd* which
	are to be failed is populated here. 

7.	Finally, the list populated above is used to set the path to marginal
	using multipath	libraries.

8.	Once the link integrity issues are fixed ,user needs to do port toggling
	i.e port disable and port enable to transition the marginal paths to normal.

9.	On receving the LINKUP/RSCN events, the daemon will set the marginal paths associated
	with host number to normal.

10.	User can run the below command to check the status of a path
		multipathd show paths format "%d %t %M" .

	The sample o/p is below

	dev dm_st  marginal_st
	sda undef  normal
	sdb active normal
	sdc active marginal


Steps to compile:
Please install udev,pthread ,devmapper libraries before we start compiling.
make clean
make
