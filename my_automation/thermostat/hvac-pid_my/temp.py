import logging
from pid import PID
from dew_point import DewPoint

import hal

class Temp(object):
    logger = None
    pid = None

    # state
    temp_request = None
    temp_measure = None
    temp_set = None
    temp_absolute = None
    pid_offset = 0
    pid_result = None

    temp_min = -100
    temp_max = 100

    mode = 'heat'

    mqtt_available = False

    def __init__(self, mqtt_available, temp_target, temp_max, temp_min, mode, **pid_options):
        self.logger = logging.getLogger('hvac-pid.temp')
        self.pid = PID(**{
            **pid_options,
            'max_output': temp_max,
            'min_output': temp_min,
        })

        self.dew_point = DewPoint()
        self.target = temp_target
        # Init target temp with initially requested temp
        self.setRequest(self.target)
        self.temp_max = temp_max
        self.temp_min = temp_min
        self.mode = mode
        self.mqtt_available = mqtt_available

        temp_embedded_value = None
        # Check for local sensor
        # Temperature:
        temp_embedded_value = hal.get_temperature()
        if temp_embedded_value:
            self.temp_source = 'embedded'
            self.logger.info('Using embedded temperature sensor')
        elif self.mqtt_available and not temp_embedded_value:
            self.temp_source = 'mqtt'
            self.logger.info('Using MQTT temperature sensor')
            self.mqtt.subscribe(os.getenv('MQTT_TEMP_TOPIC'), 0, self.temp_update_callback)
            self.mqtt.subscribe(os.getenv('MQTT_TEMP_OUTDOORS_TOPIC'), 0, self.temp_outdoors_update_callback)
        else:
            self.logger.error('No any temperatire sensors are available!')

        # Humidity
        if hal.get_humidity():
            self.temp_source = 'embedded'
        elif self.mqtt_available:
            self.temp_source = 'mqtt'
            self.mqtt.subscribe(os.getenv('MQTT_HUMI_TOPIC'), 0, self.temp_update_callback)

    def setMeasurements(self, temp_measure, temp_absolute, humi_measure):
        # TODO LOGIC: add several temp sources?
        self.temp_measure = temp_measure
        # Air temperature in the room (absolute)
        self.temp_absolute = temp_absolute
        self.humi_relative = humi_measure
        self.logger.info('Measured temp.: %s\nAbsolute temp.: %s\nHumidity: %s', self.temp_measure, self.temp_absolute, self.humi_relative)
        # TODO LOGIC: make deal with dew point: preheat, etc...
        if self.mode == 'cool':
            dew_point = self.dew_point.calculate(self.temp_measure, self.humi_relative)
            self.logger.info('Dew point.: %s\n', dew_point)
            self.temp.setMeasurements(round(dew_point, 2), self.temp_measure, self.humi_relative)

    def getMeasurements(self):
        # Air temperature after heater
        temp_measure = hal.get_temperature()
        humi_measure = hal.get_humidity()
        self.setMeasurements(temp_measure, temp_measure, humi_measure)

    def mqtt_update_callback(self, client, userdata, message):
        payload_json = json.loads(message.payload.decode('utf-8'))
        self.setMeasurements(payload_json['temperature'], payload_json['temperature'])

    def mqtt_outdoors_update_callback(self, client, userdata, message):
        payload_json = json.loads(message.payload.decode('utf-8'))
        self.temp_outdoors = float(payload_json['temperature'])

    def setRequest(self, temp_request):
        self.temp_request = temp_request
        self.logger.info('Set requested temperature to %s', self.temp_request)

    def iteratePID(self, temp_request_override = None):
        if not self.pid_result:
            self.pid_result = self.temp_absolute

        effective_temp_request = temp_request_override if temp_request_override != None else self.temp_request

        if self.mode == 'cool':
            self.pid.setLimits(0, 30)
        else:
            self.pid.setLimits(self.temp_min, self.temp_max)

        self.pid_offset = self.pid.iterate(effective_temp_request, self.temp_measure)
        self.pid_result += self.pid_offset

        if self.pid_result < self.temp_min:
            self.pid_result = self.temp_min
        elif self.pid_result > self.temp_max:
            self.pid_result = self.temp_max

        self.setTemperature(self.pid_result)

    def setTemperature(self, temp):
        max_temp = self.temp_absolute + 3.0
        min_temp = self.temp_min 
        self.logger.debug('Set temperature limits [%g, %g] input %g', max_temp, min_temp, temp)

        self.temp_set = int(round(min(max_temp, max(min_temp, temp))))
        self.logger.info('Set temperature is %s', self.temp_set)

    def setLimits(self, temp_min, temp_max):
        self.temp_min = temp_min
        self.temp_max = temp_max

    def reset(self):
        self.logger.info('Reset temps')
        self.pid.reset()
        self.pid_result = self.temp_absolute
