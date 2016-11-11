// Code upadates from https://github.com/chendry/LSY201
//-----------------------------------------------------------

//#include <SoftwareSerial.h>
#include <LSY201.h>

/* assuming the TX and RX pins on the camera are attached to pins 2 and 3 of
 * the arduino. */
//SoftwareSerial camera_serial(2, 3);
//SoftwareSerial camera_serial(4, 5);

#define camera_serial Serial1

LSY201 camera;
uint8_t buf[32*2];

void setup()
{

  Serial.begin(38400);
  Serial.println("start setup...");
  //camera.reset();
  camera.setSerial(camera_serial);
  Serial.println("after setserial...");
  
  //camera.setBaudRate(19200);
  //camera_serial.begin(19200);
  //camera.setBaudRate(38400);
  
  camera_serial.begin(38400);
  Serial.println("after setbaudrate...");
  delay(10000);
  //camera.setDebugSerial(Serial);
  Serial.println("end setup...");
  camera.reset();
  delay(10000);

}

void loop()
{
#ifdef ICHRAK
  camera.reset();
  delay(10000);
#endif
	
  Serial.println("Taking picture...");
  camera.takePicture();

  Serial.println("Bytes:");

  uint16_t offset = 0;
  while (camera.readJpegFileContent(offset, buf, sizeof(buf)))
  {
    for (int i = 0; i < sizeof(buf); i ++) {
#ifdef ICHRAK
	  Serial.print("@");
      Serial.println(buf[i], HEX);
#endif
	  Serial.print((char)buf[i]);
	}
	
    offset += sizeof(buf);
  }

  Serial.println("Done.");
  delay(1000);
  camera.stopTakingPictures(); //Otherwise picture won't update
  delay(3000);
#ifdef ICHRAK
  delay(10000);
#endif
}

