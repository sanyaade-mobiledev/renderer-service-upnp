# test_lost_client
#
# Copyright (C) 2012 Intel Corporation. All rights reserved.
#
# This program is free software; you can redistribute it and/or modify it
# under the terms and conditions of the GNU Lesser General Public License,
# version 2.1, as published by the Free Software Foundation.
#
# This program is distributed in the hope it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License
# for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
#
# Regis Merlino <regis.merlino@intel.com>
#

import sys
import gobject
import dbus
import dbus.mainloop.glib

def handle_reply(props):
    print "Total Items: " + str(len(props))
    print
    loop.quit()

def handle_error(e):
    print "An error occured"
    loop.quit()

def make_async_call():
    root.GetAll("",
                reply_handler=handle_reply,
                error_handler=handle_error)
    root.GetAll("",
                reply_handler=handle_reply,
                error_handler=handle_error)
    root.GetAll("",
                reply_handler=handle_reply,
                error_handler=handle_error)
    root.GetAll("",
                reply_handler=handle_reply,
                error_handler=handle_error)
    root.GetAll("",
                reply_handler=handle_reply,
                error_handler=handle_error)
    # Test: force quit - this should cancel the search on server side
    loop.quit()

if __name__ == '__main__':
    dbus.mainloop.glib.DBusGMainLoop(set_as_default=True)
    
    bus = dbus.SessionBus()
    root = dbus.Interface(bus.get_object(
                              'com.intel.renderer-service-upnp',
                              '/com/intel/RendererServiceUPnP/server/0'),
                          'org.freedesktop.DBus.Properties')
    
    gobject.timeout_add(1000, make_async_call)
    
    loop = gobject.MainLoop()
    loop.run()
