#include <EspUsbHost.h>
#include "EspUsbHost.h"
#include <ESP32-TWAI-CAN.hpp>
#include <Arduino_CRC32.h>
Arduino_CRC32 crc32;
unsigned long tick = 0;
unsigned long last_tick = 0;
int gone = 1;

#define CAN_TX 5
#define CAN_RX 4
unsigned long notWorking = 0;
CanFrame rxobdFrame         = {0};
CanFrame obdFrame         = {0};

void sendTSCMD(uint8_t modifier, uint8_t firstKey, uint8_t secondKey) {
  if (ESP32Can.canState() != 1) { 
    Serial.println("Can isn't working ?");
    notWorking++;
    return;
  }
    uint32_t crc32_res;
    uint8_t payload[5];
//    CanFrame obdFrame         = {0};
    obdFrame.identifier       = 0x710; // serial write;
    obdFrame.extd             = 0;
    obdFrame.data_length_code = 3;
    obdFrame.data[0]          = 0x02; // length of message to follow
    obdFrame.data[1]          = 0x00;
    obdFrame.data[2]          = 0x05;
  ESP32Can.writeFrame(obdFrame, 0);

 
    payload[0] = 0x5A;
    payload[1] = 0;
    payload[2] = 27;
    payload[3] = secondKey & 0xff;
    payload[4] = firstKey & 0xff;
    crc32_res = crc32.calc(payload, 5);

    Serial.printf("sending %02x %02x", secondKey, firstKey);
    Serial.println();
  
    obdFrame.identifier       = 0x710; // serial write;
    obdFrame.extd             = 0;
    obdFrame.data_length_code = 6;
    obdFrame.data[0]          = 0x05; // length of message to follow
    obdFrame.data[1]          = payload[0]; // Z - test buttons
    obdFrame.data[2]          = payload[1];    // TS_BUTTONBOX1_CATEGORY = 26, 0x00 0x1A
    obdFrame.data[3]          = payload[2]; // hardware button box 1 -> lookup on ECU side via lookup curve table thing; 
    obdFrame.data[4]          = payload[3]; // data 
    obdFrame.data[5]          = payload[4];  // data
    ESP32Can.writeFrame(obdFrame, 0);

    obdFrame.identifier       = 0x710; // serial write;
    obdFrame.extd             = 0;
    obdFrame.data_length_code = 5;
    obdFrame.data[0]          = 4;
    obdFrame.data[1]          = (crc32_res >> 24) & 0xff; // crc high bits
    obdFrame.data[2]          = (crc32_res >> 16) & 0xff;
    obdFrame.data[3]          = (crc32_res >> 8)  & 0xff;  
    obdFrame.data[4]          = (crc32_res     )  & 0xff; // crc low bits
    
    ESP32Can.writeFrame(obdFrame, 0);

}

class MyEspUsbHost : public EspUsbHost {
  void onGone(const usb_host_client_event_msg_t *eventMsg) {
    gone = 1;
    Serial.println("device gone");
  };
  void onReceive(const usb_transfer_t *transfer){
      int i=0;
      int modifier = 0;
      int firstKey = 0;
      int secondKey = 0;
      for (i = 0;i<transfer->data_buffer_size && i < 50;i++ ){
        Serial.printf("%02x ", transfer->data_buffer[i]);
      }
      Serial.println();

      if (transfer->num_bytes > 4 && transfer->data_buffer_size > 4) { // sanity somewhat here
        modifier  = (transfer->data_buffer[0]);
        firstKey  = (transfer->data_buffer[2]);
        secondKey = (transfer->data_buffer[3]);

         if (firstKey > 0) firstKey += (modifier * 0xff);
         if (secondKey > 0) secondKey += (modifier * 0xff);


        // return; // bit of mojibake here
        if (firstKey > 0) {
          sendTSCMD( modifier,  firstKey & 0xff,  (firstKey >> 8) & 0xff);
        } else if (secondKey > 0) {
          sendTSCMD( modifier,  secondKey & 0xff,  (secondKey >> 8) & 0xff);
        }

      }
  }
};

MyEspUsbHost usbHost;

void setup() {
  Serial.begin(115200);
  delay(500);
  int i;
  tick = xTaskGetTickCount();

gone = 0;
  usbHost.begin();
  usbHost.setHIDLocal(HID_LOCAL_Japan_Katakana);
  usbHost.task();

  ESP32Can.setPins(CAN_TX, CAN_RX);
  ESP32Can.setRxQueueSize(500);
  ESP32Can.setTxQueueSize(500);
  ESP32Can.setSpeed(ESP32Can.convertSpeed(500));
  while(!ESP32Can.begin());
}

void loop() {
  if (ESP32Can.inRxQueue() > 0) {
    ESP32Can.readFrame(rxobdFrame, 0);

  }
  if (xTaskGetTickCount() - tick > 1000) {
    tick = xTaskGetTickCount();


  Serial.printf("txq: %d, rxq: %d, rxerr: %d. txerr: %d, rxmis: %d, txmis: %d, buserr: %d, canstate: %d, notWorking: %d", 
   ESP32Can.inTxQueue(),
   ESP32Can.inRxQueue(),
   ESP32Can.rxErrorCounter(),
   ESP32Can.txErrorCounter(),
   ESP32Can.rxMissedCounter(),
   ESP32Can.txFailedCounter(),
   ESP32Can.busErrCounter(),
   ESP32Can.canState(), notWorking);
    Serial.println();


  if (notWorking > 10) {
    Serial.print(" busErrCounter - restarting esp");
    ESP.restart();
  } 
}



  usbHost.task();
}

