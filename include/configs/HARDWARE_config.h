#pragma once

// Board
#define SERIAL_MON_BAUD 115200 // Baud rate for communicating between Arduino and PC

// SIMCOM
HardwareSerial SerialAT_4g(1); // Between Arduino and Simcom board
#define SIM_RX_pin 33
#define SIM_TX_pin 32
#define SERIAL_AT_SIMCOM_BAUD 115200
#define RESERVED_NOISE_PIN GPIO_NUM_0
#define SIM7600x // alternatives: SIM7070G, A7672x, SIM7000x, SIM7600x

//-- APN SETTINGS
const char APN[] = "em"; // Your GPRS credentials, if any