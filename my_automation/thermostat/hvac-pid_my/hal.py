# This implementation allows to use different hardware.
# Imported module contains functions for hardware access fo some board/SoC.
# List of HAL methods that should be implemented in each module:
#    def init():
#        """ Initialize GPIO pins and machine itself.
#        """
#        do_something()
#
#
#    def fan_control(on_off):
#        """
#        Cooling fan control.
#        :param on_off: boolean value if fan is enabled.
#        """
#        do_something()
#
#
#    def get_temperature():
#        """ Measure bed temperature.
#        Can raise OSError or IOError on any issue with sensor.
#        :return: temperature in Celsius.
#        """
#        return measure()
#
#
#    def deinit():
#        """ De-initialise hal, stop any hardware.
#        """
#        do_something()
#
#
#    def watchdog_feed():
#        """ Feed hardware watchdog. This method should be called at least
#        once in 15 seconds. Also, this method can do no operation in hal
#        implementation and there will not be emergency stop for heaters.
#        """
#        do_something()


# check which module to import
try:
    from hal_laptop.hal import *
except ImportError:
    print("----- Hardware not detected, using virtual environment -----")
    print("----- Use M111 command to enable more detailed debug -----")
    #from cnc.hal_virtual import *

# check if all methods that is needed is implemented
if 'init' not in locals():
    raise NotImplementedError("hal.init() not implemented")
if 'fan_control' not in locals():
    raise NotImplementedError("hal.fan_control() not implemented")
if 'heater_control' not in locals():
    raise NotImplementedError("hal.heater_control() not implemented")
if 'get_temperature' not in locals():
    raise NotImplementedError("hal.get_temperature() not implemented")
if 'get_humidity' not in locals():
    raise NotImplementedError("hal.get_humidity() not implemented")
if 'deinit' not in locals():
    raise NotImplementedError("hal.deinit() not implemented")
if 'watchdog_feed' not in locals():
    raise NotImplementedError("hal.watchdog_feed() not implemented")
