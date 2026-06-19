#include "functions.h"
#include <windows.h>
#include <commdlg.h>
#include <iostream>
#include <thread>

// worker rulat in background pentru interactiunea cu driverul de com
void SerialWorker(std::string portName, std::string payload) {
    HANDLE hSerial = CreateFileA(portName.c_str(), GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

    if (hSerial == INVALID_HANDLE_VALUE) {
        std::cout << "[ERROR] Conexiune esuata pe " << portName << ". Verifica daca robotul este pornit." << std::endl;
        return;
    }

    DCB dcbSerialParams = { 0 };
    dcbSerialParams.DCBlength = sizeof(dcbSerialParams);
    GetCommState(hSerial, &dcbSerialParams);

    dcbSerialParams.BaudRate = CBR_115200;
    dcbSerialParams.ByteSize = 8;
    dcbSerialParams.StopBits = ONESTOPBIT;
    dcbSerialParams.Parity = NOPARITY;
    SetCommState(hSerial, &dcbSerialParams);

    DWORD bytesWritten;
    WriteFile(hSerial, payload.c_str(), (DWORD)payload.length(), &bytesWritten, NULL);

    // timp de siguranta pentru golirea bufferului hardware
    Sleep(100);

    CloseHandle(hSerial);
    std::cout << "[SUCCESS] Date trimise pe " << portName << ": " << payload;
}

void SendBluetoothThreaded(const std::string& portName, const std::string& payload) {
    // detasam thread-ul ca sa ruleze complet independent de bucla raylib
    std::thread t(SerialWorker, portName, payload);
    t.detach();
}

std::string OpenFileDialog() {
    char file_name[MAX_PATH] = "";
    OPENFILENAMEA ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFilter = "Imagini (*.png;*.jpg;*.jpeg)\0*.png;*.jpg;*.jpeg\0Toate Fisierele\0*.*\0";
    ofn.lpstrFile = file_name;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
    ofn.lpstrDefExt = "png";

    if (GetOpenFileNameA(&ofn)) {
        return std::string(file_name);
    }
    return "";
}