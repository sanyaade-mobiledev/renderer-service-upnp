/*
 * renderer-service-upnp
 *
 * Copyright (C) 2012 Intel Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU Lesser General Public License,
 * version 2.1, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Mark Ryan <mark.d.ryan@intel.com>
 *
 */


#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <sys/signalfd.h>

#include "renderer-service-upnp.h"
#include "async.h"
#include "device.h"
#include "error.h"
#include "log.h"
#include "prop-defs.h"
#include "settings.h"
#include "task-processor.h"
#include "upnp.h"

#ifdef UA_PREFIX
	#define PRG_NAME UA_PREFIX " dLeyna/" VERSION
#else
	#define PRG_NAME "dLeyna/" VERSION
#endif

#define RSU_INTERFACE_GET_VERSION "GetVersion"
#define RSU_INTERFACE_GET_SERVERS "GetServers"
#define RSU_INTERFACE_RELEASE "Release"

#define RSU_INTERFACE_FOUND_SERVER "FoundServer"
#define RSU_INTERFACE_LOST_SERVER "LostServer"

#define RSU_INTERFACE_HOST_FILE "HostFile"
#define RSU_INTERFACE_REMOVE_FILE "RemoveFile"

#define RSU_INTERFACE_VERSION "Version"
#define RSU_INTERFACE_SERVERS "Servers"

#define RSU_INTERFACE_PATH "Path"
#define RSU_INTERFACE_URI "Uri"
#define RSU_INTERFACE_ID "Id"

#define RSU_INTERFACE_CHANGED_PROPERTIES "changed_properties"
#define RSU_INTERFACE_INVALIDATED_PROPERTIES "invalidated_properties"
#define RSU_INTERFACE_GET "Get"
#define RSU_INTERFACE_GET_ALL "GetAll"
#define RSU_INTERFACE_SET "Set"
#define RSU_INTERFACE_INTERFACE_NAME "interface_name"
#define RSU_INTERFACE_PROPERTY_NAME "property_name"
#define RSU_INTERFACE_PROPERTIES_VALUE "properties"
#define RSU_INTERFACE_VALUE "value"
#define RSU_INTERFACE_OFFSET "offset"
#define RSU_INTERFACE_POSITION "position"
#define RSU_INTERFACE_TRACKID "trackid"
#define RSU_INTERFACE_TRACK_NUMBER "TrackNumber"

#define RSU_INTERFACE_RAISE "Raise"
#define RSU_INTERFACE_QUIT "Quit"
#define RSU_INTERFACE_PLAY "Play"
#define RSU_INTERFACE_PLAY_PAUSE "PlayPause"
#define RSU_INTERFACE_NEXT "Next"
#define RSU_INTERFACE_PREVIOUS "Previous"
#define RSU_INTERFACE_PAUSE "Pause"
#define RSU_INTERFACE_STOP "Stop"
#define RSU_INTERFACE_OPEN_URI "OpenUri"
#define RSU_INTERFACE_SEEK "Seek"
#define RSU_INTERFACE_SET_POSITION "SetPosition"
#define RSU_INTERFACE_GOTO_TRACK "GotoTrack"

#define RSU_INTERFACE_CANCEL "Cancel"

typedef struct rsu_context_t_ rsu_context_t;
struct rsu_context_t_ {
	bool error;
	guint rsu_id;
	guint sig_id;
	guint owner_id;
	GDBusNodeInfo *root_node_info;
	GDBusNodeInfo *server_node_info;
	GMainLoop *main_loop;
	GDBusConnection *connection;
	GHashTable *watchers;
	rsu_task_processor_t *processor;
	rsu_upnp_t *upnp;
	rsu_settings_context_t *settings;
};

static rsu_context_t g_context;

static const gchar g_rsu_root_introspection[] =
	"<node>"
	"  <interface name='"RSU_INTERFACE_MANAGER"'>"
	"    <method name='"RSU_INTERFACE_GET_VERSION"'>"
	"      <arg type='s' name='"RSU_INTERFACE_VERSION"'"
	"           direction='out'/>"
	"    </method>"
	"    <method name='"RSU_INTERFACE_RELEASE"'>"
	"    </method>"
	"    <method name='"RSU_INTERFACE_GET_SERVERS"'>"
	"      <arg type='as' name='"RSU_INTERFACE_SERVERS"'"
	"           direction='out'/>"
	"    </method>"
	"    <signal name='"RSU_INTERFACE_FOUND_SERVER"'>"
	"      <arg type='s' name='"RSU_INTERFACE_PATH"'/>"
	"    </signal>"
	"    <signal name='"RSU_INTERFACE_LOST_SERVER"'>"
	"      <arg type='s' name='"RSU_INTERFACE_PATH"'/>"
	"    </signal>"
	"  </interface>"
	"</node>";

static const gchar g_rsu_server_introspection[] =
	"<node>"
	"  <interface name='"RSU_INTERFACE_PROPERTIES"'>"
	"    <method name='"RSU_INTERFACE_GET"'>"
	"      <arg type='s' name='"RSU_INTERFACE_INTERFACE_NAME"'"
	"           direction='in'/>"
	"      <arg type='s' name='"RSU_INTERFACE_PROPERTY_NAME"'"
	"           direction='in'/>"
	"      <arg type='v' name='"RSU_INTERFACE_VALUE"'"
	"           direction='out'/>"
	"    </method>"
	"    <method name='"RSU_INTERFACE_GET_ALL"'>"
	"      <arg type='s' name='"RSU_INTERFACE_INTERFACE_NAME"'"
	"           direction='in'/>"
	"      <arg type='a{sv}' name='"RSU_INTERFACE_PROPERTIES_VALUE"'"
	"           direction='out'/>"
	"    </method>"
	"    <method name='"RSU_INTERFACE_SET"'>"
	"      <arg type='s' name='"RSU_INTERFACE_INTERFACE_NAME"'"
	"           direction='in'/>"
	"      <arg type='s' name='"RSU_INTERFACE_PROPERTY_NAME"'"
	"           direction='in'/>"
	"      <arg type='v' name='"RSU_INTERFACE_VALUE"'"
	"           direction='in'/>"
	"    </method>"
	"    <signal name='"RSU_INTERFACE_PROPERTIES_CHANGED"'>"
	"      <arg type='s' name='"RSU_INTERFACE_INTERFACE_NAME"'/>"
	"      <arg type='a{sv}' name='"RSU_INTERFACE_CHANGED_PROPERTIES"'/>"
	"      <arg type='as' name='"RSU_INTERFACE_INVALIDATED_PROPERTIES"'/>"
	"    </signal>"
	"  </interface>"
	"  <interface name='"RSU_INTERFACE_SERVER"'>"
	"    <method name='"RSU_INTERFACE_RAISE"'>"
	"    </method>"
	"    <method name='"RSU_INTERFACE_QUIT"'>"
	"    </method>"
	"    <property type='b' name='"RSU_INTERFACE_PROP_CAN_QUIT"'"
	"       access='read'/>"
	"    <property type='b' name='"RSU_INTERFACE_PROP_CAN_RAISE"'"
	"       access='read'/>"
	"    <property type='b' name='"RSU_INTERFACE_PROP_CAN_SET_FULLSCREEN"'"
	"       access='read'/>"
	"    <property type='b' name='"RSU_INTERFACE_PROP_HAS_TRACK_LIST"'"
	"       access='read'/>"
	"    <property type='s' name='"RSU_INTERFACE_PROP_IDENTITY"'"
	"       access='read'/>"
	"    <property type='as' name='"RSU_INTERFACE_PROP_SUPPORTED_URIS"'"
	"       access='read'/>"
	"    <property type='as' name='"RSU_INTERFACE_PROP_SUPPORTED_MIME"'"
	"       access='read'/>"
	"  </interface>"
	"  <interface name='"RSU_INTERFACE_PLAYER"'>"
	"    <method name='"RSU_INTERFACE_PLAY"'>"
	"    </method>"
	"    <method name='"RSU_INTERFACE_PAUSE"'>"
	"    </method>"
	"    <method name='"RSU_INTERFACE_PLAY_PAUSE"'>"
	"    </method>"
	"    <method name='"RSU_INTERFACE_STOP"'>"
	"    </method>"
	"    <method name='"RSU_INTERFACE_NEXT"'>"
	"    </method>"
	"    <method name='"RSU_INTERFACE_PREVIOUS"'>"
	"    </method>"
	"    <method name='"RSU_INTERFACE_OPEN_URI"'>"
	"      <arg type='s' name='"RSU_INTERFACE_URI"'"
	"           direction='in'/>"
	"    </method>"
	"    <method name='"RSU_INTERFACE_SEEK"'>"
	"      <arg type='x' name='"RSU_INTERFACE_OFFSET"'"
	"           direction='in'/>"
	"    </method>"
	"    <method name='"RSU_INTERFACE_SET_POSITION"'>"
	"      <arg type='o' name='"RSU_INTERFACE_TRACKID"'"
	"           direction='in'/>"
	"      <arg type='x' name='"RSU_INTERFACE_POSITION"'"
	"           direction='in'/>"
	"    </method>"
	"    <method name='"RSU_INTERFACE_GOTO_TRACK"'>"
	"      <arg type='u' name='"RSU_INTERFACE_TRACK_NUMBER"'"
	"           direction='in'/>"
	"    </method>"
	"    <property type='s' name='"RSU_INTERFACE_PROP_PLAYBACK_STATUS"'"
	"       access='read'/>"
	"    <property type='d' name='"RSU_INTERFACE_PROP_RATE"'"
	"       access='readwrite'/>"
	"    <property type='d' name='"RSU_INTERFACE_PROP_MINIMUM_RATE"'"
	"       access='read'/>"
	"    <property type='d' name='"RSU_INTERFACE_PROP_MAXIMUM_RATE"'"
	"       access='read'/>"
	"    <property type='ad'"
	"       name='"RSU_INTERFACE_PROP_TRANSPORT_PLAY_SPEEDS"'"
	"       access='read'/>"
	"    <property type='d' name='"RSU_INTERFACE_PROP_VOLUME"'"
	"       access='readwrite'/>"
	"    <property type='b' name='"RSU_INTERFACE_PROP_CAN_PLAY"'"
	"       access='read'/>"
	"    <property type='b' name='"RSU_INTERFACE_PROP_CAN_SEEK"'"
	"       access='read'/>"
	"    <property type='b' name='"RSU_INTERFACE_PROP_CAN_CONTROL"'"
	"       access='read'/>"
	"    <property type='b' name='"RSU_INTERFACE_PROP_CAN_PAUSE"'"
	"       access='read'/>"
	"    <property type='b' name='"RSU_INTERFACE_PROP_CAN_NEXT"'"
	"       access='read'/>"
	"    <property type='b' name='"RSU_INTERFACE_PROP_CAN_PREVIOUS"'"
	"       access='read'/>"
	"    <property type='x' name='"RSU_INTERFACE_PROP_POSITION"'"
	"       access='read'/>"
	"    <property type='a{sv}' name='"RSU_INTERFACE_PROP_METADATA"'"
	"       access='read'/>"
	"    <property type='u' name='"RSU_INTERFACE_PROP_CURRENT_TRACK"'"
	"       access='read'/>"
	"    <property type='u' name='"RSU_INTERFACE_PROP_NUMBER_OF_TRACKS"'"
	"       access='read'/>"
	"  </interface>"
	"  <interface name='"RSU_INTERFACE_PUSH_HOST"'>"
	"    <method name='"RSU_INTERFACE_HOST_FILE"'>"
	"      <arg type='s' name='"RSU_INTERFACE_PATH"'"
	"           direction='in'/>"
	"      <arg type='s' name='"RSU_INTERFACE_URI"'"
	"           direction='out'/>"
	"    </method>"
	"    <method name='"RSU_INTERFACE_REMOVE_FILE"'>"
	"      <arg type='s' name='"RSU_INTERFACE_PATH"'"
	"           direction='in'/>"
	"    </method>"
	"  </interface>"
	"  <interface name='"RSU_INTERFACE_RENDERER_DEVICE"'>"
	"    <method name='"RSU_INTERFACE_CANCEL"'>"
	"    </method>"
	"    <property type='s' name='"RSU_INTERFACE_PROP_DEVICE_TYPE"'"
	"       access='read'/>"
	"    <property type='s' name='"RSU_INTERFACE_PROP_UDN"'"
	"       access='read'/>"
	"    <property type='s' name='"RSU_INTERFACE_PROP_FRIENDLY_NAME"'"
	"       access='read'/>"
	"    <property type='s' name='"RSU_INTERFACE_PROP_ICON_URL"'"
	"       access='read'/>"
	"    <property type='s' name='"RSU_INTERFACE_PROP_MANUFACTURER"'"
	"       access='read'/>"
	"    <property type='s' name='"RSU_INTERFACE_PROP_MANUFACTURER_URL"'"
	"       access='read'/>"
	"    <property type='s' name='"RSU_INTERFACE_PROP_MODEL_DESCRIPTION"'"
	"       access='read'/>"
	"    <property type='s' name='"RSU_INTERFACE_PROP_MODEL_NAME"'"
	"       access='read'/>"
	"    <property type='s' name='"RSU_INTERFACE_PROP_MODEL_NUMBER"'"
	"       access='read'/>"
	"    <property type='s' name='"RSU_INTERFACE_PROP_SERIAL_NUMBER"'"
	"       access='read'/>"
	"    <property type='s' name='"RSU_INTERFACE_PROP_PRESENTATION_URL"'"
	"       access='read'/>"
	"    <property type='s' name='"RSU_INTERFACE_PROP_PROTOCOL_INFO"'"
	"       access='read'/>"
	"  </interface>"
	"</node>";

static void prv_process_task(rsu_task_atom_t *task, gpointer user_data);

static void prv_rsu_method_call(GDBusConnection *conn,
				const gchar *sender,
				const gchar *object,
				const gchar *interface,
				const gchar *method,
				GVariant *parameters,
				GDBusMethodInvocation *invocation,
				gpointer user_data);

static void prv_rsu_device_method_call(GDBusConnection *conn,
				       const gchar *sender,
				       const gchar *object,
				       const gchar *interface,
				       const gchar *method,
				       GVariant *parameters,
				       GDBusMethodInvocation *invocation,
				       gpointer user_data);

static void prv_props_method_call(GDBusConnection *conn,
				  const gchar *sender,
				  const gchar *object,
				  const gchar *interface,
				  const gchar *method,
				  GVariant *parameters,
				  GDBusMethodInvocation *invocation,
				  gpointer user_data);

static void prv_rsu_player_method_call(GDBusConnection *conn,
				       const gchar *sender,
				       const gchar *object,
				       const gchar *interface,
				       const gchar *method,
				       GVariant *parameters,
				       GDBusMethodInvocation *invocation,
				       gpointer user_data);

static void prv_rsu_push_host_method_call(GDBusConnection *conn,
					  const gchar *sender,
					  const gchar *object,
					  const gchar *interface,
					  const gchar *method,
					  GVariant *parameters,
					  GDBusMethodInvocation *invocation,
					  gpointer user_data);

static void prv_renderer_device_method_call(GDBusConnection *conn,
					    const gchar *sender,
					    const gchar *object,
					    const gchar *interface,
					    const gchar *method,
					    GVariant *parameters,
					    GDBusMethodInvocation *invocation,
					    gpointer user_data);

static const GDBusInterfaceVTable g_rsu_vtable = {
	prv_rsu_method_call,
	NULL,
	NULL
};

static const GDBusInterfaceVTable g_props_vtable = {
	prv_props_method_call,
	NULL,
	NULL
};

static const GDBusInterfaceVTable g_rsu_device_vtable = {
	prv_rsu_device_method_call,
	NULL,
	NULL
};

static const GDBusInterfaceVTable g_rsu_player_vtable = {
	prv_rsu_player_method_call,
	NULL,
	NULL
};

static const GDBusInterfaceVTable g_rsu_push_host_vtable = {
	prv_rsu_push_host_method_call,
	NULL,
	NULL
};

static const GDBusInterfaceVTable g_device_vtable = {
	prv_renderer_device_method_call,
	NULL,
	NULL
};

static const GDBusInterfaceVTable *g_server_vtables[RSU_INTERFACE_INFO_MAX] = {
	&g_props_vtable,
	&g_rsu_device_vtable,
	&g_rsu_player_vtable,
	&g_rsu_push_host_vtable,
	&g_device_vtable
};

rsu_task_processor_t *rsu_renderer_service_get_task_processor(void)
{
	return g_context.processor;
}

static gboolean prv_context_mainloop_quit_cb(gpointer user_data)
{
	g_main_loop_quit(g_context.main_loop);

	return FALSE;
}

static gboolean prv_context_quit_cb(gpointer user_data)
{
	RSU_LOG_DEBUG("Quitting");

	if (g_context.owner_id) {
		g_bus_unown_name(g_context.owner_id);
		g_context.owner_id = 0;
	}

	rsu_upnp_unsubscribe(g_context.upnp);

	g_timeout_add_seconds(1, prv_context_mainloop_quit_cb, NULL);

	return FALSE;
}

static void prv_process_sync_task(rsu_task_t *task)
{
	GError *error;

	switch (task->type) {
	case RSU_TASK_GET_VERSION:
		rsu_task_complete(task);
		rsu_task_queue_task_completed(task->atom.queue_id);
		break;
	case RSU_TASK_GET_SERVERS:
		task->result = rsu_upnp_get_server_ids(g_context.upnp);
		rsu_task_complete(task);
		rsu_task_queue_task_completed(task->atom.queue_id);
		break;
	case RSU_TASK_RAISE:
	case RSU_TASK_QUIT:
		error = g_error_new(RSU_ERROR, RSU_ERROR_NOT_SUPPORTED,
				    "Command not supported.");
		rsu_task_fail(task, error);
		rsu_task_queue_task_completed(task->atom.queue_id);
		g_error_free(error);
		break;
	default:
		break;
	}
}

static void prv_async_task_complete(rsu_task_t *task, GError *error)
{
	RSU_LOG_DEBUG("Enter");

	if (error) {
		rsu_task_fail(task, error);
		g_error_free(error);
	} else {
		rsu_task_complete(task);
	}

	rsu_task_queue_task_completed(task->atom.queue_id);

	RSU_LOG_DEBUG("Exit");
}

static void prv_process_async_task(rsu_task_t *task)
{
	rsu_async_task_t *async_task = (rsu_async_task_t *)task;

	RSU_LOG_DEBUG("Enter");

	async_task->cancellable = g_cancellable_new();

	switch (task->type) {
	case RSU_TASK_GET_PROP:
		rsu_upnp_get_prop(g_context.upnp, task,
				  prv_async_task_complete);
		break;
	case RSU_TASK_GET_ALL_PROPS:
		rsu_upnp_get_all_props(g_context.upnp, task,
				       prv_async_task_complete);
		break;
	case RSU_TASK_SET_PROP:
		rsu_upnp_set_prop(g_context.upnp, task,
				  prv_async_task_complete);
		break;
	case RSU_TASK_PLAY:
		rsu_upnp_play(g_context.upnp, task,
			      prv_async_task_complete);
		break;
	case RSU_TASK_PAUSE:
		rsu_upnp_pause(g_context.upnp, task,
			       prv_async_task_complete);
		break;
	case RSU_TASK_PLAY_PAUSE:
		rsu_upnp_play_pause(g_context.upnp, task,
				    prv_async_task_complete);
		break;
	case RSU_TASK_STOP:
		rsu_upnp_stop(g_context.upnp, task,
			      prv_async_task_complete);
		break;
	case RSU_TASK_NEXT:
		rsu_upnp_next(g_context.upnp, task,
			      prv_async_task_complete);
		break;
	case RSU_TASK_PREVIOUS:
		rsu_upnp_previous(g_context.upnp, task,
				  prv_async_task_complete);
		break;
	case RSU_TASK_OPEN_URI:
		rsu_upnp_open_uri(g_context.upnp, task,
				  prv_async_task_complete);
		break;
	case RSU_TASK_SEEK:
		rsu_upnp_seek(g_context.upnp, task,
			      prv_async_task_complete);
		break;
	case RSU_TASK_SET_POSITION:
		rsu_upnp_set_position(g_context.upnp, task,
				      prv_async_task_complete);
		break;
	case RSU_TASK_GOTO_TRACK:
		rsu_upnp_goto_track(g_context.upnp, task,
				    prv_async_task_complete);
		break;
	case RSU_TASK_HOST_URI:
		rsu_upnp_host_uri(g_context.upnp, task,
				  prv_async_task_complete);
		break;
	case RSU_TASK_REMOVE_URI:
		rsu_upnp_remove_uri(g_context.upnp, task,
				    prv_async_task_complete);
		break;
	default:
		break;
	}

	RSU_LOG_DEBUG("Exit");
}

static void prv_process_task(rsu_task_atom_t *task, gpointer user_data)
{
	rsu_task_t *client_task = (rsu_task_t *)task;

	if (client_task->synchronous)
		prv_process_sync_task(client_task);
	else
		prv_process_async_task(client_task);
}

static void prv_cancel_task(rsu_task_atom_t *task, gpointer user_data)
{
	rsu_task_cancel((rsu_task_t *)task);
}

static void prv_delete_task(rsu_task_atom_t *task, gpointer user_data)
{
	rsu_task_delete((rsu_task_t *)task);
}

static void prv_rsu_context_init(void)
{
	memset(&g_context, 0, sizeof(g_context));
	g_context.processor = rsu_task_processor_new(prv_context_quit_cb);
}

static void prv_rsu_context_free(void)
{
	if (g_context.upnp)
		rsu_upnp_delete(g_context.upnp);

	if (g_context.watchers)
		g_hash_table_unref(g_context.watchers);

	rsu_task_processor_free(g_context.processor);

	if (g_context.sig_id)
		(void) g_source_remove(g_context.sig_id);

	if (g_context.connection) {
		if (g_context.rsu_id)
			g_dbus_connection_unregister_object(
							g_context.connection,
							g_context.rsu_id);
	}

	if (g_context.main_loop)
		g_main_loop_unref(g_context.main_loop);

	if (g_context.owner_id)
		g_bus_unown_name(g_context.owner_id);

	if (g_context.connection)
		g_object_unref(g_context.connection);

	if (g_context.server_node_info)
		g_dbus_node_info_unref(g_context.server_node_info);

	if (g_context.root_node_info)
		g_dbus_node_info_unref(g_context.root_node_info);

	if (g_context.settings)
		rsu_settings_delete(g_context.settings);
}

static void prv_remove_client(const gchar *name)
{
	rsu_task_processor_remove_queues_for_source(g_context.processor, name);

	rsu_upnp_lost_client(g_context.upnp, name);

	(void) g_hash_table_remove(g_context.watchers, name);
	if (g_hash_table_size(g_context.watchers) == 0)
		if (!rsu_settings_is_never_quit(g_context.settings))
			rsu_task_processor_set_quitting(g_context.processor);
}

static void prv_lost_client(GDBusConnection *connection, const gchar *name,
			    gpointer user_data)
{
	RSU_LOG_INFO("Client %s lost", name);
	prv_remove_client(name);
}

static void prv_add_task(rsu_task_t *task, const gchar *sink)
{
	const gchar *client_name;
	guint watcher_id;
	const rsu_task_queue_key_t *queue_id;

	client_name = g_dbus_method_invocation_get_sender(task->invocation);

	if (!g_hash_table_lookup(g_context.watchers, client_name)) {
		watcher_id = g_bus_watch_name(G_BUS_TYPE_SESSION, client_name,
					      G_BUS_NAME_WATCHER_FLAGS_NONE,
					      NULL, prv_lost_client, NULL,
					      NULL);

		g_hash_table_insert(g_context.watchers, g_strdup(client_name),
				    GUINT_TO_POINTER(watcher_id));
	}

	queue_id = rsu_task_processor_lookup_queue(g_context.processor,
						   client_name, sink);
	if (!queue_id)
		queue_id = rsu_task_processor_add_queue(
						g_context.processor,
						client_name,
						sink,
						RSU_TASK_QUEUE_FLAG_AUTO_START,
						prv_process_task,
						prv_cancel_task,
						prv_delete_task);

	rsu_task_queue_add_task(queue_id, &task->atom);
}

static void prv_rsu_method_call(GDBusConnection *conn,
				const gchar *sender, const gchar *object,
				const gchar *interface,
				const gchar *method, GVariant *parameters,
				GDBusMethodInvocation *invocation,
				gpointer user_data)
{
	const gchar *client_name;
	rsu_task_t *task;

	RSU_LOG_INFO("Calling %s method", method);

	if (!strcmp(method, RSU_INTERFACE_RELEASE)) {
		client_name = g_dbus_method_invocation_get_sender(invocation);
		prv_remove_client(client_name);
		g_dbus_method_invocation_return_value(invocation, NULL);
	} else {
		if (!strcmp(method, RSU_INTERFACE_GET_VERSION))
			task = rsu_task_get_version_new(invocation);
		else if (!strcmp(method, RSU_INTERFACE_GET_SERVERS))
			task = rsu_task_get_servers_new(invocation);
		else
			goto finished;

		prv_add_task(task, RSU_SINK);
	}

finished:

	return;
}

static const gchar *prv_get_device_id(const gchar *object, GError **error)
{
	rsu_device_t *device;

	device = rsu_device_from_path(object,
				rsu_upnp_get_server_udn_map(g_context.upnp));


	if (!device) {
		RSU_LOG_WARNING("Cannot locate device for %s", object);

		*error = g_error_new(RSU_ERROR, RSU_ERROR_OBJECT_NOT_FOUND,
				     "Cannot locate device corresponding to"
				     " the specified path");
		goto on_error;
	}

	return device->path;

on_error:

	return NULL;
}

static void prv_props_method_call(GDBusConnection *conn,
				  const gchar *sender,
				  const gchar *object,
				  const gchar *interface,
				  const gchar *method,
				  GVariant *parameters,
				  GDBusMethodInvocation *invocation,
				  gpointer user_data)
{
	rsu_task_t *task;
	const gchar *device_id;
	GError *error = NULL;

	device_id = prv_get_device_id(object, &error);
	if (!device_id) {
		g_dbus_method_invocation_return_gerror(invocation, error);
		g_error_free(error);

		goto finished;
	}

	if (!strcmp(method, RSU_INTERFACE_GET_ALL))
		task = rsu_task_get_props_new(invocation, object, parameters);
	else if (!strcmp(method, RSU_INTERFACE_GET))
		task = rsu_task_get_prop_new(invocation, object, parameters);
	else if (!strcmp(method, RSU_INTERFACE_SET))
		task = rsu_task_set_prop_new(invocation, object, parameters);
	else
		goto finished;

	prv_add_task(task, device_id);

finished:

	return;
}

static void prv_rsu_device_method_call(GDBusConnection *conn,
				       const gchar *sender,
				       const gchar *object,
				       const gchar *interface,
				       const gchar *method,
				       GVariant *parameters,
				       GDBusMethodInvocation *invocation,
				       gpointer user_data)

{
	rsu_task_t *task;
	const gchar *device_id;
	GError *error = NULL;

	device_id = prv_get_device_id(object, &error);
	if (!device_id) {
		g_dbus_method_invocation_return_gerror(invocation, error);
		g_error_free(error);

		goto finished;
	}

	if (!strcmp(method, RSU_INTERFACE_RAISE))
		task = rsu_task_raise_new(invocation);
	else if (!strcmp(method, RSU_INTERFACE_QUIT))
		task = rsu_task_quit_new(invocation);
	else
		goto finished;

	prv_add_task(task, device_id);

finished:

	return;
}

static void prv_rsu_player_method_call(GDBusConnection *conn,
				       const gchar *sender,
				       const gchar *object,
				       const gchar *interface,
				       const gchar *method,
				       GVariant *parameters,
				       GDBusMethodInvocation *invocation,
				       gpointer user_data)

{
	rsu_task_t *task;
	const gchar *device_id;
	GError *error = NULL;

	device_id = prv_get_device_id(object, &error);
	if (!device_id) {
		g_dbus_method_invocation_return_gerror(invocation, error);
		g_error_free(error);

		goto finished;
	}

	if (!strcmp(method, RSU_INTERFACE_PLAY))
		task = rsu_task_play_new(invocation, object);
	else if (!strcmp(method, RSU_INTERFACE_PAUSE))
		task = rsu_task_pause_new(invocation, object);
	else if (!strcmp(method, RSU_INTERFACE_PLAY_PAUSE))
		task = rsu_task_play_pause_new(invocation, object);
	else if (!strcmp(method, RSU_INTERFACE_STOP))
		task = rsu_task_stop_new(invocation, object);
	else if (!strcmp(method, RSU_INTERFACE_NEXT))
		task = rsu_task_next_new(invocation, object);
	else if (!strcmp(method, RSU_INTERFACE_PREVIOUS))
		task = rsu_task_previous_new(invocation, object);
	else if (!strcmp(method, RSU_INTERFACE_OPEN_URI))
		task = rsu_task_open_uri_new(invocation, object, parameters);
	else if (!strcmp(method, RSU_INTERFACE_SEEK))
		task = rsu_task_seek_new(invocation, object, parameters);
	else if (!strcmp(method, RSU_INTERFACE_SET_POSITION))
		task = rsu_task_set_position_new(invocation, object,
						 parameters);
	else if (!strcmp(method, RSU_INTERFACE_GOTO_TRACK))
		task = rsu_task_goto_track_new(invocation, object, parameters);
	else
		goto finished;

	prv_add_task(task, device_id);

finished:

	return;
}

static void prv_rsu_push_host_method_call(GDBusConnection *conn,
					  const gchar *sender,
					  const gchar *object,
					  const gchar *interface,
					  const gchar *method,
					  GVariant *parameters,
					  GDBusMethodInvocation *invocation,
					  gpointer user_data)
{
	rsu_task_t *task;
	const gchar *device_id;
	GError *error = NULL;

	device_id = prv_get_device_id(object, &error);
	if (!device_id) {
		g_dbus_method_invocation_return_gerror(invocation, error);
		g_error_free(error);

		goto on_error;
	}

	if (!strcmp(method, RSU_INTERFACE_HOST_FILE))
		task = rsu_task_host_uri_new(invocation, object, parameters);
	else if (!strcmp(method, RSU_INTERFACE_REMOVE_FILE))
		task = rsu_task_remove_uri_new(invocation, object, parameters);
	else
		goto on_error;

	prv_add_task(task, device_id);

on_error:

	return;
}

static void prv_renderer_device_method_call(GDBusConnection *conn,
					    const gchar *sender,
					    const gchar *object,
					    const gchar *interface,
					    const gchar *method,
					    GVariant *parameters,
					    GDBusMethodInvocation *invocation,
					    gpointer user_data)
{
	const gchar *device_id = NULL;
	GError *error = NULL;
	const gchar *client_name;
	const rsu_task_queue_key_t *queue_id;

	device_id = prv_get_device_id(object, &error);
	if (!device_id) {
		g_dbus_method_invocation_return_gerror(invocation, error);
		g_error_free(error);

		goto finished;
	}

	if (!strcmp(method, RSU_INTERFACE_CANCEL)) {
		client_name = g_dbus_method_invocation_get_sender(invocation);

		queue_id = rsu_task_processor_lookup_queue(g_context.processor,
							client_name, device_id);
		if (queue_id)
			rsu_task_processor_cancel_queue(queue_id);

		g_dbus_method_invocation_return_value(invocation, NULL);
	}

finished:

		return;
}

static void prv_found_media_server(const gchar *path)
{
	RSU_LOG_INFO("New media server %s", path);

	(void) g_dbus_connection_emit_signal(g_context.connection,
					     NULL,
					     RSU_OBJECT,
					     RSU_INTERFACE_MANAGER,
					     RSU_INTERFACE_FOUND_SERVER,
					     g_variant_new("(s)", path),
					     NULL);
}

static void prv_lost_media_server(const gchar *path)
{
	RSU_LOG_INFO("Lost %s", path);

	(void) g_dbus_connection_emit_signal(g_context.connection,
					     NULL,
					     RSU_OBJECT,
					     RSU_INTERFACE_MANAGER,
					     RSU_INTERFACE_LOST_SERVER,
					     g_variant_new("(s)", path),
					     NULL);

	rsu_task_processor_remove_queues_for_sink(g_context.processor, path);
}

static void prv_bus_acquired(GDBusConnection *connection, const gchar *name,
			     gpointer user_data)
{
	unsigned int i;
	rsu_interface_info_t *info;

	g_context.connection = connection;

	g_context.rsu_id =
		g_dbus_connection_register_object(connection, RSU_OBJECT,
						  g_context.root_node_info->
						  interfaces[0],
						  &g_rsu_vtable,
						  NULL, NULL, NULL);

	if (!g_context.rsu_id) {
		RSU_LOG_DEBUG("Failed to acquire Bus %s", name);
		g_context.error = true;
		g_main_loop_quit(g_context.main_loop);
	} else {
		RSU_LOG_INFO("Bus %s acquiered", name);
		info = g_new0(rsu_interface_info_t, RSU_INTERFACE_INFO_MAX);

		for (i = 0; i < RSU_INTERFACE_INFO_MAX; ++i) {
			info[i].interface =
				g_context.server_node_info->interfaces[i];
			info[i].vtable = g_server_vtables[i];
		}

		g_context.upnp = rsu_upnp_new(connection, info,
					      prv_found_media_server,
					      prv_lost_media_server);
	}
}

static void prv_name_lost(GDBusConnection *connection, const gchar *name,
			  gpointer user_data)
{
	RSU_LOG_INFO("Lost Bus %s", name);

	g_context.connection = NULL;

	rsu_task_processor_set_quitting(g_context.processor);
}

static gboolean prv_quit_handler(GIOChannel *source, GIOCondition condition,
				 gpointer user_data)
{
	rsu_task_processor_set_quitting(g_context.processor);
	g_context.sig_id = 0;

	return FALSE;
}

static bool prv_init_signal_handler(sigset_t mask)
{
	bool retval = false;
	int fd = -1;
	GIOChannel *channel = NULL;

	fd = signalfd(-1, &mask, SFD_NONBLOCK);

	if (fd == -1)
		goto on_error;

	channel = g_io_channel_unix_new(fd);
	g_io_channel_set_close_on_unref(channel, TRUE);

	if (g_io_channel_set_flags(channel, G_IO_FLAG_NONBLOCK, NULL) !=
	    G_IO_STATUS_NORMAL)
		goto on_error;

	if (g_io_channel_set_encoding(channel, NULL, NULL) !=
	    G_IO_STATUS_NORMAL)
		goto on_error;

	g_context.sig_id = g_io_add_watch(channel, G_IO_IN | G_IO_PRI,
					  prv_quit_handler,
					  NULL);

	retval = true;

on_error:

	if (channel)
		g_io_channel_unref(channel);

	return retval;
}

static void prv_unregister_client(gpointer client)
{
	guint id = GPOINTER_TO_UINT(client);
	g_bus_unwatch_name(id);
}

rsu_upnp_t *rsu_renderer_service_get_upnp(void)
{
	return g_context.upnp;
}

int main(int argc, char *argv[])
{
	sigset_t mask;
	int retval = 1;

	prv_rsu_context_init();

	sigemptyset(&mask);
	sigaddset(&mask, SIGTERM);
	sigaddset(&mask, SIGINT);

	if (sigprocmask(SIG_BLOCK, &mask, NULL) == -1)
		goto on_error;

	g_type_init();

	rsu_log_init(argv[0]);
	rsu_settings_new(&g_context.settings);

	g_set_prgname(PRG_NAME);

	g_context.root_node_info =
		g_dbus_node_info_new_for_xml(g_rsu_root_introspection, NULL);

	if (!g_context.root_node_info)
		goto on_error;

	g_context.server_node_info =
		g_dbus_node_info_new_for_xml(g_rsu_server_introspection, NULL);

	if (!g_context.server_node_info)
		goto on_error;

	g_context.main_loop = g_main_loop_new(NULL, FALSE);

	g_context.owner_id = g_bus_own_name(G_BUS_TYPE_SESSION,
					    RSU_SERVER_NAME,
					    G_BUS_NAME_OWNER_FLAGS_NONE,
					    prv_bus_acquired, NULL,
					    prv_name_lost, NULL, NULL);

	g_context.watchers = g_hash_table_new_full(g_str_hash, g_str_equal,
						   g_free,
						   prv_unregister_client);

	if (!prv_init_signal_handler(mask))
		goto on_error;

	g_main_loop_run(g_context.main_loop);

	if (g_context.error)
		goto on_error;

	retval = 0;

on_error:

	prv_rsu_context_free();

	rsu_log_finalize();

	return retval;
}
