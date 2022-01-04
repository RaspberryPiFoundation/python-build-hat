import weakref
import time
import os
import sys
import gc

class DeviceTypes:
    """Translate BuildHAT device types"""

    _device_names = {  2: "PassiveMotor",
                       8: "Light",
                      34: "TiltSensor",
                      35: "MotionSensor",
                      37: "ColorDistanceSensor",
                      61: "ColorSensor",
                      62: "DistanceSensor",
                      63: "ForceSensor",
                      64: "Matrix",
                      38: "Motor",
                      46: "Motor",
                      47: "Motor",
                      48: "Motor",
                      49: "Motor",
                      65: "Motor",
                      75: "Motor",
                      76: "Motor"
                    }

    # With corresponding part numbers
    _device_descriptions = {  2: "PassiveMotor",
                       8: "Light", #88005
                      34: "WeDo 2.0 Tilt Sensor", #45305
                      35: "MotionSensor",
                      37: "Color & Distance Sensor", #88007
                      61: "Color Sensor", #45605
                      62: "Distance Sensor", #45604
                      63: "Force Sensor", #45606
                      64: "3x3 Color Light Matrix", #45608
                      38: "Medium Linear Motor", # Either 45303 or 88008, not sure
                      46: "Large Motor", #88013
                      47: "XL Motor", #88014
                      48: "Medium Motor", # Maybe 45303
                      49: "Large Angular Motor", #88017
                      65: "Small Angular Motor", #45607
                      75: "Medium Angular Motor", # Maybe 88018, came with 51515
                      76: "Motor"
                    }

    _unknown_device_name = "Unknown"
    _disconnected_device_name = "Disconnected"

    def __init__(self):
        pass

    def name_for_id(typeid):
        """Translate integer type id to device name"""
        if typeid in DeviceTypes._device_names:
            return DeviceTypes._device_names[typeid]
        else:
            return DeviceTypes._unknown_device_name

    def desc_for_id(typeid):
        """Translate integer type id to something more descriptive than the device name"""
        if typeid in DeviceTypes._device_descriptions:
            return DeviceTypes._device_descriptions[typeid]
        else:
            return DeviceTypes._unknown_device_name

    def all_ids_for_name(devicename):
        """
        Get an array of all typeids that match a given Device name
        (Different devices that are used similarly, such as Motors)
        """

        return [k for k,v in DeviceTypes._device_names.items() if v == devicename]

