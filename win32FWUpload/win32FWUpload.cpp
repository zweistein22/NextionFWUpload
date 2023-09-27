
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <Windows.h>



std::string string_to_hex(const std::string& input)
{
    static const char hex_digits[] = "0123456789ABCDEF";

    std::string output;
    output.reserve(input.length() * 2);
    for (unsigned char c : input)
    {
        output.push_back(hex_digits[c >> 4]);
        output.push_back(hex_digits[c & 15]);
    }
    return output;
}

class SerialPort {
public:
    std::wstring port = L"";
    SerialPort(const wchar_t* portName) {
        port = portName;

        open();
    }


    void open(){
        hSerial = INVALID_HANDLE_VALUE;
        char clessidra[8] = { '|', '/', '-', '\\', '|', '/', '-', '\\' };
        int l = 0;
        while (hSerial == INVALID_HANDLE_VALUE) {
            hSerial = CreateFile(port.c_str(), GENERIC_WRITE | GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

            if (hSerial == INVALID_HANDLE_VALUE) {
                std::cout << "\r" << clessidra[l++ % 8] << " Failed to open serial port: " << port.c_str() << ". Retrying...";
                Sleep(1000); // Wait for a second before retrying
            }
        }
    }

    void close() {

        CloseHandle(hSerial);
        std::cout << "Serial port closed." << std::endl;
    }

    ~SerialPort() {
        close();
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

    unsigned int _fwsize = 0;
    unsigned int nuploaded;
    unsigned int _download_baudrate;
    unsigned int  _baudrate=9600;

    bool upload_init(unsigned int fwsize, unsigned int download_baudrate) {
        _fwsize = fwsize;
        nuploaded = 0;
        _download_baudrate = download_baudrate;

        if (_getBaudrate() == 0)
        {
            return 0;
        }
        if (!_setDownloadBaudrate(_download_baudrate))
        {
            return 0;
        }
        return 1;
    }
    unsigned int _getBaudrate(void)
    {
        uint32_t baudrate_array[7] = { 9600,115200,57600,38400,19200,4800,2400 };

        for (uint8_t i = 0; i < 7; i++)
        {
            if (_searchBaudrate(baudrate_array[i]))
            {
                _baudrate = baudrate_array[i];

                break;
            }
        }
        return _baudrate;
    }

    void setbaudrate(unsigned int baudrate) {
        DCB dcbSerialParams = { 0 };
        dcbSerialParams.DCBlength = sizeof(dcbSerialParams);
        GetCommState(hSerial, &dcbSerialParams);
        dcbSerialParams.BaudRate = baudrate;  // CBR_9600;
        dcbSerialParams.ByteSize = 8;
        dcbSerialParams.StopBits = ONESTOPBIT;
        dcbSerialParams.Parity = NOPARITY;
        if (!SetCommState(hSerial, &dcbSerialParams)) {
            throw std::runtime_error("Failed to set serial port parameters.");
        }

        COMMTIMEOUTS timeouts;
        timeouts.ReadIntervalTimeout = MAXDWORD;
        timeouts.ReadTotalTimeoutConstant = 500;
        timeouts.ReadTotalTimeoutMultiplier = MAXDWORD;
        timeouts.WriteTotalTimeoutConstant = 500;
        timeouts.WriteTotalTimeoutMultiplier = MAXDWORD;

        if (!SetCommTimeouts(hSerial, &timeouts)) {
            throw std::runtime_error("Error in SetCommTimeouts.");
        }
        Sleep(ms_delay);

    }
    bool _searchBaudrate(unsigned int baudrate)
    {
        std::cout << baudrate << "baud." << std::endl;
      //  close();
     //   open();
        flush();
        setbaudrate(baudrate);
       
        std::string s = "";
        
         sendCommand("DRAKJHSUYDGBNCJHGJKSHBDN");
         send0Command();
         sendCommand("connect");
         sendCommand("ÿÿconnect");
        
          
         s = "";
         recvRetString(s, 800, true);
       
      
        std::cout<<s<<std::endl;
        if (s.length()>0 && s.find("comok") >= 0)
        {
            recvRetString(s, 500, true);
            return 1;
        }
        return 0;
    }
    int ms_delay = 100;
    bool _setDownloadBaudrate(unsigned int baudrate)
    {
        std::string rstr = "";
        sendCommand("runmode=2");
        recvRetString(rstr, 500, true);  
        sendCommand("print \"mystop_yesABC\"");
        recvRetString(rstr, 500, true);
        sendCommand("get sleepÿÿÿget dimÿÿÿget baudÿÿÿprints \"ABC\",0");
        
        rstr = "";
   
        recvRetString(rstr, 500, true);  
       
        Sleep(80);
        std::string cmd = "";
        std::string filesize_str = std::to_string(_fwsize);
        std::string baudrate_str = std::to_string(baudrate);
        cmd = "00ÿÿÿwhmi-wri ";
        cmd += filesize_str;
        cmd += "," + baudrate_str + ",1";
        
      
        sendCommand(cmd.c_str());
        flush();
        close();
        open();
   
        setbaudrate(baudrate);
        
        rstr = "";
        recvRetString(rstr, 500, true);  
        std::cout << string_to_hex(rstr) << std::endl;
      
        if (rstr.find(0x05) != -1)
        {
            
            return 1;
        }
        return 0;
    }


    unsigned int recvRetString(std::string &str, unsigned int timeout, bool recv_flag)
    {

        COMMTIMEOUTS        commtimeouts;

        GetCommTimeouts(hSerial, &commtimeouts);

        commtimeouts.ReadIntervalTimeout = MAXDWORD;
        commtimeouts.ReadTotalTimeoutMultiplier = MAXDWORD;
        commtimeouts.ReadTotalTimeoutConstant = timeout;
      
        SetCommTimeouts(hSerial, &commtimeouts);

      
        char c = 0;
        try {
            DWORD bytesRead;
            char buffer[200] = { 0 };
            if (!ReadFile(hSerial, buffer,200, &bytesRead, nullptr)) {
                throw std::runtime_error(reinterpret_cast<const char*>(lastError()));
            }
            for (int i = 0; i < min(200, bytesRead); i++) {

                str += (char)buffer[i];
            }
            
        }
        catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << std::endl;
        }
       
        return str.length();


    }
   
    void send0Command() {
        char buf[4];
        buf[0] = 0;
        buf[1]=buf[2]=buf[3] = 0xff;
        write(buf,4);
        Sleep(ms_delay);
    }

    void sendCommand(const char* cmd) {

        std::string buf = cmd;
        buf += 0xff;
        buf += 0xff;
        buf += 0xff;
        write(buf.c_str(), buf.length());
        Sleep(ms_delay);
    }

private:
    HANDLE hSerial;
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
      
        // Listen to the serial port
      

        const char* filename = "upload.tft";

        std::ifstream file(filename, std::ios::binary | std::ios::ate);
        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);
      

        std::cout << "upload.tft: " << size << " bytes." << std::endl;

        if (!serialPort.upload_init(size, 57600)) return -1;

        // Now send in 4096-byte chunks
        long bytesWritten = 0;
        char buffer[4096];

             
       // Sleep(2000);
       // serialPort.flush();
        std::string rstr = "";
        int k = 0;
        while (file.read(buffer, sizeof(buffer))) {
            int retry = 1;

            do {
                serialPort.write(buffer, sizeof(buffer));
                serialPort.flush();
                rstr = "";
               
                serialPort.recvRetString(rstr,500, true);  
                std::cout << string_to_hex(rstr) << std::endl;
                if (rstr.find(0x05) == -1)
                {
                
                    retry--;
                }
                else {
                    break;
                }
            } while (retry > 0);

            if (retry <= 0) {

                throw std::runtime_error("ERROR : 0x05 not received. ");
            }
         

            bytesWritten += sizeof(buffer);
            std::cout << "\rSent " << bytesWritten << " bytes to Nextion." << std::endl;
        }

        size_t remaining = file.gcount();
        if (remaining > 0) {
            serialPort.write(buffer, remaining);
            bytesWritten += remaining;
        }
        std::cout << "Sent " << bytesWritten << " bytes to Nextion." << std::endl;
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
