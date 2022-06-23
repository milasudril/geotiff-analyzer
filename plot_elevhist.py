#!/usr/bin/python3

import numpy
import numpy.polynomial as poly
import matplotlib
import matplotlib.pyplot as plt
import os.path
import sys
from scipy.optimize import curve_fit
from dataclasses import dataclass
import matplotlib.colors

@dataclass
class DataSeries:
	name : str
	peak : float
	values : []

def plot(val):
	if type(val).__name__ == 'DataSeries':
		p = plt.plot(val.values[0],
			val.values[1]/1e6,
			label=val.name)
		return p
	else:
		x = numpy.linspace(val.x_min/1.05, val.x_max*1.05, 128, True)
		y = val(x)
		return plt.plot(x, y,label=val.name)

def load(filename, name):
	data = numpy.loadtxt(filename)
	peak = numpy.argmax(data, axis = 0)
	data = numpy.transpose(data);
	return DataSeries(name, peak[0], data)

if __name__ == '__main__':

	SMALL_SIZE = 8
	MEDIUM_SIZE = 10
	BIGGER_SIZE = 12

	plt.rc('font', size=SMALL_SIZE)          # controls default text sizes
	plt.rc('xtick', labelsize=SMALL_SIZE)    # fontsize of the tick labels
	plt.rc('ytick', labelsize=SMALL_SIZE)    # fontsize of the tick labels
	plt.rc('font',**{'family':'serif','serif':['Latin Modern Sans']})

	cm = 1/2.54  # centimeters in inches
	fig, ax = plt.subplots(1, figsize=(14*cm, 8*cm))
	plt.sca(ax)
	plt.xscale('log', basex=2)
	plt.yscale('log', basey=2)
	plt.xlabel('Elevation / m')
	plt.ylabel('Coverage / ($\\mathregular{km}^2/\\mathregular{m}$)')
	k = 0
	for item in sys.argv[2:]:
		print(item)
		data = load(item, str(k+1))
		x_vals = data.values[0]
		y_vals = data.values[1]
		k = k + 1
		plot(data)

	legend = plt.legend(loc='best', framealpha=0.5)

	plt.savefig(sys.argv[1])