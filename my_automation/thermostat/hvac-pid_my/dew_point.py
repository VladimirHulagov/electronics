from math import log10

class DewPoint(object):
	# Constants
	# water @ -20 - +50 C
	humidity_constants = {
		'T_n': 240.7263, 
		'm': 7.591386,
		'A': 6.116441,
		'C': 2.16679,
		'Celsius2Kelvin': 273.15,
	}

	def water_vapour_pressure(self, temperature, relative_humidity):
		# https://www.vaisala.com/sites/default/files/documents/Humidity_Conversion_Formulas_B210973EN-F.pdf
		# Formula 6
		try:
			P_ws = self.humidity_constants['A'] * pow(10, (self.humidity_constants['m'] * temperature) / (temperature + self.humidity_constants['T_n']))
			P_w = P_ws * relative_humidity / 100
		except ValueError as error:
			P_w = None
		finally: 
			return P_w

	def calculate(self, temperature, relative_humidity):
		# https://www.vaisala.com/sites/default/files/documents/Humidity_Conversion_Formulas_B210973EN-F.pdf
		# Formula 7
		try:
			P_w = self.water_vapour_pressure(temperature, relative_humidity)
			T_d = self.humidity_constants['T_n'] / ((self.humidity_constants['m'] / log10(P_w / self.humidity_constants['A'])) - 1) 
		except ValueError as error:
			T_d = None
		finally: 
			return T_d
