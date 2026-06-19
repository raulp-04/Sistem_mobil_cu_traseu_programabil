#ifndef FUNCTIONS_H
#define FUNCTIONS_H

#include <string>

void SendBluetoothThreaded(const std::string& portName, const std::string& payload);
std::string OpenFileDialog();

#endif