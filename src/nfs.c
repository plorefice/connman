/*
 *
 *  Connection Manager
 *
 *  Copyright (C) 2007-2014  Intel Corporation. All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "connman.h"

enum connman_nfs_method {
	NFS_METHOD_STATIC = 0,
	NFS_METHOD_DHCP,
	NFS_METHOD_NONE
};

struct connman_nfs_connection {
	bool enabled;
	enum connman_nfs_method method;

	char *ipaddr;
	char *bootserver;
	char *gateway;
	char *netmask;
	char *hostname;
	char *devname;
	char *nameservers[2];

	struct connman_device *device;
	struct connman_service *service;
};

static struct connman_nfs_connection nfsroot;

bool connman_nfs_get_enabled(void)
{
	return nfsroot.enabled;
}

const char * connman_nfs_get_interface(void)
{
	return nfsroot.devname;
}

int connman_nfs_set_device(struct connman_device *device)
{
	DBG("device %p", device);

	nfsroot.device = device;

	return 0;
}

struct connman_device * connman_nfs_get_device(void)
{
	return nfsroot.device;
}

int connman_nfs_set_service(struct connman_service *service)
{
	DBG("service %p", service);

	nfsroot.service = service;

	return 0;
}

struct connman_service * connman_nfs_get_service(void)
{
	return nfsroot.service;
}

void connman_nfs_configure_service(void)
{
	struct connman_service * service;
	struct connman_ipconfig * ipconfig;

	service = connman_nfs_get_service();
	ipconfig = __connman_service_get_ip4config(service);

	switch (nfsroot.method) {
	case NFS_METHOD_DHCP:
		__connman_ipconfig_set_method(ipconfig, CONNMAN_IPCONFIG_METHOD_DHCP);
		break;

	case NFS_METHOD_STATIC:
	case NFS_METHOD_NONE:
		__connman_ipconfig_set_method(ipconfig, CONNMAN_IPCONFIG_METHOD_MANUAL);
		__connman_ipconfig_set_local(ipconfig, nfsroot.ipaddr);
		__connman_ipconfig_set_gateway(ipconfig, nfsroot.gateway);
		__connman_ipconfig_set_prefixlen(ipconfig,
			connman_ipaddress_calc_netmask_len(nfsroot.netmask));
		break;
	}

	__connman_service_nameserver_append(service, nfsroot.nameservers[0], false);
	__connman_service_nameserver_append(service, nfsroot.nameservers[1], false);
}

static void parse_cmdline(void)
{
	gchar *cmdline, **args, **arg;

	g_file_get_contents("/proc/cmdline", &cmdline, NULL, NULL);

	args = g_strsplit(cmdline, " ", -1);

	for (arg = args; *arg; arg++) {
		gchar **keyval;

		keyval = g_strsplit(*arg, "=", 2);

		if (g_strcmp0(keyval[0], "nfsroot") == 0){
			nfsroot.enabled = true;
		}
		else if (g_strcmp0(keyval[0], "ip") == 0) {
			if (g_strcmp0(keyval[1], "dhcp") == 0) {
				nfsroot.method = NFS_METHOD_DHCP;
			}
			else {
				gchar **addrlist;

				addrlist = g_strsplit(keyval[1], ":", -1);

				nfsroot.method = NFS_METHOD_STATIC;
				nfsroot.ipaddr = g_strdup(addrlist[0]);
				nfsroot.bootserver = g_strdup(addrlist[1]);
				nfsroot.gateway = g_strdup(addrlist[2]);
				nfsroot.netmask = g_strdup(addrlist[3]);
				nfsroot.hostname = g_strdup(addrlist[4]);

				nfsroot.nameservers[0] = g_strdup(addrlist[7]);
				nfsroot.nameservers[1] = g_strdup(addrlist[8]);

				g_strfreev(addrlist);
			}
		}

		g_strfreev(keyval);
	}

	g_strfreev(args);
	g_free(cmdline);
}

static void parse_netpnp(void)
{
	gchar *pnp, **lines, **line;

	g_file_get_contents("/proc/net/pnp", &pnp, NULL, NULL);

	lines = g_strsplit(pnp, "\n", -1);

	for (line = lines; *line; line++) {
		gchar **keyval;

		if ((*line)[0] == '#')
			continue;

		keyval = g_strsplit(*line, " ", 2);

		if (g_strcmp0(keyval[0], "nameserver") == 0)
			nfsroot.nameservers[0] = g_strdup(keyval[1]);
		else if (g_strcmp0(keyval[0], "bootserver") == 0)
			nfsroot.bootserver = g_strdup(keyval[1]);

		g_strfreev(keyval);
	}

	g_strfreev(lines);
	g_free(pnp);
}

static int parse_nfsrootargs(void)
{
	parse_cmdline();

	if (!nfsroot.enabled)
		return 0;

	switch (nfsroot.method)
	{
	case NFS_METHOD_DHCP:
		parse_netpnp();

	case NFS_METHOD_STATIC:
	case NFS_METHOD_NONE:
	default:
		break;
	}

	return 0;
}

static void nfs_init(const char *devname)
{
	nfsroot.enabled = false;
	nfsroot.method = NFS_METHOD_NONE;

	nfsroot.ipaddr = NULL;
	nfsroot.bootserver = NULL;
	nfsroot.gateway = NULL;
	nfsroot.netmask = NULL;
	nfsroot.hostname = NULL;

	nfsroot.devname = g_strdup(devname);

	nfsroot.nameservers[0] = NULL;
	nfsroot.nameservers[1] = NULL;

	nfsroot.device = NULL;
	nfsroot.service = NULL;
}

int __connman_nfs_init(const char *devname)
{
	DBG("devname %s", devname);

	nfs_init(devname);

	return parse_nfsrootargs();
}

void __connman_nfs_cleanup(void)
{
	DBG("");

	g_free(nfsroot.ipaddr);
	g_free(nfsroot.bootserver);
	g_free(nfsroot.gateway);
	g_free(nfsroot.netmask);
	g_free(nfsroot.hostname);
	g_free(nfsroot.devname);
	g_free(nfsroot.nameservers[0]);
	g_free(nfsroot.nameservers[1]);
}
