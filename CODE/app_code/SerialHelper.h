#ifndef SERIAL_HELPER_H
#define SERIAL_HELPER_H

#include <string>

// lanseaza trimiterea pe un thread separat ca sa nu blocheze interfata
void SendBluetoothThreaded(const std::string& portName, const std::string& payload);

#endif