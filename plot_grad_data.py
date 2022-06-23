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

class PolyFit:
	def __call__(self, arg):
		return self.coefs(arg)

	def __init__(self, x_vals, y_vals, deg):
		self.coefs = poly.polynomial.Polynomial.fit(x_vals, y_vals, deg, rcond = None)
		print(self.__dict__)

class Fit0:
	def __call__(self, arg):
		return arg/self.a

	def __init__(self, x_vals, y_vals):
		x = numpy.array(x_vals)
		y = numpy.array(y_vals)
		self.x_min = x[0]
		self.x_max = x[-1]
		A = numpy.vstack([x]).T
		self.a = 1/numpy.linalg.lstsq(A, y, rcond = None)[0]
		self.name='Pure exp'
		print('Exp: %s'%self.__dict__)

def f2(arg, A, z0, alpha, beta):
	x = arg/z0
	return A*(pow(x, alpha)/(1 + pow(x, alpha)))*((1 + pow(x, 2*beta))/(1 + pow(x, beta)))

class Fit2:
	def __call__(self, arg):
		return f2(arg, self.A, self.z0, self.alpha, self.beta)

	def __init__(self, x_vals, y_vals):
		x = numpy.array(x_vals)
		y = numpy.array(y_vals)
		self.x_min = x[0]
		self.x_max = x[-1]
		self.A = 1/2
		self.z0 = 256
		self.alpha = 2
		self.beta = 0.25

		opt, _ = curve_fit(f2, x, y, [self.A, self.z0, self.alpha, self.beta], maxfev=16384, bounds=(
			 (0, 0, 0, 0),
			 (numpy.inf, numpy.inf, numpy.inf, 1)
			))

		self.A = opt[0]
		self.z0 = opt[1]
		self.alpha = opt[2]
		self.beta = opt[3]
		self.name = 'Model 2'
		print('Model 2: %s'%(self.__dict__))

def f1(arg, z0, alpha):
	x = arg/z0
	return pow(x, alpha)

class Fit1:
	def __call__(self, arg):
		return f1(arg, self.z0, self.alpha)

	def __init__(self, x_vals, y_vals):
		x = numpy.array(x_vals)
		y = numpy.array(y_vals)
		self.x_min = x[0]
		self.x_max = x[-1]
		self.z0 = 4096
		self.alpha = 0.5

		opt, _ = curve_fit(f1, x, y, [self.z0, self.alpha], maxfev=16384, bounds=(
			 (0, 0),
			 (numpy.inf, 1)))

		self.z0= opt[0]
		self.alpha = opt[1]
		self.name = 'Model 1'
		print('Model 1: %s'%(self.__dict__))

@dataclass
class DataSeries:
	name : str
	peak : float
	values : []

def plot(val):
	if type(val).__name__ == 'DataSeries':
		p = plt.scatter(val.values[0],
			val.values[1],
			marker='.',
			s=0.125,
			linewidth=0,
			linewidths=0,
			label='Data points')
		plt.plot([], [])
		return p
	else:
		x = numpy.linspace(val.x_min/1.05, val.x_max*1.05, 128, True)
		y = val(x)
		return plt.plot(x, y,label=val.name)

def load(filename):
	data = numpy.loadtxt(filename)
	peak = numpy.argmax(data, axis = 0)
	data = numpy.transpose(data);
	return DataSeries(os.path.basename(filename).split('.')[0], peak[0], data)

if __name__ == '__main__':
	data = load(sys.argv[1])
	x_vals = data.values[0]
	y_vals = data.values[1]
	fit0 = Fit0(x_vals, y_vals)
	fit1 = Fit1(x_vals, y_vals)
	fit2 = Fit2(x_vals, y_vals)

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
	plt.ylabel('Gradient magnitude')

	plot(data)
	plot(fit0)
	plot(fit1)
	plot(fit2)

	legend = plt.legend(loc='best', framealpha=0.5, scatterpoints = 256)

	plt.savefig(sys.argv[2])