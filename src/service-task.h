/*
 * renderer-service-upnp
 *
 * Copyright (C) 2013 Intel Corporation. All rights reserved.
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
 * Ludovic Ferrandis <ludovic.ferrandis@intel.com>
 * Regis Merlino <regis.merlino@intel.com>
 *
 */

#ifndef RSU_SERVICE_TASK_H__
#define RSU_SERVICE_TASK_H__

#include <glib.h>
#include <libgupnp/gupnp-service-proxy.h>

#include "renderer-service-upnp.h"
#include "task-atom.h"

typedef struct rsu_service_task_t_ rsu_service_task_t;

typedef GUPnPServiceProxyAction *(*rsu_service_task_action)
						(rsu_service_task_t *task,
						 GUPnPServiceProxy *proxy,
						 gboolean *failed);

const char *rsu_service_task_create_source(void);

void rsu_service_task_add(const rsu_task_queue_key_t *queue_id,
			  rsu_service_task_action action,
			  rsu_device_t *device,
			  GUPnPServiceProxy *proxy,
			  GUPnPServiceProxyActionCallback action_cb,
			  GDestroyNotify free_func,
			  gpointer cb_user_data);

void rsu_service_task_begin_action_cb(GUPnPServiceProxy *proxy,
				      GUPnPServiceProxyAction *action,
				      gpointer user_data);
void rsu_service_task_process_cb(rsu_task_atom_t *atom, gpointer user_data);
void rsu_service_task_cancel_cb(rsu_task_atom_t *atom, gpointer user_data);
void rsu_service_task_delete_cb(rsu_task_atom_t *atom, gpointer user_data);

rsu_device_t *rsu_service_task_get_device(rsu_service_task_t *task);
gpointer *rsu_service_task_get_user_data(rsu_service_task_t *task);

#endif /* RSU_SERVICE_TASK_H__ */
