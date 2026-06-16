#ifndef SERIAL_HELPER_H
#define SERIAL_HELPER_H

#include <string>

void SendBluetoothThreaded(const std::string& portName, const std::string& payload);

#endif