# -*- coding: utf-8 -*-

# renderer-console
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
# Sébastien Bianti <sebastien.bianti@linux.intel.com>
#

import dbus
import json
import xml.etree.ElementTree as ET

ROOT_OBJECT_PATH = '/com/intel/RendererServiceUPnP'
RENDERER_BUS = 'com.intel.renderer-service-upnp'

PROPS_IF_NAME = 'org.freedesktop.DBus.Properties'
INTROSPECTABLE_IF_NAME = 'org.freedesktop.DBus.Introspectable'

DEVICE_IF_NAME = 'com.intel.UPnP.RendererDevice'
PUSH_HOST_IF_NAME = 'com.intel.RendererServiceUPnP.PushHost'
MANAGER_INTERFACE = 'com.intel.RendererServiceUPnP.Manager'

MEDIAPLAYER2_IF_NAME = 'org.mpris.MediaPlayer2'
PLAYER_IF_NAME = 'org.mpris.MediaPlayer2.Player'


global bus_type
bus_type = dbus.SessionBus()

def print_json(props):
    print json.dumps(props, indent=4, sort_keys=True)

def get_interface(path, if_name):
    return dbus.Interface(bus_type.get_object(RENDERER_BUS, path), if_name)

class Renderer(object):
    "Represent a renderer service"

    def __init__(self, object_path):
        self.__path = object_path
        self.__propsIF = get_interface(object_path, PROPS_IF_NAME)
        self.__playerIF = get_interface(object_path, PLAYER_IF_NAME)
        self.__pushhostIF = get_interface(object_path, PUSH_HOST_IF_NAME)

    def get_interfaces(self):
        try:
            introspectable_IF = get_interface(self.__path,
                                              INTROSPECTABLE_IF_NAME)
        except:
            print(u"Failed to retrieve introspectable interface")

        introspection = introspectable_IF.Introspect()
        tree = ET.fromstring(introspection)

        return [i.attrib['name'] for i in tree if i.tag == "interface"]

    def interfaces(self):
        for i in self.get_interfaces():
            print i

    def get_prop(self, prop_name, inner_if_name = ""):
        return self.__propsIF.Get(inner_if_name, prop_name)

    def get_props(self, inner_if_name = ""):
        return self.__propsIF.GetAll(inner_if_name)

    def print_props(self, inner_if_name = ""):
        print_json(self.get_props(inner_if_name))

    def set_prop(self, prop_name, if_name, val):
        """
        Sets only the following properties :
        	Rate and Volume
        """
        return self.__propsIF.Set(if_name, prop_name, val)

# Control methods
    def play(self):
        self.__playerIF.Play()

    def pause(self):
        self.__playerIF.Pause()

    def play_pause(self):
        self.__playerIF.PlayPause()

    def next(self):
        self.__playerIF.Next()

    def open_uri(self, uri):
        self.__playerIF.OpenUri(uri)

    def previous(self):
        self.__playerIF.Previous()

    def seek(self, offset):
        self.__playerIF.Seek(offset)

    def goto_track(self, trackID):
        self.__playerIF.GotoTrack(trackID)

    def set_position(self, trackID, position):
        self.__playerIF.setPosition(trackID, position)

    def stop(self):
        self.__playerIF.Stop()

# Push Host methods
    def host_file(self, path):
        return self.__pushhostIF.HostFile(path)

    def remove_file(self, path):
        self.__pushhostIF.RemoveFile(path)

class Manager(object):
    """
    High level class for detecting Renderers and doing common operations
    on RendererServiceUPnP
    """

    def __init__(self):
        self.__manager = get_interface(ROOT_OBJECT_PATH, MANAGER_INTERFACE)
        self.__renderers = []

    def update_renderers(self):
        self.__renderers = self.__manager.GetServers()

    def get_renderers(self):
        self.update_renderers()
        return self.__renderers

    def renderers(self):
        self.update_renderers()

        for path in self.__renderers:
            try:
                renderer = Renderer(path)
                renderer_name = renderer.get_prop("Identity")
                print(u"%s : %s" % (path, renderer_name))
            except:
                print(u"Failed to retrieve Identity for interface %s" % path)

    def get_version(self):
        return self.__manager.GetVersion()

    def version(self):
        print self.get_version()

    def release(self):
        self.__manager.Release()


if __name__ == "__main__":

    print("\n\t\t\tExample for using rendererconsole:")
    print("\t\t\t¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯\n")

    manager = Manager()
    print("Version = %s" %  manager.get_version())
    print("¯¯¯¯¯¯¯")

    print "\nRenderer's list:"
    print("¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯")
    manager.renderers()

    renderer_list = manager.get_renderers()

    for name in renderer_list:
        renderer = Renderer(name)
        interface_list = renderer.get_interfaces()

        print("\nInterfaces of %s:" % name)
        print("¯¯¯¯¯¯¯¯¯¯¯¯¯¯" + "¯" * len(name))
        for i in interface_list:
            print i

        if_name = DEVICE_IF_NAME
        if (if_name in interface_list) :
            print("\nProperties of %s on %s:" % (if_name, name))
            print("¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯" + (len(name) + len(if_name)) * "¯")
            renderer.print_props(if_name)
