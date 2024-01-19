//#include <unity.h>
#include <Arduino.h>
#include <SPI.h>
#include "FS.h"
#include "SPIFFS.h"
#include <ArduCAM.h>

#include "memorysaver.h"
#include <Wire.h>
#include <algorithm> 

#define FORMAT_SPIFFS_IF_FAILED true
const int CS = 34;
const int CAM_POWER_ON = A10;
const int sleepTimeS = 10;

// // static const size_t bufferSize = 2048;
// // static uint8_t buffer[bufferSize] = {0xFF};
char pname[20];
//static int  index=0;
byte buf[256];
uint8_t data;
static int i = 0;
static int k = 0;
uint8_t temp = 0, temp_last = 0;
uint32_t length = 0;
bool is_header = false;
//just run for 10 frames
static uint8_t frameCount = 0;

ArduCAM myCAM(OV5642, CS);


void capture2SD(fs::FS &fs, const char * path) {
  File file ;
  //Flush the FIFO
  myCAM.flush_fifo();
  //Clear the capture done flag
  myCAM.clear_fifo_flag();
  //Start capture
  myCAM.start_capture();
  Serial.println(F("Star Capture"));
  while (!myCAM.get_bit(ARDUCHIP_TRIG , CAP_DONE_MASK));
  Serial.println(F("Capture Done."));
  length = myCAM.read_fifo_length();
  Serial.print(F("The fifo length is :"));
  Serial.println(length, DEC);
  if (length >= MAX_FIFO_SIZE) //8M
  {
    Serial.println(F("Over size."));
  }
  if (length == 0 ) //0 kb
  {
    Serial.println(F("Size is 0."));
  }
  file = fs.open(path, FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open file for writing");
    return;
  }
  i = 0;
  myCAM.CS_LOW();
  myCAM.set_fifo_burst();

  //remove the while condition and run only once 
  while ( length-- )
  {
  temp_last = temp;
  temp =  SPI.transfer(0x00);
    //Read JPEG data from FIFO
  if ( (temp == 0xD9) && (temp_last == 0xFF) ) //If find the end ,break while,
    {
      buf[i++] = temp;  //save the last  0XD9
      //Write the remain bytes in the buffer
      myCAM.CS_HIGH();
      file.write(buf, i);
      //Close the file
      file.close();
      Serial.println(F("Image save OK."));
      //delay(5000);
      is_header = false;
      i = 0;
    }
  if (is_header == true)
    {
      //Write image data to buffer if not full
      if (i < 256)
        buf[i++] = temp;
      else
      {
        //resize_image(buf,320,240,buf,96,96,3);
        //Write 256 bytes image data to file
        myCAM.CS_HIGH();
        file.write(buf, 256);
        i = 0;
        buf[i++] = temp;
        myCAM.CS_LOW();
        myCAM.set_fifo_burst();
      }
    }
  else if ((temp == 0xD8) & (temp_last == 0xFF))
    {
      is_header = true;
      buf[i++] = temp_last;
      buf[i++] = temp;
    }
    //delay(5000);
  }
  delay(2000);
}



void listDir(fs::FS &fs, const char * dirname, uint8_t levels) {
  Serial.printf("Listing directory: %s\r\n", dirname);

  File root = fs.open(dirname);
  if (!root) {
    Serial.println("- failed to open directory");
    return;
  }
  if (!root.isDirectory()) {
    Serial.println(" - not a directory");
    return;
  }

  File file = root.openNextFile();
  while (file) {
    if (file.isDirectory()) {
      Serial.print("  DIR : ");
      Serial.println(file.name());
      if (levels) {
        listDir(fs, file.name(), levels - 1);
      }
    } else {
      Serial.print("  FILE: ");
      Serial.print(file.name());
      Serial.print("\tSIZE: ");
      Serial.println(file.size());
    }
    file = root.openNextFile();
  }
}

void readImage(fs::FS &fs, const char * path) {
  Serial.printf("Reading Image: %s\r\n", path);
  File file = fs.open(path);
  if (!file || file.isDirectory()) {
    Serial.println("- failed to open file for reading");
    return;
  }

  Serial.println("- read from file:");
  const int bufferSize = 256;  // Adjust the buffer size as needed
  uint8_t buffer[bufferSize];
  while (file.available()) {
    Serial.write(file.read());
    //Serial.println(file.read(), HEX);
  }
  Serial.println();
  file.close();
  Serial.println("\nImage sent.");
}

void deleteFile(fs::FS &fs, const char * path){
    Serial.printf("Deleting file: %s\r\n", path);
    if(fs.remove(path)){
        Serial.println("- file deleted");
    } else {
        Serial.println("- delete failed");
    }
}
void setup(){
  uint8_t vid, pid;
  uint8_t temp;
  static int i = 0;
  
  pinMode(CS, OUTPUT);
  pinMode(CAM_POWER_ON , OUTPUT);
  digitalWrite(CAM_POWER_ON, HIGH);
  Wire.begin();
  Serial.begin(115200);
  Serial.println(F("ArduCAM Start!"));

  SPI.begin();
  while (1)
  {
    myCAM.write_reg(ARDUCHIP_TEST1, 0x55);
    temp = myCAM.read_reg(ARDUCHIP_TEST1);
    if (temp != 0x55) {
      Serial.println(F("SPI interface Error!"));
      delay(2);
      continue;
    }
    else
      break;
  }
  //Add support for SPIFFS
  if (!SPIFFS.begin(FORMAT_SPIFFS_IF_FAILED)) {
    Serial.println("SPIFFS Mount Failed");
    return;
  }
  Serial.println("SPIFFS Mount Successful");

  myCAM.wrSensorReg16_8(0xff, 0x01);
  myCAM.rdSensorReg16_8(OV5642_CHIPID_HIGH, &vid);
  myCAM.rdSensorReg16_8(OV5642_CHIPID_LOW, &pid);

  if ((vid != 0x56) || (pid != 0x42)) {
    Serial.println(F("Can't find OV5642 module!"));
  }
  else
    Serial.println(F("OV5642 detected."));

  myCAM.set_format(JPEG);
  myCAM.InitCAM();

  myCAM.write_reg(ARDUCHIP_TIM, VSYNC_LEVEL_MASK);   //VSYNC is active HIGH
  myCAM.OV5642_set_JPEG_size(OV5642_320x240);

  delay(1000);
}

void loop() {
 while (frameCount < 30) {
  sprintf((char*)pname, "/%05d.jpg", k);
  capture2SD(SPIFFS, pname);
  listDir(SPIFFS, "/", 0);
  //readImage(SPIFFS, pname);
  //deleteFile(SPIFFS, pname);
  k++;
  frameCount++;
  delay(3000);
 }
  //listDir(SPIFFS, "/", 0);
  Serial.println(F("sleep 10 secs"));
  //frameCount = 0;
  ESP.deepSleep(sleepTimeS * 1000000);//ESP32 sleep 10s
  //Serial.println(frameCount);
  //listDir(SPIFFS, "/", 0);
  //delay(5000);
  //}
  
  //while (1);
}



///////////////////////////////////////////////////////////////////////////////////////////////////
// void camCapture(ArduCAM myCAM)
// {
//   uint32_t len  = myCAM.read_fifo_length();
//   if (len >= MAX_FIFO_SIZE) //8M
//   {
//     Serial.println(F("Over size."));
//   }
//   if (len == 0 ) //0 kb
//   {
//     Serial.println(F("Size is 0."));
//   }

//   myCAM.CS_LOW();
//   myCAM.set_fifo_burst();

//   i = 0;
//   while ( len-- )
//   {
//     temp_last = temp;
//     temp =  SPI.transfer(0x00);

//     if ( (temp == 0xD9) && (temp_last == 0xFF) )
//     {
//       buffer[i++] = temp;  //save the last  0XD9 
//       is_header = false;
//       i = 0;
//       myCAM.CS_HIGH();
//       break;
//     }
//     if (is_header == true)
//     {
//       if (i < bufferSize)
//       {
//         buffer[i++] = temp;
//       }else
//       {
//         i = 0;
//         buffer[i++] = temp;
//       }
//     } 
//     else if ((temp == 0xD8) & (temp_last == 0xFF))
//     {
//     is_header = true;
//     buffer[i++] = temp_last;
//     buffer[i++] = temp;   
//     } 
//   }
// }

// void camCapture(ArduCAM myCAM)
// {
//   uint32_t len = myCAM.read_fifo_length();
//   if (len >= MAX_FIFO_SIZE) //8M
//   {
//     Serial.println(F("Over size."));
//   }
//   if (len == 0 ) //0 kb
//   {
//     Serial.println(F("Size is 0."));
//   }

//   myCAM.CS_LOW();
//   myCAM.set_fifo_burst();

//   // Serial.write(0xFF);
//   // Serial.write(0xD8); 

//   while (len--) {
//     temp_last = temp;
//     uint8_t temp = SPI.transfer(0x00);

//     // Send image data to PC through serial
    
//     if ((temp == 0xD9) && (temp_last == 0xFF)) {
//       buffer[i++] = temp;  //save the last  0XD9 
//       Serial.write(&buffer[0], i);
//       is_header = false;
//       i = 0;
//       myCAM.CS_HIGH();
//       break; // End of JPEG marker
//     }
//     if (is_header == true){
//       if (i < bufferSize)
//     buffer[i++] = temp;
//     else
//     {
//       Serial.write(&buffer[0], bufferSize);
//       i = 0;
//       buffer[i++] = temp;
//     }
//     }
//     else if ((temp == 0xD8) & (temp_last == 0xFF))
//     {
//     is_header = true;
//     buffer[i++] = temp_last;
//     buffer[i++] = temp;   
//     } 
//   }
// }

// void camCapture(ArduCAM myCAM) {
//   uint32_t len = myCAM.read_fifo_length();
  
//   Serial.print("Image size: ");
//   Serial.println(len);

//   if (len >= MAX_FIFO_SIZE) {
//     Serial.println(F("Over size."));
//   }

//   if (len == 0) {
//     Serial.println(F("Size is 0."));
//   }

//   myCAM.CS_LOW();
//   //Serial.print("cam low");
//   myCAM.set_fifo_burst();
//   //Serial.print("cam fifo");
  
//   Serial.write(0xFF);
//   Serial.write(0xD8); // Start of JPEG marker

//   while (len--) {
//     uint8_t temp_last = temp;
//     //Serial.print("while");
//     uint8_t temp = SPI.transfer(0x00);
//     //Serial.print("spi int");
//     // Serial.print(F("temp: 0x"));
//     // Serial.print(temp, HEX);
//     // Serial.print(F(", temp_last: 0x"));
//     // Serial.println(temp_last, HEX);
//     Serial.write(temp);
//     if ((temp == 0xD9) && (temp_last == 0xFF)) {
//       Serial.print("if loop");
//       // End of JPEG marker, send the image data
//       buffer[i++] = temp;
//       Serial.println(F("Sending image data..."));
//       Serial.write(&buffer[0], i);
//       is_header = false;
//       i = 0;
//       myCAM.CS_HIGH();
//       break;
//     }

//     if (is_header) {
//       // Write image data to buffer if not full
//       if (i < bufferSize)
//         buffer[i++] = temp;
//       else {
//         // Write bufferSize bytes image data to serial
//         Serial.println(F("Buffer full, sending..."));
//         Serial.write(&buffer[0], bufferSize);
//         i = 0;
//         buffer[i++] = temp;
//       }
//     } else if ((temp == 0xD8) && (temp_last == 0xFF)) {
//       // Start of JPEG marker, set header flag
//       is_header = true;
//       buffer[i++] = temp_last;
//       buffer[i++] = temp;
//       Serial.println(F("JPEG header detected."));
//     }
//   }
// }


// void serverCapture(){
//   delay(1000);
// start_capture();
// Serial.println(F("CAM Capturing"));

// int total_time = 0;
// total_time = millis();
// while (!myCAM.get_bit(ARDUCHIP_TRIG, CAP_DONE_MASK));
// total_time = millis() - total_time;
// Serial.print(F("capture total_time used (in miliseconds):"));
// Serial.println(total_time, DEC);

// total_time = 0;

// Serial.println(F("CAM Capture Done."));
// total_time = millis();
// camCapture(myCAM);
// total_time = millis() - total_time;
// Serial.print(F("send total_time used (in miliseconds):"));
// Serial.println(total_time, DEC);
// Serial.println(F("CAM send Done."));
// }
/////////////////////////////////////////////////////////////////////////////
//server
// void setup() {
//   uint8_t vid, pid;
//   uint8_t temp;
//   static int i = 0;
//     //set the CS as an output:
//   pinMode(CS,OUTPUT);
//   pinMode(CAM_POWER_ON , OUTPUT);
//   digitalWrite(CAM_POWER_ON, HIGH);

//   #if defined(__SAM3X8E__)
//     Wire1.begin();
//     #else
//     Wire.begin();
//     #endif

//   Serial.begin(115200);
//   Serial.println(F("ArduCAM Start!"));

//   SPI.begin();
//   SPI.setFrequency(8000000); //8MHz
//   //Check if the ArduCAM SPI bus is OK
//   myCAM.write_reg(ARDUCHIP_TEST1, 0x55);
//   temp = myCAM.read_reg(ARDUCHIP_TEST1);
//   if (temp != 0x55){
//   Serial.println(F("SPI1 interface Error!"));
//   while(1);
//   }

//   #if defined (OV5642_MINI_5MP_PLUS) || defined (OV5642_MINI_5MP) || defined (OV5642_MINI_5MP_BIT_ROTATION_FIXED) ||(defined (OV5642_CAM))
//    //Check if the camera module type is OV5642
//     myCAM.wrSensorReg16_8(0xff, 0x01);
//     myCAM.rdSensorReg16_8(OV5642_CHIPID_HIGH, &vid);
//     myCAM.rdSensorReg16_8(OV5642_CHIPID_LOW, &pid);
//     if((vid != 0x56) || (pid != 0x42)){
//     Serial.println(F("Can't find OV5642 module!"));
//     }
//     else
//     Serial.println(F("OV5642 detected."));
//     #endif
//     myCAM.write_reg(ARDUCHIP_TIM, VSYNC_LEVEL_MASK);   //VSYNC is active HIGH
//     myCAM.OV5640_set_JPEG_size(OV5642_320x240);  
//     myCAM.clear_fifo_flag();
//   //RUN_UNITY_TESTS();
// }

// void loop() {
//   // put your main code here, to run repeatedly:
  
//   //Serial.println(F("OV5642 detected main loop."));
//   myCAM.set_format(JPEG);
//   myCAM.InitCAM();
  
//   start_capture();
//   //Serial.println(F("CAM Capturing"));

//   int total_time = 0;
//   total_time = millis();
//   //Serial.print("ARDUCHIP_TRIG: 0x");
//   //Serial.println(myCAM.read_reg(ARDUCHIP_TRIG), HEX);
//   delay(100);
//   while(!myCAM.get_bit(ARDUCHIP_TRIG, CAP_DONE_MASK));
// //   {
// //     Serial.println(myCAM.read_reg(ARDUCHIP_TRIG), HEX);
// //   // Add more debug prints if needed
// //     delay(100);  // Adjust the delay as needed
// //  }
//   total_time = millis() - total_time;
//   //Serial.print(F("send total_time used (in miliseconds):"));
//   //Serial.println(total_time, DEC);
//   total_time = 0;

//   //Serial.println(F("CAM Capture Done."));

//   total_time = millis();
//   camCapture(myCAM);
//   total_time = millis() - total_time;
//   //Serial.print(F("send total_time used (in miliseconds):"));
//   //Serial.println(total_time, DEC);
//   //Serial.println(F("CAM send Done."));
//   delay(1000);
// }



///////////////////////////////////////////////////////////////////////////////////////////////////
//spiflash