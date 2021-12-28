/*
  thermistor.cpp - Thermistor Library
*/
#include "thermistor.h"

#define ABS_ZERO -273.15

Thermistor::Thermistor(
	int pin,
	double vcc,
	double analogReference,
	int adcMax,
	int seriesResistor,
	int thermistorNominal,
	int temperatureNominal,
	int bCoef,
	int samples,
	int sampleDelay) : _pin(pin),
					   _vcc(vcc),
					   _analogReference(analogReference),
					   _adcMax(adcMax),
					   _seriesResistor(seriesResistor),
					   _thermistorNominal(thermistorNominal),
					   _temperatureNominal(temperatureNominal),
					   _bCoef(bCoef),
					   _samples(samples),
					   _sampleDelay(sampleDelay)
{
	pinMode(_pin, INPUT);
}

double Thermistor::readADC() const
{
	unsigned sum = 0;
	for (int i = 0; i < _samples - 1; i++)
	{
		sum += analogRead(_pin);
		delay(_sampleDelay);
	}
	sum += analogRead(_pin);
	return (1. * sum) / _samples;
	//return sum / _samples;
}

double Thermistor::readTempK() const
{
	return adcToK(readADC());
}

double Thermistor::readTempC() const
{
	return kToC(readTempK());
}

double Thermistor::readTempF() const
{
	return cToF(readTempC());
}

double Thermistor::adcToK(double adc) const
{
	// double resistance = -1.0 * (_analogReference * _seriesResistor * adc) / (_analogReference * adc - _vcc * _adcMax);
	// double steinhart = (1.0 / (_temperatureNominal - ABS_ZERO)) + (1.0 / _bCoef) * log(resistance / _thermistorNominal);
	// double kelvin = 1.0 / steinhart;

	float c1 = 1.009249522e-03, c2 = 2.378405444e-04, c3 = 2.019202697e-07;
	double logR2 = log(_seriesResistor * (_adcMax / (float)adc - 1.0));
	double kelvin = (1.0 / (c1 + c2 * logR2 + c3 * logR2 * logR2 * logR2));
	return kelvin;
}

double Thermistor::kToC(double k) const
{
	double c = k + ABS_ZERO;
	return c;
}

double Thermistor::cToF(double c) const
{
	return (c * 1.8) + 32;
}
