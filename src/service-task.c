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

#include "device.h"
#include "log.h"
#include "service-task.h"
#include "task-processor.h"

struct rsu_service_task_t_ {
	rsu_task_atom_t base; /* pseudo inheritance - MUST be first field */
	GUPnPServiceProxyActionCallback callback;
	GUPnPServiceProxyAction *p_action;
	GDestroyNotify free_func;
	gpointer user_data;
	rsu_service_task_action t_action;
	rsu_device_t *device;
	GUPnPServiceProxy *proxy;
};

const char *rsu_service_task_create_source(void)
{
	static unsigned int cpt = 1;
	static char source[20];

	g_snprintf(source, 20, "source-%d", cpt);
	cpt++;

	return source;
}

void rsu_service_task_add(const rsu_task_queue_key_t *queue_id,
			  rsu_service_task_action action,
			  rsu_device_t *device,
			  GUPnPServiceProxy *proxy,
			  GUPnPServiceProxyActionCallback action_cb,
			  GDestroyNotify free_func,
			  gpointer cb_user_data)
{
	rsu_service_task_t *task;

	task = g_new0(rsu_service_task_t, 1);

	task->t_action = action;
	task->callback = action_cb;
	task->free_func = free_func;
	task->user_data = cb_user_data;
	task->device = device;
	task->proxy = proxy;

	if (proxy != NULL)
		g_object_add_weak_pointer((G_OBJECT(proxy)),
					  (gpointer *)&task->proxy);

	rsu_task_queue_add_task(queue_id, &task->base);
}

void rsu_service_task_begin_action_cb(GUPnPServiceProxy *proxy,
				      GUPnPServiceProxyAction *action,
				      gpointer user_data)
{
	rsu_service_task_t *task = (rsu_service_task_t *)user_data;

	task->p_action = NULL;
	task->callback(proxy, action, task->user_data);

	rsu_task_queue_task_completed(task->base.queue_id);
}

void rsu_service_task_process_cb(rsu_task_atom_t *atom, gpointer user_data)
{
	gboolean failed = FALSE;
	rsu_service_task_t *task = (rsu_service_task_t *)atom;

	task->p_action = task->t_action(task, task->proxy, &failed);

	if (failed)
		rsu_task_processor_cancel_queue(task->base.queue_id);
	else if (!task->p_action)
		rsu_task_queue_task_completed(task->base.queue_id);
}

void rsu_service_task_cancel_cb(rsu_task_atom_t *atom, gpointer user_data)
{
	rsu_service_task_t *task = (rsu_service_task_t *)atom;

	if (task->p_action) {
		if (task->proxy)
			gupnp_service_proxy_cancel_action(task->proxy,
							  task->p_action);
		task->p_action = NULL;

		rsu_task_queue_task_completed(task->base.queue_id);
	}
}

void rsu_service_task_delete_cb(rsu_task_atom_t *atom, gpointer user_data)
{
	rsu_service_task_t *task = (rsu_service_task_t *)atom;

	if (task->free_func != NULL)
		task->free_func(task->user_data);

	if (task->proxy != NULL)
		g_object_remove_weak_pointer((G_OBJECT(task->proxy)),
					     (gpointer *)&task->proxy);

	g_free(task);
}

rsu_device_t *rsu_service_task_get_device(rsu_service_task_t *task)
{
	return task->device;
}

gpointer *rsu_service_task_get_user_data(rsu_service_task_t *task)
{
	return task->user_data;
}
