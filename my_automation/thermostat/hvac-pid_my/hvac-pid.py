import logging
import time
import os
import json
from datetime import datetime, timedelta
from mqtt import MQTTClient
from temp import Temp
from fan import Fan
from power import Power
from math import floor, ceil
from config import Config
from dotenv import load_dotenv
from state import State

load_dotenv()

class HVACPIDController(object):
    logger = None
    mqtt = None
    temp = None
    fan = None
    power = None
    config = None
    state = None

    temp_outdoors = 0

    mode = 'auto'
    manual = False
    control_enable = False
    hvac_state = {}
    next_iteration = None

    def __init__(self):
        self.logger = logging.getLogger('hvac-pid')
        self.logger.info('Starting hvac-pid')

        self.mqtt_available = False
        self.pid_options = None
        self.temp_options = None

        self.config = Config()

        # MQTT
        self.topic_prefix = os.getenv('MQTT_PID_TOPIC_PREFIX')
        self.mqtt = MQTTClient(os.getenv('MQTT_CLIENT_ID'), os.getenv('MQTT_BROKER_HOST'))
        try:
          self.mqtt.connect()
          self.mqtt_available = True
          self.logger.info('Successfully connected to MQTT broker')
        except Exception as e:
          self.mqtt_available = False
          print(e)

        # PID options
        self.pid_options = self.config.getPIDOptions(self.mode)
        self.logger.debug('Loaded PID coeff.: %s', self.pid_options)
        self.temp_options = self.config.getTempOptions(self.mode)
        self.logger.debug('Loaded temperature targets: %s', self.temp_options)

        # Temp
        # TODO CODE: get config from YAML-file?
        # TODO LOGIC: get 'mode' from temp_target
        self.temp = Temp(self.mqtt_available, **{**self.temp_options, **self.pid_options})


        # Fan
        self.fan = Fan()

        # Heater?
        self.power = Power()

        # Occupancy state
        self.state = State(**self.config.getStateOptions())

        #self.next_iteration = datetime.now() + timedelta(minutes=2)
        self.next_iteration = datetime.now() + timedelta(minutes=1)

        if self.mqtt_available:
            self.logger.info('Working with MQTT root topic: %s', self.topic_prefix)
            # subscribe
            self.mqtt.subscribe(self.topic_prefix + '/mode/set', 0, self.set_mode)
            self.mqtt.subscribe(self.topic_prefix + '/temperature/set', 0, self.set_temp)
            self.mqtt.subscribe(self.topic_prefix + '/fan/set', 0, self.set_fan)
            #self.mqtt.subscribe(os.getenv('MQTT_HVAC_OCCUPANCY_STATE_TOPIC'), 0, self.set_occupancy_state)
            if self.temp.temp_source == 'mqtt':
                self.mqtt.subscribe(os.getenv('MQTT_TEMP_TOPIC'), 0, self.temp.mqtt_update_callback)
                self.mqtt.subscribe(os.getenv('MQTT_TEMP_OUTDOORS_TOPIC'), 0, self.temp.mqtt_outdoors_update_callback)

            # Fan
            self.publish_fan()
            self.publish_temp()
            self.publish_mode()

        # wait a bit before enabling control
        time.sleep(5)

    def iterate(self):
        if self.manual:
            self.logger.info('Manual mode, skipping PID iteration')
        else:
            self.logger.info('Auto mode, iterate PID')
            # Added initial value from target temp.
            compensated_request_temp = self.state.compensateRequestTemp(self.temp.temp_request, self.temp_outdoors)
            if self.temp_outdoors:
                max_set_temp = ceil(self.temp.temp_absolute) + 3

                # temp hax
                # limit min temp when outdoors is < -10
                if self.temp_outdoors < -10:
                    self.temp.setLimits(floor(compensated_request_temp) - 1, max_set_temp)
                    self.logger.debug('Limiting min temp to %g when outdoor temp is %g', self.temp.temp_min, self.temp_outdoors)
                else:
                    self.temp.setLimits(self.config.getSetTempMin(), max_set_temp)

            self.logger.info('Requested temp: %s', compensated_request_temp)
            self.temp.iteratePID(compensated_request_temp)
            self.fan.calculate(self.temp.pid_offset, self.mode)
            self.power.calculate(self.temp.temp_request, self.temp.temp_measure, self.mode, self.temp_outdoors)
            if not self.power.state:
                self.temp.reset()
            if self.mqtt_available:
                self.publish_state()
                self.publish_temp()

    def setHVAC(self):
        if self.mqtt_available:
            topic = os.getenv('MQTT_HVAC_TOPIC')
            new_state = {
                'power': self.power.state,
                'mode': self.mode.upper(),
                'temperature': self.temp.temp_set,
                'fan': self.fan.speed,
            }

            is_state_changed = (new_state['power'] and self.hvac_state != new_state)
            is_power_state_changed = (self.hvac_state and new_state['power'] != self.hvac_state['power'])
            old_state_doesnt_exists = (not self.hvac_state)

            if is_state_changed or is_power_state_changed or old_state_doesnt_exists:
                message = json.dumps(new_state)

                self.logger.debug('Controlling HVAC with command %s', message)
                if self.mqtt_available:
                    self.mqtt.publish(topic, message, 1)
            else:
                self.logger.debug('HVAC state unchanged %s', self.hvac_state)
        else:
            self.logger.debug('Controlling HVAC disabled')

    def set_mode(self, client, userdata, message):
        mode = message.payload.decode('utf-8')
        previous_mode = self.mode

        # reset PID if switching between modes
        if previous_mode != mode:
            self.pid_options = self.config.getPIDOptions(mode)
            self.temp_options = self.config.getTempOptions(mode)
            self.temp = Temp(**{**temp_options, **pid_options})

        if mode == 'off':
            self.manual = True
            self.mode = 'auto'
            self.power.state = False
            self.logger.info('Set mode to off')
        if mode == 'manual':
            self.manual = True
            self.power.state = True
            self.mode = 'auto'
            self.temp.temp_set = self.temp.temp_request
            self.logger.info('Set mode to manual')
        elif mode == 'heat':
            self.manual = False
            self.mode = mode
            self.logger.info('Set mode to %s', self.mode)
        elif mode == 'cool':
            self.manual = False
            self.mode = mode
            self.temp.temp_set = self.temp.temp_absolute
            self.logger.info('Set mode to %s', self.mode)

        self.state.setMode(mode)
        self.publish_mode()
        #self.setHVAC()
        self.set_next_iteration(2)

    def publish_mode(self):
        if not self.mqtt_available:
            return

        topic = self.topic_prefix + '/mode/state'

        if self.manual:
            if self.power.state == False:
                mode = 'off'
            else:
                mode = 'manual'
        elif self.mode == 'auto':
            mode = 'manual'
        else:
            mode = self.mode

        self.mqtt.publish(topic, mode, 1, True)

    def set_temp(self, client, userdata, message):
        temp = round(float(message.payload.decode('utf-8')), 2)

        print("SET_TEMP OPTIONS:")
        print(self.temp_options)
        if temp >= float(os.getenv('REQUEST_MIN_TEMP', 0)) and temp <= float(os.getenv('REQUEST_MAX_TEMP', 100)):
            self.temp.setRequest(temp)

            if self.manual:
                self.temp.temp_set = self.temp.temp_request
            else:
                self.temp.reset()

            self.publish_temp()
            #self.setHVAC()

    def publish_temp(self):
        if not self.mqtt_available:
            return

        self.logger.debug('Publishing temperature measurements: %s', self.temp.temp_measure)
        self.mqtt.publish(self.topic_prefix + '/temperature/state', self.temp.temp_request, 1, True)
        self.mqtt.publish(self.topic_prefix + '/measured_temperature', self.temp.temp_measure, 1, True)

    def set_fan(self, client, userdata, message):
        fan = message.payload.decode('utf-8')

        if fan != "auto":
            fan_int = int(fan)

            if self.manual and fan_int >= 1 and fan_int <= 5:
                self.fan.speed = fan_int
                self.publish_fan()
                #self.setHVAC()
                self.logger.info('Manually set fan speed to %s/5', self.fan.speed)

    def publish_fan(self):
        if not self.mqtt_available:
            return

        if self.manual:
            fan = self.fan.speed
        else:
            fan = 'auto'

        self.logger.debug('Publishing FAN state: %s', fan)
        topic = self.topic_prefix + '/fan/state'

        self.mqtt.publish(topic, fan, 1, True)

    def publish_state(self):
        if not self.mqtt_available:
            return

        timestamp = time.time()
        topic = os.getenv('MQTT_PID_TOPIC_PREFIX') + '/state'
        message = json.dumps({
            'mode': self.mode,
            'manual': self.manual,
            'temperature_request': float(self.temp.temp_request),
            'temperature_set': float(self.temp.temp_set),
            'temperature_measure': float(self.temp.temp_measure),
            'temperature_error': float(self.temp.pid.previous_error),
            'set_temperature_lower_limit': float(self.temp.temp_min),
            'set_temperature_upper_limit': float(self.temp.temp_max),
            'fan': int(self.fan.speed if self.power.state else 0),
            'power': self.power.state,
            'Kp': float(self.temp.pid.Kp),
            'Ki': float(self.temp.pid.Ki),
            'Kd': float(self.temp.pid.Kd),
            'integral': float(self.temp.pid.integral),
            'integral_max': float(self.temp.pid.integral_max),
            'pid_offset': float(self.temp.pid_offset),
            'pid_result': float(self.temp.pid_result),
            'timestamp': float(timestamp),
        })
        self.mqtt.publish(topic, message, 1)

    def set_occupancy_state(self, client, userdata, message):
        state = message.payload.decode('utf-8')
        self.state.setState(state)
        self.logger.info('Setting occupancy state to %s', self.state.state)
        self.temp.reset()
        self.set_next_iteration(2)

    def set_next_iteration(self, interval):
        self.next_iteration = datetime.now() + timedelta(minutes=interval)
        self.logger.info('Next iteration at %s', self.next_iteration)

if __name__ == '__main__':
    logger = logging.getLogger('hvac-pid')
    logger.setLevel(logging.DEBUG)
    logger.propagate = False

    formatter = logging.Formatter('%(asctime)s - %(name)s - %(levelname)s - %(message)s')

    ch = logging.StreamHandler()
    ch.setLevel(logging.DEBUG)
    ch.setFormatter(formatter)

    logger.addHandler(ch)

    ctrl = HVACPIDController()

    while True:
        time.sleep(2)
        if ctrl.temp.temp_source == 'embedded':
            ctrl.temp.getMeasurements()
#        if not ctrl.temp.temp_absolute:
#            continue
        print(ctrl.pid_options)
        print(ctrl.temp.target)
        #ctrl.iterate(ctrl.temp.target, ctrl.temp.temp_measure)
        ctrl.iterate()
        logger.debug("Sleeping... Configured interval is %s", ctrl.config.getWaitTime(ctrl.mode))
        logger.debug("...next control iteration will be in %s", ctrl.next_iteration.strftime("%H:%M:%s"))
        if datetime.now() > ctrl.next_iteration:
            #ctrl.set_next_iteration(ctrl.config.getWaitTime(ctrl.mode))
            ctrl.set_next_iteration(ctrl.config.getWaitTime(ctrl.mode))
            logger.debug("...next control iteration will be in %s", ctrl.next_iteration.strftime("%H:%M:%s"))

            if not ctrl.manual:
                ctrl.iterate()
                #ctrl.setHVAC()
                ctrl.publish_mode()
                ctrl.publish_fan()
                ctrl.publish_temp()

            if not ctrl.mqtt_available:
                ctrl.publish_temp()
