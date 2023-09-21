
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <Windows.h>

class SerialPort {
public:
    SerialPort(const wchar_t* portName) {
        hSerial = INVALID_HANDLE_VALUE;
        char clessidra[8] = { '|', '/', '-', '\\', '|', '/', '-', '\\' };
        int l = 0;
        while (hSerial == INVALID_HANDLE_VALUE) {
            hSerial = CreateFile(portName, GENERIC_WRITE | GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

            if (hSerial == INVALID_HANDLE_VALUE) {
                std::cout << "\r" << clessidra[l++ % 8] << " Failed to open serial port: " << portName << ". Retrying...";
                Sleep(1000); // Wait for a second before retrying
            }
        }
    }

    ~SerialPort() {
        CloseHandle(hSerial);
        std::cout << "Serial port closed." << std::endl;
    }

    LPCTSTR lastError() {
        DWORD error = GetLastError();

        LPTSTR msgBuf = nullptr;
        FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            nullptr, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            reinterpret_cast<LPTSTR>(&msgBuf), 0, nullptr);
        return msgBuf;
    }

    void flush() {
        if (!FlushFileBuffers(hSerial)) {
            throw std::runtime_error(reinterpret_cast<const char*>(lastError()));
        }
    }
    bool devicewaitreadytoread(unsigned int timeout) {
        char c = 0;
        unsigned int tstart = GetTickCount64();
        do {
           
            try {
                c = read();
                //std::cout << c << std::endl;
                unsigned int tcur = GetTickCount64();
                if (tcur - tstart > timeout) throw std::runtime_error("timeout : device readytoread.");
            }
            catch (const std::exception& e) {
                std::cerr << "Error: " << e.what() << std::endl;
            }
        } while (c != 0x05);
        return true;
    }
    char read() {
        DWORD bytesRead;
        char buffer[1] = { 0 };
        if (!ReadFile(hSerial, buffer, 1, &bytesRead, nullptr)) {
            throw std::runtime_error(reinterpret_cast<const char*>(lastError()));
        }
        return buffer[0];
    }

    void writeinchunkswithchecks(const void* data, size_t size, int chunksize) {

        int chunks = size / chunksize;
        int remainder = size % chunksize;
        DWORD bytesWritten;
        int timeout = 5000;
        for (int i = 0; i < chunks; i++) {

            devicewaitreadytoread(timeout);
            if (!WriteFile(hSerial, (char*)data + (i * chunksize), static_cast<DWORD>(chunksize), &bytesWritten, nullptr)) {
                throw std::runtime_error(reinterpret_cast<const char*>(lastError()));
            }
           
        }

        if (remainder > 0) {
            devicewaitreadytoread(timeout);
            if (!WriteFile(hSerial, (char*)data + (chunks * chunksize), static_cast<DWORD>(remainder), &bytesWritten, nullptr)) {
                throw std::runtime_error(reinterpret_cast<const char*>(lastError()));
            }
           
        }


    }
    void write(const void* data, size_t size) {
        DWORD bytesWritten;
            if (!WriteFile(hSerial, (char *)data, static_cast<DWORD>(size), &bytesWritten, nullptr)) {
                throw std::runtime_error(reinterpret_cast<const char*>(lastError()));
            }
    }

    HANDLE handle() const {
        return hSerial;
    }

private:
    HANDLE hSerial;
};

class RingBuffer {
public:
    explicit RingBuffer(size_t capacity) : buffer_(capacity), start_(0), end_(0), full_(false) {}

    void push_back(char c) {
        buffer_[end_] = c;
        if (full_) {
            start_ = (start_ + 1) % buffer_.size();
        }
        end_ = (end_ + 1) % buffer_.size();
        full_ = start_ == end_;
    }

    std::string str() const {
        if (start_ < end_) {
            return std::string(&buffer_[start_], &buffer_[end_]);
        }
        else {
            std::string firstPart(&buffer_[start_], &buffer_.back() + 1);
            firstPart.append(&buffer_.front(), &buffer_[end_]);
            return firstPart;
        }
    }

private:
    std::vector<char> buffer_;
    size_t start_;
    size_t end_;
    bool full_;
};

int wmain(int argc, wchar_t* argv[]){
    int rv = 0;
    try {
        if (argc < 2) {
            throw std::runtime_error("Please provide the COM port name as a command line argument.");
        }
       
        const wchar_t* portName = reinterpret_cast<const wchar_t*>(argv[1]);

        // Open serial port
        SerialPort serialPort(portName);

        // Set port parameters
        DCB dcbSerialParams = { 0 };
        dcbSerialParams.DCBlength = sizeof(dcbSerialParams);
        GetCommState(serialPort.handle(), &dcbSerialParams);
        dcbSerialParams.BaudRate = CBR_115200;  // CBR_9600;
        dcbSerialParams.ByteSize = 8;
        dcbSerialParams.StopBits = ONESTOPBIT;
        dcbSerialParams.Parity = NOPARITY;
        if (!SetCommState(serialPort.handle(), &dcbSerialParams)) {
            throw std::runtime_error("Failed to set serial port parameters.");
        }

        COMMTIMEOUTS timeouts;
        timeouts.ReadIntervalTimeout = 50;
        timeouts.ReadTotalTimeoutConstant = 10;
        timeouts.ReadTotalTimeoutMultiplier = 10;
        timeouts.WriteTotalTimeoutConstant = 50;
        timeouts.WriteTotalTimeoutMultiplier = 10;

        if (!SetCommTimeouts(serialPort.handle(), &timeouts)) {
            throw std::runtime_error("Error in SetCommTimeouts.");
        }

        // Listen to the serial port
        std::cout << "Waiting for Arduino..." << std::endl;
        RingBuffer rbuffer(1024);
        std::string message;
        const char* signature = "***START->";
        do {
            Sleep(10);
            char c = serialPort.read();
            rbuffer.push_back(c);
        } while (rbuffer.str().find(signature) == std::string::npos);

        std::cout << "Found signature: " << signature << std::endl;

        const char* filename = "upload.tft";

        std::ifstream file(filename, std::ios::binary | std::ios::ate);
        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);

        std::cout << "upload.tft: " << size << " bytes." << std::endl;

        // Now send in 4096-byte chunks
        long bytesWritten = 0;
        char buffer[4096];

        if (size > 0) {
            serialPort.write(&size, sizeof(size));
        }

        Sleep(2000);
        serialPort.flush();

        while (file.read(buffer, sizeof(buffer))) {
            // write this in 48 byte chunks with checks

            serialPort.writeinchunkswithchecks(buffer, sizeof(buffer), 48);
            //serialPort.write(buffer, sizeof(buffer));
            //serialPort.devicewaitreadytoread(5000);
           

            bytesWritten += sizeof(buffer);
            std::cout << "\rSent " << bytesWritten << " bytes to Arduino." << std::endl;
        }

        size_t remaining = file.gcount();
        if (remaining > 0) {
            serialPort.writeinchunkswithchecks(buffer, remaining, 48);
            bytesWritten += remaining;
        }
        std::cout << "Sent " << bytesWritten << " bytes to Arduino." << std::endl;
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        rv = 1;
    }

    std::cout << "Enter a non-empty key + [enter] to exit." << std::endl;
    char c;
    std::cin >> c;
    return rv;
}
