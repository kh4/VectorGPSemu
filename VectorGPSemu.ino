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

uint8_t to_nibble(uint8_t c) {
  uint8_t r = tolower(c);
  if (r>'9') {
    r -= ('a'-10);
  } else {
    r -= '0';
  }
  return r;
}

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
      case 'a': // altitude  "axxxxxxxx[-+]"
      case 's': // speed     "sxxxxxxxx"
      case 'c': // course    "cxxxxx"
      case 'n': // sats      "nxx"
      case '1': // flg0      "1xx"
      case '2': // flg1      "2xx"
      case '3': // flg2      "3xx"
      case '4': // flg3      "4xx"
      case 'x': // magx      "xxxxx"
      case 'y': // magy      "yxxxx"
      case 'z': // magz      "zxxxx"
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
        } else if (ch=='s' && serbuflen==8) {
          for (uint8_t i=0; i<4; i++) {
            databuf[20-i] = (0x10 * ((serbuf[i*2]-'0')&0x0f)) +
                            ((serbuf[i*2+1]-'0')&0x0f);
          }
        } else if ((ch=='a') && ((serbuflen==8) || (serbuflen==9 && serbuf[8]=='-'))) {
          for (uint8_t i=0; i<4; i++) {
            databuf[27-i] = (0x10 * ((serbuf[i*2]-'0')&0x0f)) +
                            ((serbuf[i*2+1]-'0')&0x0f);
          }
          if (serbuflen==9) {
            // negative
          } else {
            // positive
          }
        } else if (ch=='n' && serbuflen==2) {
          databuf[33]=(0x10 * ((serbuf[0]-'0')&0x0f)) +
                     ((serbuf[1]-'0')&0x0f);
        } else if (ch=='c' && serbuflen==5) {
          databuf[23] = ((serbuf[0]-'0')&0x0f);
          databuf[22] = (0x10 * ((serbuf[1]-'0')&0x0f)) +
                        ((serbuf[2]-'0')&0x0f);
          databuf[21] = (0x10 * ((serbuf[3]-'0')&0x0f)) +
                        ((serbuf[4]-'0')&0x0f);
        } else if (ch=='1' && serbuflen==2) {
          databuf[28]=(0x10 * (to_nibble(serbuf[0])&0x0f)) + 
                      (to_nibble(serbuf[1])&0x0f);
        } else if (ch=='2' && serbuflen==2) {
          databuf[34]=(0x10 * (to_nibble(serbuf[0])&0x0f)) + 
                      (to_nibble(serbuf[1])&0x0f);
        } else if (ch=='3' && serbuflen==2) {
          databuf[35]=(0x10 * (to_nibble(serbuf[0])&0x0f)) + 
                      (to_nibble(serbuf[1])&0x0f);
        } else if (ch=='4' && serbuflen==2) {
          databuf[36]=(0x10 * (to_nibble(serbuf[0])&0x0f)) + 
                      (to_nibble(serbuf[1])&0x0f);
        } else if (ch=='x' && serbuflen==4) {
          databuf[0]=(0x10 * (to_nibble(serbuf[0])&0x0f)) + 
                      (to_nibble(serbuf[1])&0x0f);
          databuf[1]=(0x10 * (to_nibble(serbuf[2])&0x0f)) + 
                      (to_nibble(serbuf[3])&0x0f);
        } else if (ch=='y' && serbuflen==4) {
          databuf[2]=(0x10 * (to_nibble(serbuf[0])&0x0f)) + 
                      (to_nibble(serbuf[1])&0x0f);
          databuf[3]=(0x10 * (to_nibble(serbuf[2])&0x0f)) + 
                      (to_nibble(serbuf[3])&0x0f);
        } else if (ch=='z' && serbuflen==4) {
          databuf[4]=(0x10 * (to_nibble(serbuf[0])&0x0f)) + 
                      (to_nibble(serbuf[1])&0x0f);
          databuf[5]=(0x10 * (to_nibble(serbuf[2])&0x0f)) + 
                      (to_nibble(serbuf[3])&0x0f);
        }
        getbuf=0;
      }
    }
  }
}

