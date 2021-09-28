import logging
import subprocess

def init():
    """ Initialize GPIO pins and machine itself.
    """
    logging.info("HW init passed")
    
    return heater_type, fan_

def fan_control(on_off):
    """
    Cooling fan control.
    :param on_off: boolean value if fan is enabled.
    """
    if on_off:
        logging.info("Fan is on")
    else:
        logging.info("Fan is off")


def heater_control(proportion, subproc_handle=None, cycle_time=200):
    """ Heater control.
    :param percent: heater power in percent 0..100. 0 turns heater off.
    """
    min_cycle_time = 20
    logging.info("Set heater power to proportion {}, a value must be between 0 and 100".format(proportion))
    if proportion < 0 or proportion > 100:
        err_msg = "heater power setting must be between 0 and 100"
        raise ValueError(proportion, err_msg)
    setting = 1/proportion
    duration =  cycle_time * setting
    #time_off = cycle_time - durationp
    logging.debug("Heater settings: p = {}; Time on = {}".format(proportion, duration))
    if subproc_handle:
        if duration > min_cycle_time:
            subproc_state = subproc_handle.poll()
            if subproc_state is None:
                logging.debug("Subprocess is still alive: do nothing...")
        else:
            subproc_handle.kill
    else:
        logging.debug("Subprocess is dead: starting a new one...")
        stress_process = subprocess.Popen(['stress-ng', '--cpu' , '0', '--cpu-method', 'fft', '--timeout', str(duration)])

def get_temperature():
    """ Measure temperature.
    :return: temperature in Celsius.
    """
    with open('/sys/bus/acpi/devices/LNXTHERM:00/thermal_zone/temp', 'r') as cpu_temp:
      cpu_temp_raw = cpu_temp.read()
      logging.debug("RAW temperature: {}".format(cpu_temp_raw))
      temperature = int(float(cpu_temp_raw.strip())/1000)
      logging.debug("Current temperature: {} deg.C".format(temperature))
      return temperature

def get_humidity():
    return 80

def deinit():
    """ De-initialize hardware.
    """
    logging.info("De-initialize hardware")


def watchdog_feed():
    """ Feed hardware watchdog.
    """
    watchdog.feed()
