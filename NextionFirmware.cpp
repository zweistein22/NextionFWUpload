#include "NextionFirmware.h"

int ARDUINOSERIALBUFFERSIZE = 64;

  NextionFirmware::NextionFirmware(NeoICSerial *  nextion){

    _nextion=nextion;
    string.reserve(80);
  }

uint32_t NextionFirmware::_baudrate; /*nextion serail baudrate*/
uint32_t NextionFirmware::_fwsize; /*undownload byte of tft file*/
uint32_t NextionFirmware::_download_baudrate; /*download baudrate*/
String NextionFirmware::string =String("");
bool NextionFirmware::bdebug=true;
long nuploaded=0;

volatile int tx_completed=0;

void TX_Complete(){

  tx_completed=1;
}

void NextionFirmware::DebugOut(const char *msg){
  if(!bdebug) return;
  string = "DBG: " + String(msg);
  sendCommand(string.c_str());
  
}
bool NextionFirmware::upload_init(uint32_t fwsize,uint32_t download_baudrate){
  _fwsize=fwsize;
  nuploaded=0;
  _download_baudrate=download_baudrate;
  
    if(_getBaudrate() == 0)
    {
       return 0;
    }
    if(!_setDownloadBaudrate(_download_baudrate))
    {
        return 0;
    }
    return 1;
}

uint32_t NextionFirmware::_getBaudrate(void)
{
    uint32_t baudrate_array[7] = {115200,19200,57600,38400,9600,4800,2400};
     
    for(uint8_t i = 0; i < 7; i++)
    {
        if(_searchBaudrate(baudrate_array[i]))
        {
            _baudrate = baudrate_array[i];
          
            break;
        }
    }
    return _baudrate;
}


bool NextionFirmware::_searchBaudrate(uint32_t baudrate)
{
    _nextion->begin(baudrate);
    sendCommand("");
    sendCommand("connect");
    string="";
    recvRetString(string,500,true);  
    DebugOut(string.c_str());
    if(string.indexOf("comok") != -1)
    {
       return 1;
    } 
    return 0;
}

void NextionFirmware::sendCommand(const char* cmd)
{
   
    while (_nextion->available())
    {
        _nextion->read();
    }
    tx_completed=0;
    _nextion->attachTxCompleteInterrupt(TX_Complete);
    _nextion->write(cmd);
    _nextion->write(0xFF);_nextion->write(0xFF); _nextion->write(0xFF);
    _nextion->flush();
    while(tx_completed==0){
      delay(50);
    }
    _nextion->detachTxCompleteInterrupt();
}

uint16_t NextionFirmware::recvRetString(String &string, uint32_t timeout,bool recv_flag)
{
    uint16_t ret = 0;
    uint8_t c = 0;
    long start;
    bool exit_flag = false;
    start = millis();
    while (millis() - start <= timeout)
    {
        while (_nextion->available())
        {
            c = _nextion->read(); 
            if(c == 0)
            {
                continue;
            }
            string += (char)c;
            if(recv_flag)
            {
                if(string.indexOf(0x05) != -1)
                { 
                    exit_flag = true;
                } 
            }
        }
        if(exit_flag)
        {
            break;
        }
    }
    ret = string.length();
    return ret;
}

bool NextionFirmware::_setDownloadBaudrate(uint32_t baudrate)
{
    static String cmd = String("");
    cmd.reserve(80);
    static String filesize_str = String(_fwsize,10);
    static String baudrate_str = String(baudrate,10);
    cmd = "whmi-wri ";
    cmd+=filesize_str;
    cmd+= "," + baudrate_str + ",0";
    sendCommand("");
    sendCommand(cmd.c_str());
    _nextion->begin(baudrate);
    //delay(50);
    string="";
    recvRetString(string,500,true);  // acknowledge in new baud rate
    if(string.indexOf(0x05) != -1)
    { 
        return 1;
    } 
    return 0;
}

bool NextionFirmware::wait_acknowledge(Stream* stream) {
    String tmp = "";
    if (bdebug) return true;
    recvRetString(tmp, 500, true);
    if (tmp.indexOf(0x05) != -1) {
      //  stream->write(0x05);
        return true;
    }
    return false; 
}

long NextionFirmware::upload_chunk(Stream* stream,long bytes)
{
    int t = 0;
   
    unsigned long tstart=millis();
    unsigned long tactive = 0;
    unsigned long timeout=30000L;
    long maxavail=0;
    long pos=0;
    long bytes_sent = 0;
    if (bytes == 0) return 0;

   
    string = "uploading : ";

    for(int i=0;i<bytes;i++){
       
        int avail = 0;
        do {
            avail = stream->available();
            if (avail == 0) delay(20);
        } while (avail == 0);
        if (avail >= ARDUINOSERIALBUFFERSIZE-1) {
            string = " Serial buffer overflow: avail=";
            string += String(avail);
            DebugOut(string.c_str());
        }
         if(avail>maxavail) {
          maxavail=avail;
          pos= bytes_sent;
         }
        
         while(avail>0){
           char c = stream->read();
          
           if (!bdebug) {
               _nextion->write(c);
              // _nextion->flush();
               
           }
           else {
               delay(1);
           }
           avail--;
           bytes_sent++;
          // string = " bytes_sent=";
          // string += String(bytes_sent);
          // DebugOut(string.c_str());
           if(bytes_sent==bytes)  goto trasmitted;
           
         }
         tactive = millis() - tstart;
         if (tactive > timeout) { goto timeout_flag; }
       
    }

 timeout_flag:
    string += "TIMEOUT :";
    string += String(tactive);
    string += "ms , max=";
    string += String(timeout);
    DebugOut(string.c_str());
 trasmitted:
   
   
   
    //string =" maxavail=";
    //string+=String(maxavail);
    //string+=", pos=";
    //string+=String(pos);
    //DebugOut(string.c_str());
    
    if (true) /*bytes_sent != bytes)*/ {
        string = "bytes_sent=";
        string += String(bytes_sent);
        string += " bytes=";
        string += String(bytes);
       
    }
    DebugOut(string.c_str());
   
    return bytes_sent;
 }
