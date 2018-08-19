#include <Arduboy2.h>
#include <swRTC.h>
#include <TOTP.h>
#include <Base32.h>
#include <EEPROM.h>

//#define DEBUG 1

const long LOOP_INTERVAL = 10000; // 10 seconds
unsigned long previousMillis = 0;

#ifdef DEBUG
  #define DEBUG_PRINT(x) Serial.print (x)
  #define DEBUG_PRINTDEC(x) Serial.print (x, DEC)
  #define DEBUG_PRINTLN(x) Serial.println (x)
  #define DEBUG_PRINTLN_INTERVAL(x) unsigned long currentMillis = millis(); if (currentMillis - previousMillis >= LOOP_INTERVAL) { previousMillis = currentMillis; Serial.println(x); }
#else
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINTDEC(x)
  #define DEBUG_PRINTLN(x)
  #define DEBUG_PRINTLN_INTERVAL(x)
#endif

const byte APP_ID = 0x2A;
const int APP_ID_SAVE_ADDRESS = EEPROM_STORAGE_SPACE_START + 51;
const int TOTP_SECRET_SAVE_ADDRESS = APP_ID_SAVE_ADDRESS + sizeof(int);
const int TOTP_SEC_SAVE_ADDRESS = TOTP_SECRET_SAVE_ADDRESS + (sizeof(byte) * 16);
const int TOTP_MIN_SAVE_ADDRESS = TOTP_SEC_SAVE_ADDRESS + sizeof(int);
const int TOTP_HOUR_SAVE_ADDRESS = TOTP_MIN_SAVE_ADDRESS + sizeof(int);
const int TOTP_DAY_SAVE_ADDRESS = TOTP_HOUR_SAVE_ADDRESS + sizeof(int);
const int TOTP_MON_SAVE_ADDRESS = TOTP_DAY_SAVE_ADDRESS + sizeof(int);
const int TOTP_YEAR_SAVE_ADDRESS = TOTP_MON_SAVE_ADDRESS + sizeof(int);

Arduboy2 arduboy;
Base32 base32;
swRTC rtc;

struct TotpInfo {
  String secret;
  int mon;
  int day;
  int year;
  int hour;
  int minu;
  int sec;
};

String totpCode;
bool secretSet;
int secretPosition;
String dateStr;
bool dateSet;
int datePosition;
TotpInfo totpInfo = {
  "MMMMMMMMMMMMMMMM",
  07, 23, 2018,
  12, 0, 0
};

byte* hmacKey = NULL;
byte secretBuf[16];

const unsigned char PROGMEM oktadevlogo[] =
{
0x00, 0x00, 0xb8, 0x04, 0xfb, 0xfd, 0xfe, 0xff, 0x1f, 0xef, 0x2f, 0x2f, 0x2f, 0x2f, 0x00, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x80, 0x80, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0x08, 0xf4, 0xfa, 0xfa, 0xfa, 0xfa, 0xf4, 0x08, 0xf0, 0x00, 0x00, 0x00, 0x80, 0x80, 0x80, 0x80, 0x80, 0xf0, 0x00, 0xf2, 0xfa, 0xfa, 0xfa, 0xf2, 0xe4, 0x18, 0xe0, 0x80, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x80, 0x80, 0x80, 0x00, 0x00, 0x00, 0x80, 0x80, 0x80, 0x80, 0x00, 0x00, 0x0e, 0x00, 0x0e, 0x2f, 0x2f, 0x0f, 0xef, 0x4f, 0x3f, 0xff, 0xfe, 0xfd, 0xf2, 0x0c, 0xf8, 0x00, 0x00, 
0xc0, 0x38, 0x82, 0xf8, 0xff, 0xff, 0xff, 0x7f, 0x80, 0x7f, 0x00, 0x00, 0x00, 0xc0, 0x20, 0x90, 0xe8, 0xf4, 0xfa, 0xf9, 0xfd, 0xfd, 0x7e, 0x7e, 0x7e, 0xfc, 0xfd, 0xfd, 0xfb, 0xfa, 0xf4, 0xe8, 0xbf, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x80, 0xef, 0xf4, 0xfa, 0xfd, 0xfe, 0xfe, 0x7e, 0xbe, 0x5e, 0xed, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7d, 0x82, 0xcc, 0xf4, 0xfa, 0xfa, 0xfd, 0xfd, 0xfc, 0x7e, 0x7e, 0x7e, 0xfd, 0xfd, 0xfd, 0xfe, 0xfe, 0xfe, 0xfe, 0xfd, 0x02, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1f, 0xe0, 0x00, 0xff, 0xff, 0xff, 0xff, 0xe0, 0x9f, 0x60, 0x80, 
0x36, 0xcf, 0x1f, 0xff, 0xff, 0xff, 0xf9, 0xe6, 0x19, 0xe0, 0x00, 0x00, 0x1e, 0x61, 0x8e, 0x7f, 0xff, 0xff, 0xff, 0xff, 0xf1, 0xce, 0xd3, 0x81, 0xd1, 0xde, 0xc0, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x3f, 0x7f, 0xff, 0xff, 0xff, 0xf9, 0xf6, 0xeb, 0xd1, 0x20, 0x7f, 0x80, 0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xc1, 0xdf, 0xa1, 0xbf, 0x80, 0x3f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xe0, 0xde, 0xd1, 0x81, 0xd3, 0xce, 0xe1, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xc0, 0xdf, 0x80, 0x00, 0x80, 0x00, 0x00, 0x80, 0x70, 0x09, 0xf6, 0xf9, 0xff, 0xff, 0x7f, 0x9f, 0x6f, 0x16, 
0x00, 0x01, 0xd4, 0x01, 0xff, 0xff, 0xff, 0xff, 0x80, 0x3f, 0x60, 0x20, 0x20, 0x00, 0x01, 0x02, 0x05, 0x0b, 0x17, 0x17, 0x2f, 0x2f, 0x2f, 0x2f, 0x2f, 0x2f, 0x2f, 0x2f, 0x17, 0x1b, 0x09, 0x04, 0x0f, 0x10, 0x2f, 0x2f, 0x2f, 0x2f, 0x2f, 0x2f, 0x10, 0x0f, 0x04, 0x09, 0x13, 0x2f, 0x2f, 0x2f, 0x2f, 0x2f, 0x2f, 0x2e, 0x13, 0x0c, 0x0b, 0x13, 0x17, 0x2f, 0x2f, 0x2f, 0x4f, 0x4f, 0x2f, 0x04, 0x10, 0x09, 0x0b, 0x17, 0x2f, 0x2f, 0x2f, 0x2f, 0x2f, 0x2f, 0x2f, 0x2f, 0x17, 0x17, 0x17, 0x2f, 0x2f, 0x2f, 0x4f, 0x2f, 0x2f, 0x10, 0x2f, 0x20, 0x20, 0x7f, 0x20, 0xc0, 0xff, 0xff, 0xff, 0xff, 0x00, 0xff, 0x00, 0x00, 
0x00, 0x00, 0x01, 0x02, 0x0d, 0x0b, 0x07, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x00, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0x00, 0x07, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x07, 0x0b, 0x04, 0x03, 0x01, 0x00, 0x00, 
};

void drawLogoBitmap(int16_t y)
{
  arduboy.drawBitmap(14, y, oktadevlogo, 100, 36);
}

void begin() {
  arduboy.boot();
  arduboy.display();
  arduboy.flashlight();
  arduboy.systemButtons();
  arduboy.audio.begin();
  arduboy.bootLogoShell(drawLogoBitmap);
  arduboy.waitNoButtons();
}

void setup() {
  #ifdef DEBUG
    Serial.begin(9600);
    while (!Serial) {
      ;
    }
  #endif
  DEBUG_PRINTLN("setup()");

  secretSet = false;
  secretPosition = 0;
  dateSet = false;
  datePosition = 0;
  
  byte appId = EEPROM.read(APP_ID_SAVE_ADDRESS);
  if (appId != APP_ID) {
    DEBUG_PRINTLN("First time in. Writing APP_ID and settings...");
    EEPROM.write(APP_ID_SAVE_ADDRESS, APP_ID);
    writeTotpInfo(totpInfo);
  } else {
    DEBUG_PRINTLN("Found APP_ID. Loading in settings...");
    totpInfo = readTotpInfo();
    secretSet = true;
    updateHmacKey();
  }
  updateRtc();
  dateStr = getDateString(totpInfo);
  begin();
}

void loop() {
  if (!(arduboy.nextFrame())) return;

  arduboy.pollButtons();
  arduboy.clear();

  if (!secretSet) {
    setSecret();
  } else if (!dateSet) {
    setDate();
  } else if (secretSet && dateSet) {
    showTotpCode();
  }

  arduboy.display();
}

void setSecret() {
  DEBUG_PRINTLN_INTERVAL("setSecret()");
  // Okta secret like: C46MPM6ZTP3X6HQB
  arduboy.setCursor(0, 0);
  arduboy.print("Set OKTA Secret:");
  arduboy.setCursor(0, 30);
  arduboy.println("u/d: cycle letters/#s");
  arduboy.println("l/r: set position");
  arduboy.println("A: save secret");

  if (arduboy.everyXFrames(6)) {
    if (arduboy.pressed(RIGHT_BUTTON)) {
      secretPosition++;
      if (secretPosition >= totpInfo.secret.length()) {
        secretPosition = 0;
      }
    }
  
    if (arduboy.pressed(LEFT_BUTTON)) {
      secretPosition--;
      if (secretPosition < 0) {
        secretPosition = totpInfo.secret.length() - 1;
      }
    }
  
    if (arduboy.pressed(DOWN_BUTTON)) {
      totpInfo.secret = updateSecret(totpInfo.secret, secretPosition, -1);  
    }
  
    if (arduboy.pressed(UP_BUTTON)) {
      totpInfo.secret = updateSecret(totpInfo.secret, secretPosition, 1);
    }  
  }

  if (arduboy.justPressed(A_BUTTON)) {
    writeString(TOTP_SECRET_SAVE_ADDRESS, totpInfo.secret);
    updateHmacKey();
    secretSet = true;
  }    

  printWithInvertChar(totpInfo.secret, 0, 15, secretPosition, 1);
}

void setDate() {
  DEBUG_PRINTLN_INTERVAL("setDate()");
  
  arduboy.setCursor(0, 0);
  arduboy.print("Set Date (GMT):");
  arduboy.setCursor(0, 30);
  arduboy.println("u/d: cycle #s");
  arduboy.println("l/r: set position");
  arduboy.println("A: save date");
  arduboy.println("B: set secret");

  if (arduboy.everyXFrames(6)) {
    if (arduboy.pressed(RIGHT_BUTTON)) {
      datePosition++;
      if (datePosition == 2 or datePosition == 5 or datePosition == 10 or datePosition == 13 or datePosition == 16) {
        datePosition++;
      }
      if (datePosition >= dateStr.length()) {
        datePosition = 0;
      }
    }
  
    if (arduboy.pressed(LEFT_BUTTON)) {
      datePosition--;
      if (datePosition == 2 or datePosition == 5 or datePosition == 10 or datePosition == 13 or datePosition == 16) {
        datePosition--;
      }
  
      if (datePosition < 0) {
        datePosition = dateStr.length() - 1;
      }
    }
  
    if (arduboy.pressed(DOWN_BUTTON)) {
      dateStr = updateDate(dateStr, datePosition, -1);  
    }
  
    if (arduboy.pressed(UP_BUTTON)) {
      dateStr = updateDate(dateStr, datePosition, 1);
    }    
  }

  if (arduboy.justPressed(B_BUTTON)) {
    secretSet = false;
  }

  if (arduboy.justPressed(A_BUTTON)) {
    totpInfo.mon = dateStr.substring(0,2).toInt();
    totpInfo.day = dateStr.substring(3,5).toInt();
    totpInfo.year = dateStr.substring(6,10).toInt();
    totpInfo.hour = dateStr.substring(11,13).toInt();
    totpInfo.minu = dateStr.substring(14,16).toInt();
    totpInfo.sec = dateStr.substring(17).toInt();

    updateRtc();
    
    writeInt(TOTP_SEC_SAVE_ADDRESS, totpInfo.sec);
    writeInt(TOTP_MIN_SAVE_ADDRESS, totpInfo.minu);
    writeInt(TOTP_HOUR_SAVE_ADDRESS, totpInfo.hour);
    writeInt(TOTP_DAY_SAVE_ADDRESS, totpInfo.day);
    writeInt(TOTP_MON_SAVE_ADDRESS, totpInfo.mon);
    writeInt(TOTP_YEAR_SAVE_ADDRESS, totpInfo.year);
    
    dateSet = true;
  }

  printWithInvertChar(dateStr, 0, 15, datePosition, 1);
}

void showTotpCode() {
  DEBUG_PRINTLN_INTERVAL("showTotpCode()");

  totpInfo.mon = rtc.getMonth();
  totpInfo.day = rtc.getDay();
  totpInfo.year = rtc.getYear();
  totpInfo.hour = rtc.getHours();
  totpInfo.minu = rtc.getMinutes();
  totpInfo.sec = rtc.getSeconds();

  arduboy.setCursor(0, 45);
  arduboy.println("A: set date");
  arduboy.println("B: set secret");

  if (arduboy.justPressed(B_BUTTON)) {
    secretSet = false;
  }

  if (arduboy.justPressed(A_BUTTON)) {
    dateSet = false;
  }

  TOTP totp = TOTP(hmacKey, 10);
  
  long GMT = rtc.getTimestamp();
  totpCode = totp.getCode(GMT);
  printWithInvertChar(totpCode, 0, 10, -1, 2);

  dateStr = getDateString(totpInfo);
  
  printWithInvertChar(dateStr, 0, 30, -1, 1);
}

void printWithInvertChar(String str, int16_t x, int16_t y, int16_t charToInvert, int16_t size) {
  int color = 1;
  int bg = 0;
  for(int i = 0; str[i] != '\0'; i++) {
    if (charToInvert == i) {
      color = 0;
      bg = 1;
    } else {
      color = 1;
      bg = 0;
    }
    arduboy.drawChar(x + (6*i*size), y, str[i], color, bg, size);
  }
}

String padNum(int num) {
  if (num < 10) {
    return "0" + String(num);
  } else {
    return String(num);
  }
}

String updateSecret(String secret, int secretPosition, int addOrSub) {
  String ret = secret.substring(0, secretPosition);
  char upd = secret.charAt(secretPosition) + addOrSub;
  if (upd < '0') {
    upd = 'Z';
  } else if (upd > 'Z') {
    upd = '0';
  } else if (upd < 'A' and upd > '9' and addOrSub < 0) {
    upd = '9';
  } else if (upd > '9' and upd < 'A' and addOrSub > 0) {
    upd = 'A';
  }
  ret += upd + secret.substring(secretPosition + 1);
  return ret;
}

String updateDate(String date, int datePosition, int addOrSub) {
  String ret = date.substring(0, datePosition);
  char upd = date.charAt(datePosition) + addOrSub;
  if (upd < '0') {
    upd = '9';
  } else if (upd > '9') {
    upd = '0';
  }
  ret += upd + date.substring(datePosition + 1);
  return ret;
}

void updateHmacKey() {
    totpInfo.secret.getBytes(secretBuf, totpInfo.secret.length() + 1);
    base32.fromBase32(secretBuf, sizeof(secretBuf), (byte*&)hmacKey);
}

void updateRtc() {
    rtc.stopRTC();
    rtc.setDate(totpInfo.day, totpInfo.mon, totpInfo.year);
    rtc.setTime(totpInfo.hour, totpInfo.minu, totpInfo.sec);
    rtc.startRTC();
}

String getDateString(TotpInfo totpInfo) {
  return padNum(totpInfo.mon) + "/" + padNum(totpInfo.day) + "/" + padNum(totpInfo.year) + " " + padNum(totpInfo.hour) + ":" + padNum(totpInfo.minu) + ":" + padNum(totpInfo.sec);
}

void writeString(int address, String str) {
  DEBUG_PRINT("Writing String: ");
  for(int i = 0; str[i] != '\0'; i++) {
    EEPROM.write(address + i, str[i]);
    DEBUG_PRINT(str[i]);
  }
  DEBUG_PRINTLN();
}

String readString(int address, int size) {
  char buf[size+1];
  DEBUG_PRINT("Reading String: ");
  for (int i = 0; i < size; i++) {
    buf[i] = EEPROM.read(address + i);
    DEBUG_PRINT(buf[i]);
  }
  DEBUG_PRINTLN();
  buf[size] = 0;
  return String(buf);
}

void writeInt(int address, int val) {
  EEPROM.put(address, val);
}

int readInt(int address) {
  int ret;
  EEPROM.get(address, ret);
  return ret;
}

void writeTotpInfo(TotpInfo totpInfo) {
  writeString(TOTP_SECRET_SAVE_ADDRESS, totpInfo.secret);
  writeInt(TOTP_SEC_SAVE_ADDRESS, totpInfo.sec);
  writeInt(TOTP_MIN_SAVE_ADDRESS, totpInfo.minu);
  writeInt(TOTP_HOUR_SAVE_ADDRESS, totpInfo.hour);
  writeInt(TOTP_DAY_SAVE_ADDRESS, totpInfo.day);
  writeInt(TOTP_MON_SAVE_ADDRESS, totpInfo.mon);
  writeInt(TOTP_YEAR_SAVE_ADDRESS, totpInfo.year);
  DEBUG_PRINTLN("Wrote totpInfo to eeprom.");
}

TotpInfo readTotpInfo() {
  TotpInfo ret = {};
  
  ret.secret = readString(TOTP_SECRET_SAVE_ADDRESS, 16);
  ret.sec = readInt(TOTP_SEC_SAVE_ADDRESS);
  ret.minu = readInt(TOTP_MIN_SAVE_ADDRESS);
  ret.hour = readInt(TOTP_HOUR_SAVE_ADDRESS);
  ret.day = readInt(TOTP_DAY_SAVE_ADDRESS);
  ret.mon = readInt(TOTP_MON_SAVE_ADDRESS);
  ret.year = readInt(TOTP_YEAR_SAVE_ADDRESS);
  
  return ret;
}
