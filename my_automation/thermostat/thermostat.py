import queue
import sys
import threading
import time

class SmoothSampler():
    """ average sevaral readings to get sample """
    def __init__(self, count, period, sample_q=None):
        self.count = count
        self.period = period
        self.sample_q = sample_q or queue.Queue()

        self.sample_q = queue.Queue()
        self._thread = None

    def run(self):
        def _run():
            total = 0
            sample_count = 0
            with open('/sys/bus/acpi/devices/LNXTHERM:00/thermal_zone/temp', 'r') as cpu_temp:
                cpu_temp_raw = cpu_temp.read()
                raw_q = int(cpu_temp_raw.strip().rstrip('000'))


            try:
                for sample in (iter(raw_q.get, None)):
                    total += sample.value
                    sample_count += 1
                    if (sample_count >= self.count):
                        averaged_sample = Sample(total/sample_count, sample.time)
                        self.sample_q.put(averaged_sample)
                        total = 0
                        sample_count = 0

            finally:
                self._raw_sampler.stop()

        if not self.is_running():
            self._thread = threading.Thread(target=_run)
            self._thread.start()

    def stop(self):
        if self.is_running():
            raw_q = self._raw_sampler.sample_q
            raw_q.put(None)
            self._thread.join()

    def is_running(self):
        return self._thread and self._thread.is_alive()

class Heater():
    def __init__(self, pin=17, cycle_time=20, minimum_duration=1):
        self.set_cycle_time(cycle_time)
        self.setting = 0
        self.period = 30
        self.sample_q = queue.Queue()
        self.minimum_duration = minimum_duration

        #super().__init__(pin)
        self.sampler = SmoothSampler(3, self.period, self.sample_q)

    def set(self, fraction):
        """ set heater power to fraction, a value between 0 and 1 """
        if fraction < 0 or fraction > 100:
            err_msg = "heater power setting must be between 0 and 100"
            raise ValueError(fraction, err_msg)
        self.setting = fraction
        time_on =  self.cycle_time * self.setting
        time_off = self.cycle_time - time_on
        if time_off < self.minimum_duration:
            self.set_on()
        elif time_on < self.minimum_duration:
            self.set_off()
        else:
            self.set_cycle(time_on, time_off)

    def set_cycle_time(self, seconds):
        """ set duration of the heater cycle """
        if seconds <= 0:
            err_msg = "heat_cycle must be a positive value"
            raise ValueError(seconds, err_msg)
        self.cycle_time = seconds

    def stop(self):
#        logging.info("Heating thread stop")
        self.set_off()

class Cooker():
    def __init__(self, relay_pin, target=50, heater=None ):
        self.relay_pin = relay_pin

        # Cooker state variables
        self.mode = "auto"
        self.temperature = -1
        self.sample_time = -1
        self.target = target
        self.heater_setting = 0
        self.kp = 0.2           # max if error is 2 degrees low or more
        self.proportional = 0
        self.ki = 0.01/60
        self.offset = 0         # integral term

        # Heater, temperature sampler, threads, etc
        self.heater = heater or Heater(relay_pin)
        self._state_lock = threading.RLock()
        self._sampler_thread = None

        self.sample_q = queue.Queue()
        self.period = 30
        self.history = []       # list of cooker states

    def control(self, new_sample=None):
        """ control the heater using Cooker's state """
        with self._state_lock as l:
            if new_sample:
                (sample_time, new_temp) = new_sample
                dt = sample_time - self.sample_time
                self.temperature = new_temp
                self.sample_time = sample_time
            else:
                dt = 0

            # set the heater_settting to new value depending on mode
            if self.mode == "manual":
                self.heater_setting = self.offset
            else:
                self.pid(dt)

            self.heater.set(self.heater_setting)

            state = self.get_state()
            if (not self.history) or (state != self.history[-1]):
                self.history.append(state)

    def pid(self, dt=0):
        """ control the heater using Cooker's state """
        with self._state_lock as l:
            error = self.target - self.temperature
            if abs(error) < 2:
                self.offset = max(self.offset + self.ki * error * dt, 0)
            else:
                self.offset = 0
            self.proportional = self.kp * error
            heater = self.proportional + self.offset
            heater = min( max(heater, 0.0), 1.0)
            self.heater_setting = heater

    def _sampler_is_running(self):
        return self._sampler_thread and self._sampler_thread.is_alive()

    def start_sampling(self):
        """ start getting new temperature samples, run self.control() to update
            state when new temperatures arrive """
        def _run_sampler():
            self.sampler.run()
            try:
                for new_sample in iter(self.sample_q.get, None):
                    self.control(new_sample)
            finally:
                self.sampler.stop()

        if not self._sampler_is_running():
            self._sampler_thread = threading.Thread(target=_run_sampler)
            self._sampler_thread.start()

    def stop_sampling(self):
        if self._sampler_is_running():
            self.sample_q.put(None)

    def close(self):
        self.heater.stop()
        self.stop_sampling()

    def get_state(self):
        """ return the current state of the Cooker as a dict """
        return {'sample_time': self.sample_time,
                'temperature': self.temperature,
                'target': self.target,
                'mode': self.mode,
                'error': self.target - self.temperature,
                'setting': self.heater_setting,
                'proportional': self.proportional,
                'offset': self.offset,
                'kp': self.kp,
                'ki': self.ki}

    def set_state(self, data):
        """ set the state of the Cooker to the values passed in a dict (of the
            type returned by get_state) """
        self.target = float(data['target'])
        self.mode = (data['mode'])
        self.offset = float(data['offset'])
        self.kp = float(data['kp'])
        self.ki = float(data['ki'])
        self.control()

    def __str__(self):
        """ pretty print Cooker """
        time_str = time.strftime("%H:%M:%S", time.localtime(self.sample_time))
        header = "Cooker: <{}>({}, {:0.2f} seconds ago):".format(
            self.mode, time_str, time.time() - self.sample_time)
        temps = "  current: {:7.2f}  target: {:7.2f}  error: {:7.3f}".format(
            self.temperature, self.target, self.target - self.temperature)
        proportional = "proportional: {:8.4f}, kp: {:7.5f} ".format(
            self.proportional, self.kp)
        offset = "offset: {:14.4f}, ki: {:7.5f}".format(
            self.offset, self.ki)
        settings = "  setting: {:7.2f}% {:6.4f}\n    {}\n    {}".format(
            self.heater_setting*100, self.heater_setting, proportional, offset)
        return "\n".join([header, temps, settings])


if __name__ == "__main__":
    cooker = Cooker(relay_pin=10, target=50)

    try:
        cooker.heater.run()
        cooker.start_sampling()
        cooker._sampler_thread.join()
    finally:
        cooker.close()
