#ifndef THERMISTOR_HELPER_H
#define THREMISTOR_HELPER_H

// Thermistor parameters (adjust based on your thermistor's datasheet)
const float R_NOMINAL = 10000.0;      // Resistance at nominal temperature (e.g., 10kΩ at 25°C)
const float T_NOMINAL = 18.3;         // Nominal temperature in Celsius
const float B_COEFFICIENT = 3950.0;   // 3950.0;   // Beta coefficient
const float VCC = 3.3;                // Pico's supply voltage
const float FIXED_RESISTOR = 10000.0; // Value of the fixed resistor in the voltage divider

uint16_t getTemperature(uint16_t raw_adc)
{
    uint16_t conversion = 0;
    // Function definition: calculates the sum and returns it.
    float temperature_c;
    // Convert raw ADC to voltage
    float adc_voltage = raw_adc * (VCC / 4095.0); // 4095 for 12-bit ADC
    // printf("ADC voltage: %f\n", adc_voltage);
    //  Calculate thermistor resistance using voltage divider formula
    float thermistor_resistance = FIXED_RESISTOR / ((VCC / adc_voltage) - 1);
    // Apply Steinhart-Hart equation to calculate temperature in Kelvin
    float steinhart_temp_k = 1.0 / ((1.0 / (T_NOMINAL + 273.15)) + (log(thermistor_resistance / R_NOMINAL) / B_COEFFICIENT));
    // Convert Kelvin to Celsius
    temperature_c = steinhart_temp_k - 273.15; // Calculate Centigrade.
    conversion = (int)temperature_c;
    return (int)conversion;
}
#endif