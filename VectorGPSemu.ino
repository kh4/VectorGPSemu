#include "I2C.h"

uint8_t vectmode=0;
uint8_t vectdata;

uint8_t databuf[37] = { 0xFA,0x24,0x02,0x8A,0x13,0xCB,0x1B,0x00,
                        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
                        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
                        0x00,0x00,0x00,0x00,0x60,0x00,0x30,0x07,
                        0x18,0x00,0x14,0x00,0x80};
uint8_t *sendptr;
uint8_t sendremain=0;

uint32_t lastgps=0;
bool     newGPS=0;

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
          if (newGPS) {
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

void dumpbuf(uint8_t b[] ,uint8_t cnt)
{
  for (int i=0;i<cnt;i++) {
    Serial.print(b[i],16);
    if (i==(cnt-1))
      Serial.print('\n');
    else
      Serial.print(',');
  }
}

char serbuf[20];
uint8_t getbuf = 0;
uint8_t serbuflen = 0;

void loop() {
  uint32_t now=millis();
  if ((now-lastgps)>100) {
    lastgps=now;
    databuf[7]++; // increase frame number
    stepTime(databuf + 29); // advance time too
  }
  if (Serial.available()) {
    char ch = Serial.read();
    ch = tolower(ch);
    switch (ch) {
      case 'd':
        dumpbuf(databuf,37);
        break;
      case 'g': // GPS coord "g[nsew]ddmmmmmm"
      case 'h': // HDOP      "hxx"
      case 'a': // altitude  "a[-+]xxxxxxxx"
      case 's': // speed     "sxxxxxxxx"
      case 'c': // course    "cxxxxx"
      case 'n': // sats      "nxx"
        getbuf = 1;
        break;
    }
    if (getbuf) {
      uint32_t start   = millis();
      serbuflen = 0;
      while ((getbuf==1) && ((millis()-start)<500)) {
        if (Serial.available()) {
          char c = Serial.read();
          if ((c!='\r') && (c!='\n')) {
            serbuf[serbuflen++]=c;
            if (serbuflen == sizeof(serbuf)) {
              Serial.println("Overflow");
              getbuf=0; // OVERFLOW
            }
          } else {
              getbuf=2; //DONE
          }
        }
      }
      if (getbuf==2) {
        if (ch=='g' && serbuflen==9) {
          uint8_t coord[4],tgtoffset=8;
          for (uint8_t i=0; i<4; i++) {
            coord[3-i] = (0x10 * ((serbuf[i*2+1]-'0')&0x0f)) +
                         ((serbuf[i*2+2]-'0')&0x0f);
          }
          serbuf[0]=tolower(serbuf[0]);
          if (serbuf[0]=='n') {
          } else if (serbuf[0]=='s') {
          } else if (serbuf[0]=='e') {
            tgtoffset+=4;
          } else if (serbuf[0]=='w') {
            tgtoffset+=4;
          }
          for (uint8_t i=0; i<4; i++) {
            databuf[tgtoffset++]=coord[i];
          }
        } else if (ch=='h' && serbuflen==2) {
          databuf[16]=(0x10 * ((serbuf[0]-'0')&0x0f)) +
                     ((serbuf[1]-'0')&0x0f);
        }
        getbuf=0;
      }
    }
  }
}

