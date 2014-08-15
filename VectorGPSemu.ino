#include "I2C.h"

uint8_t vectmode=0;
uint8_t vectdata;

uint8_t databuf[37] = { 0xFA,0x24,0x02,0x8A,0x13,0xCB,0x1B,0x00,
                        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
                        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
                        0x00,0x00,0x00,0x00,0x60,0x61,0x30,0x07,
                        0x18,0x00,0x14,0x00,0x80};
uint8_t *sendptr;
uint8_t sendremain=0;

uint32_t lastgps=0;

void stepTime(uint8_t buf[]) {
  buf[0]+= 0x10;
  if (buf[0] > 0x99) {
    buf[1]++;
    buf[0]-= 0xA0;
    if ((buf[1] & 0x0f) > 0x09) {
      buf[1] += 6;
      if (buf[1] > 0x59) {
	buf[2]++;
	buf[1]-= 0x60;
	if ((buf[2] & 0x0f) > 0x09) {
	  buf[2]+= 6;
	  if (buf[2] > 0x59) {
	    buf[3]++;
	    buf[2]-= 0x60;
	    if ((buf[3] & 0x0f) > 0x09) {
	      buf[3]+= 6;
	      if (buf[3]==0x24) {
		buf[3]=0;
	      }
	    }
	  }
	}
      }
    }
  }
}

uint8_t slaveHandler(uint8_t *data, uint8_t flags) {
  if (flags & MYI2C_SLAVE_ISTX) {
    switch (vectmode) {
      case 1: // fw version query, return 0x04/NAK
        Serial.println("FW query!");
        *data=4;
        vectmode=0;
        return 0;
      case 2: // some other thing... return 0x01/NAK
        Serial.println("? query!");
        *data=0x01;
        vectmode=0;
        return 0;
      case 3: // start databuf send
        {
          uint32_t now=millis();
          if ((now-lastgps)>100) {
            lastgps=now;
            databuf[7]++; // increase frame number
            stepTime(databuf + 29); // advance time too
            sendremain=37;
            Serial.println("send GPS");
          } else {
            sendremain=8;
            Serial.println("send MAG");
          }
          sendptr = &(databuf[0]);
          vectmode = 4;
        }
      case 4: // send databuf content
        *data = *(sendptr++);
        sendremain--;
        if (sendremain) {
          return 1;
        } else {
          vectmode=0;
          return 0;
        }
      default: // invalid
        *data=0;
        return 0;  
    }
  } else {
    if (flags & MYI2C_SLAVE_ISFIRST) {
      if (*data == 0x41) { 
        vectmode=0x41;
        return 1;
      } else if (*databuf == 0x5A) {
         vectmode=0x5a;
         return 1;
      } else if (*databuf == 0x07) {
        vectmode=3;
        return 1;
      } else {
         return 0;
      }
    } else {
      if ((vectmode == 0x41) && (*data == 0x02)) {
         vectmode =  1;
         return 1;
      } else if ((vectmode == 0x5A) && (*data == 0x02)) {
         vectmode = 2;
         return 1;
      } else {
        return 0;
      }
    }
  }
}



void setup() {
  myI2C_init(1);
  myI2C_slaveSetup(0x58,0,0,slaveHandler);
  Serial.begin(115200);
  Serial.println("Vector GPS emulator running");

}

void loop() {
}

