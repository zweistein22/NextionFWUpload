#define INFOSERIAL(call) Serial.call
#include <NeoICSerial.h>
#include "NextionFirmware.h"

NeoICSerial nextion;
NextionFirmware firmware(&nextion);
 
#define NEXTIONBAUD 9600
#define SERIALBAUD 115200
#define RETRIES 5

void ErrorHandler(const char *msg);


void setup() {
  firmware.bdebug=false;
}


long length=0;

bool indataloop=false;
bool inchunkupload = false;
bool initdone=false;
long remaining=0;
long bytesread=0;
long btransferred = 0;
void ErrorHandler(const char *msg){
  // set baud back to init baud rate?
    firmware.DebugOut(msg);
    Serial.end();
   indataloop=false;
   initdone=false;
}


void loop() {
  // put your main code here, to run repeatedly:
  
  if(indataloop){
      if (inchunkupload) {
          int maxchunk = 48;

           
          long ackafter = 4096L;
          if (remaining < 0) {
              ackafter = 4096L + remaining;
              
          }

          int ichunks = ackafter/maxchunk;
          int lastchunk = ackafter % maxchunk;
          int chunksize = maxchunk;
         
          int currentchunk = btransferred / maxchunk;


          if (lastchunk!=0 && currentchunk>=ichunks) {
              chunksize = lastchunk;
              
          }
          String string = "currentchunk : ";
          string += String(currentchunk);
          string += ", btransferred=";
          string += String(btransferred);
          firmware.DebugOut(string.c_str());
          if (chunksize != 0) {
              Serial.write((uint8_t)0x05); // we signal  ready to read data from Serial
              int uploaded = firmware.upload_chunk(&Serial, chunksize);

              btransferred += uploaded;
              if (uploaded != chunksize) {
                  ErrorHandler("Error uploading last chunk");
                  return;
              }
          }
       
          if (btransferred == ackafter) {

              // so last chunk
              if (firmware.wait_acknowledge(&Serial)) {
                  if (remaining < 0) remaining = 0;
                  inchunkupload = false;
                  String string = "sent : ";
                  string += String(length - remaining);
                  string += " bytes";
                  firmware.DebugOut(string.c_str());
                  if (remaining == 0) {
                      Serial.end();
                      indataloop = false;
                      initdone = false;
                  }
              }
              
              else { ErrorHandler("Error uploading last chunk"); 
                    return;
              }
              
          }

      }
      else {
          
          remaining -= 4096L;
          btransferred = 0;
          inchunkupload = true;
          
      }
   }
  else {
    if(!initdone){
      initdone=true;
      
      nextion.begin(NEXTIONBAUD);  // Start serial comunication at baud=9600
      Serial.begin(SERIALBAUD);
      delay(50);
    }
     Serial.write("***START->");
     int avail=0;
     for(int i=0;i<RETRIES;i++){
         avail=Serial.available();
         if(avail>=4) break;
         delay(50);
     };
     if(avail<4) return;
     length=0;
     int rv = Serial.readBytes((char *) &length,sizeof(long));
     if(rv!=4) return;
     remaining=length;
     bytesread=0;
     btransferred = 0;
     if(!firmware.upload_init(length,57600)) {
       return ErrorHandler("ERROR : upload_init");
     }
     indataloop=true;
   }
}
