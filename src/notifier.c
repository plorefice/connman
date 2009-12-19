/*
 *
 *  Connection Manager
 *
 *  Copyright (C) 2007-2009  Intel Corporation. All rights reserved.
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

#include <gdbus.h>

#include "connman.h"

static DBusConnection *connection = NULL;

static GSList *notifier_list = NULL;

static gint compare_priority(gconstpointer a, gconstpointer b)
{
	const struct connman_notifier *notifier1 = a;
	const struct connman_notifier *notifier2 = b;

	return notifier2->priority - notifier1->priority;
}

/**
 * connman_notifier_register:
 * @notifier: notifier module
 *
 * Register a new notifier module
 *
 * Returns: %0 on success
 */
int connman_notifier_register(struct connman_notifier *notifier)
{
	DBG("notifier %p name %s", notifier, notifier->name);

	notifier_list = g_slist_insert_sorted(notifier_list, notifier,
							compare_priority);

	return 0;
}

/**
 * connman_notifier_unregister:
 * @notifier: notifier module
 *
 * Remove a previously registered notifier module
 */
void connman_notifier_unregister(struct connman_notifier *notifier)
{
	DBG("notifier %p name %s", notifier, notifier->name);

	notifier_list = g_slist_remove(notifier_list, notifier);
}

#define MAX_TECHNOLOGIES 10

static volatile gint registered[MAX_TECHNOLOGIES];
static volatile gint enabled[MAX_TECHNOLOGIES];
static volatile gint connected[MAX_TECHNOLOGIES];

void __connman_notifier_list_registered(DBusMessageIter *iter)
{
	int i;

	for (i = 0; i < 10; i++) {
		const char *type = __connman_service_type2string(i);

		if (type == NULL)
			continue;

		if (g_atomic_int_get(&registered[i]) > 0)
			dbus_message_iter_append_basic(iter,
						DBUS_TYPE_STRING, &type);
	}
}

void __connman_notifier_list_enabled(DBusMessageIter *iter)
{
	int i;

	for (i = 0; i < 10; i++) {
		const char *type = __connman_service_type2string(i);

		if (type == NULL)
			continue;

		if (g_atomic_int_get(&enabled[i]) > 0)
			dbus_message_iter_append_basic(iter,
						DBUS_TYPE_STRING, &type);
	}
}

void __connman_notifier_list_connected(DBusMessageIter *iter)
{
	int i;

	for (i = 0; i < 10; i++) {
		const char *type = __connman_service_type2string(i);

		if (type == NULL)
			continue;

		if (g_atomic_int_get(&connected[i]) > 0)
			dbus_message_iter_append_basic(iter,
						DBUS_TYPE_STRING, &type);
	}
}

static void technology_registered(enum connman_service_type type,
						connman_bool_t registered)
{
	DBusMessage *signal;
	DBusMessageIter iter;

	DBG("type %d registered %d", type, registered);

	signal = dbus_message_new_signal(CONNMAN_MANAGER_PATH,
				CONNMAN_MANAGER_INTERFACE, "PropertyChanged");
	if (signal == NULL)
		return;

	dbus_message_iter_init_append(signal, &iter);
	connman_dbus_property_append_variable_array(&iter,
				"AvailableTechnologies", DBUS_TYPE_STRING,
					__connman_notifier_list_registered);

	g_dbus_send_message(connection, signal);
}

static void technology_enabled(enum connman_service_type type,
						connman_bool_t enabled)
{
	DBusMessage *signal;
	DBusMessageIter iter;
	GSList *list;

	DBG("type %d enabled %d", type, enabled);

	signal = dbus_message_new_signal(CONNMAN_MANAGER_PATH,
				CONNMAN_MANAGER_INTERFACE, "PropertyChanged");
	if (signal == NULL)
		goto done;

	dbus_message_iter_init_append(signal, &iter);
	connman_dbus_property_append_variable_array(&iter,
				"EnabledTechnologies", DBUS_TYPE_STRING,
					__connman_notifier_list_enabled);

	g_dbus_send_message(connection, signal);

done:
	for (list = notifier_list; list; list = list->next) {
		struct connman_notifier *notifier = list->data;

		if (notifier->service_enabled)
			notifier->service_enabled(type, enabled);
	}
}

static void technology_connected(enum connman_service_type type,
						connman_bool_t connected)
{
	DBusMessage *signal;
	DBusMessageIter iter;

	DBG("type %d connected %d", type, connected);

	signal = dbus_message_new_signal(CONNMAN_MANAGER_PATH,
				CONNMAN_MANAGER_INTERFACE, "PropertyChanged");
	if (signal == NULL)
		return;

	dbus_message_iter_init_append(signal, &iter);
	connman_dbus_property_append_variable_array(&iter,
				"ConnectedTechnologies", DBUS_TYPE_STRING,
					__connman_notifier_list_connected);

	g_dbus_send_message(connection, signal);
}

void __connman_notifier_register(enum connman_service_type type)
{
	DBG("type %d", type);

	switch (type) {
	case CONNMAN_SERVICE_TYPE_UNKNOWN:
	case CONNMAN_SERVICE_TYPE_SYSTEM:
	case CONNMAN_SERVICE_TYPE_VPN:
		return;
	case CONNMAN_SERVICE_TYPE_ETHERNET:
	case CONNMAN_SERVICE_TYPE_WIFI:
	case CONNMAN_SERVICE_TYPE_WIMAX:
	case CONNMAN_SERVICE_TYPE_BLUETOOTH:
	case CONNMAN_SERVICE_TYPE_CELLULAR:
		break;
	}

	if (g_atomic_int_exchange_and_add(&registered[type], 1) == 0)
		technology_registered(type, TRUE);
}

void __connman_notifier_unregister(enum connman_service_type type)
{
	DBG("type %d", type);

	switch (type) {
	case CONNMAN_SERVICE_TYPE_UNKNOWN:
	case CONNMAN_SERVICE_TYPE_SYSTEM:
	case CONNMAN_SERVICE_TYPE_VPN:
		return;
	case CONNMAN_SERVICE_TYPE_ETHERNET:
	case CONNMAN_SERVICE_TYPE_WIFI:
	case CONNMAN_SERVICE_TYPE_WIMAX:
	case CONNMAN_SERVICE_TYPE_BLUETOOTH:
	case CONNMAN_SERVICE_TYPE_CELLULAR:
		break;
	}

	if (g_atomic_int_dec_and_test(&registered[type]) == TRUE)
		technology_registered(type, FALSE);
}

void __connman_notifier_enable(enum connman_service_type type)
{
	DBG("type %d", type);

	switch (type) {
	case CONNMAN_SERVICE_TYPE_UNKNOWN:
	case CONNMAN_SERVICE_TYPE_SYSTEM:
	case CONNMAN_SERVICE_TYPE_VPN:
		return;
	case CONNMAN_SERVICE_TYPE_ETHERNET:
	case CONNMAN_SERVICE_TYPE_WIFI:
	case CONNMAN_SERVICE_TYPE_WIMAX:
	case CONNMAN_SERVICE_TYPE_BLUETOOTH:
	case CONNMAN_SERVICE_TYPE_CELLULAR:
		break;
	}

	if (g_atomic_int_exchange_and_add(&enabled[type], 1) == 0)
		technology_enabled(type, TRUE);
}

void __connman_notifier_disable(enum connman_service_type type)
{
	DBG("type %d", type);

	switch (type) {
	case CONNMAN_SERVICE_TYPE_UNKNOWN:
	case CONNMAN_SERVICE_TYPE_SYSTEM:
	case CONNMAN_SERVICE_TYPE_VPN:
		return;
	case CONNMAN_SERVICE_TYPE_ETHERNET:
	case CONNMAN_SERVICE_TYPE_WIFI:
	case CONNMAN_SERVICE_TYPE_WIMAX:
	case CONNMAN_SERVICE_TYPE_BLUETOOTH:
	case CONNMAN_SERVICE_TYPE_CELLULAR:
		break;
	}

	if (g_atomic_int_dec_and_test(&enabled[type]) == TRUE)
		technology_enabled(type, FALSE);
}

void __connman_notifier_connect(enum connman_service_type type)
{
	DBG("type %d", type);

	switch (type) {
	case CONNMAN_SERVICE_TYPE_UNKNOWN:
	case CONNMAN_SERVICE_TYPE_SYSTEM:
	case CONNMAN_SERVICE_TYPE_VPN:
		return;
	case CONNMAN_SERVICE_TYPE_ETHERNET:
	case CONNMAN_SERVICE_TYPE_WIFI:
	case CONNMAN_SERVICE_TYPE_WIMAX:
	case CONNMAN_SERVICE_TYPE_BLUETOOTH:
	case CONNMAN_SERVICE_TYPE_CELLULAR:
		break;
	}

	if (g_atomic_int_exchange_and_add(&connected[type], 1) == 0)
		technology_connected(type, TRUE);
}

void __connman_notifier_disconnect(enum connman_service_type type)
{
	DBG("type %d", type);

	switch (type) {
	case CONNMAN_SERVICE_TYPE_UNKNOWN:
	case CONNMAN_SERVICE_TYPE_SYSTEM:
	case CONNMAN_SERVICE_TYPE_VPN:
		return;
	case CONNMAN_SERVICE_TYPE_ETHERNET:
	case CONNMAN_SERVICE_TYPE_WIFI:
	case CONNMAN_SERVICE_TYPE_WIMAX:
	case CONNMAN_SERVICE_TYPE_BLUETOOTH:
	case CONNMAN_SERVICE_TYPE_CELLULAR:
		break;
	}

	if (g_atomic_int_dec_and_test(&connected[type]) == TRUE)
		technology_connected(type, FALSE);
}

static void technology_default(enum connman_service_type type)
{
	DBusMessage *signal;
	DBusMessageIter iter;
	const char *str;

	str = __connman_service_type2string(type);
	if (str == NULL)
		str = "";

	signal = dbus_message_new_signal(CONNMAN_MANAGER_PATH,
				CONNMAN_MANAGER_INTERFACE, "PropertyChanged");
	if (signal == NULL)
		return;

	dbus_message_iter_init_append(signal, &iter);
	connman_dbus_property_append_variant(&iter, "DefaultTechnology",
						DBUS_TYPE_STRING, &str);

	g_dbus_send_message(connection, signal);
}

void __connman_notifier_default_changed(struct connman_service *service)
{
	enum connman_service_type type = connman_service_get_type(service);
	GSList *list;

	technology_default(type);

	for (list = notifier_list; list; list = list->next) {
		struct connman_notifier *notifier = list->data;

		if (notifier->default_changed)
			notifier->default_changed(service);
	}
}

static void offlinemode_changed(dbus_bool_t enabled)
{
	DBusMessage *signal;
	DBusMessageIter iter;

	DBG("enabled %d", enabled);

	signal = dbus_message_new_signal(CONNMAN_MANAGER_PATH,
				CONNMAN_MANAGER_INTERFACE, "PropertyChanged");
	if (signal == NULL)
		return;

	dbus_message_iter_init_append(signal, &iter);
	connman_dbus_property_append_variant(&iter, "OfflineMode",
					DBUS_TYPE_BOOLEAN, &enabled);

	g_dbus_send_message(connection, signal);
}

void __connman_notifier_offlinemode(connman_bool_t enabled)
{
	GSList *list;

	DBG("enabled %d", enabled);

	__connman_profile_changed(FALSE);

	offlinemode_changed(enabled);

	for (list = notifier_list; list; list = list->next) {
		struct connman_notifier *notifier = list->data;

		if (notifier->offline_mode)
			notifier->offline_mode(enabled);
	}
}

connman_bool_t __connman_notifier_is_enabled(enum connman_service_type type)
{
	DBG("type %d", type);

	switch (type) {
	case CONNMAN_SERVICE_TYPE_UNKNOWN:
	case CONNMAN_SERVICE_TYPE_SYSTEM:
	case CONNMAN_SERVICE_TYPE_VPN:
		return FALSE;
	case CONNMAN_SERVICE_TYPE_ETHERNET:
	case CONNMAN_SERVICE_TYPE_WIFI:
	case CONNMAN_SERVICE_TYPE_WIMAX:
	case CONNMAN_SERVICE_TYPE_BLUETOOTH:
	case CONNMAN_SERVICE_TYPE_CELLULAR:
		break;
	}

	if (g_atomic_int_get(&enabled[type]) > 0)
		return TRUE;

	return FALSE;
}

int __connman_notifier_init(void)
{
	DBG("");

	connection = connman_dbus_get_connection();

	return 0;
}

void __connman_notifier_cleanup(void)
{
	DBG("");

	dbus_connection_unref(connection);
}
