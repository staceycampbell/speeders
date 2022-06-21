#include <stdio.h>
#include <math.h>
#include <stdint.h>
#include "castotas.h"
#include "metar.h"

// convert Calibrated Air Speed to True Air Speed
// https://aviation.stackexchange.com/a/64251
// cas in knots, altitude in feet

int32_t
CAStoTAS(double metar_temp_c, double metar_elevation_m, int32_t cas, int32_t altitude)
{
	int32_t tas;
	double cas_mps;
	double h;
	double T;
	static const double a0 = 340.3; // m/s is the speed of sound at sea level in the ISA,
	static const double g = 9.80665; // m/s2 is the standard acceleration due to gravity,
	static const double L = 0.0065; // K/m is the standard ISA temperature lapse rate for the troposphere,
	static const double M = 0.0289644; // kg/mol is the molar mass of dry air,
	static const double R = 8.31446261815324; // J/(mol⋅K) is the universal gas constant,
	static const double T0 = 288.15; // K is the static air temperature at sea level in the ISA.

	cas_mps = (double)cas * 0.514444; // calibrated airspeed m/s
	h = (double)altitude * 0.3048; // altitude in m

	// https://www.grc.nasa.gov/www/k-12/airplane/atmosmet.html
	// extrapolate static air temperature at aircraft altitude using
	// temp and elevation of nearby METAR source
	T = metar_temp_c - L * (h - metar_elevation_m) + 273.15; // K

	double Lh_div_T0 = (L * h) / T0;
	double neg_gM_div_RL = -((g * M) / (R * L));
	double cas_squared_div_5a0_squared = pow(cas_mps, 2.0) / (5.0 * pow(a0, 2.0));
	double sevenRT_div_M = (7.0 * R * T) / M;

	double expr0 = pow(cas_squared_div_5a0_squared + 1, 7.0 / 2.0) - 1;
	double expr1 = pow(1.0 - Lh_div_T0, neg_gM_div_RL);
	double expr2 = expr0 * expr1 + 1;
	double expr3 = pow(expr2, 2.0 / 7.0) - 1;
	double expr4 = sevenRT_div_M * expr3;
	double tas_mps = sqrt(expr4);

	tas = tas_mps * 1.94384 + 0.5; // m/s to knots

	return tas; // knots
}
