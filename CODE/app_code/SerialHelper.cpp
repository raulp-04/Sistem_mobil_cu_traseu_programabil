#include "SerialHelper.h"
#include <windows.h>
#include <iostream>

void SendBluetooth(std::string portName) {
    HANDLE hSerial = CreateFileA(portName.c_str(), GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

    if (hSerial == INVALID_HANDLE_VALUE) {
        std::cout << "Conexiune esuata." << std::endl;
        return;
    }

    DCB dcbSerialParams = { 0 };
    dcbSerialParams.DCBlength = sizeof(dcbSerialParams);
    GetCommState(hSerial, &dcbSerialParams);
    dcbSerialParams.BaudRate = CBR_115200;
    SetCommState(hSerial, &dcbSerialParams);

    std::string mesaj = "TEST";
    DWORD bytesWritten;
    WriteFile(hSerial, mesaj.c_str(), (DWORD)mesaj.length(), &bytesWritten, NULL);

    CloseHandle(hSerial);
    std::cout << "Semnal trimis." << std::endl;
}