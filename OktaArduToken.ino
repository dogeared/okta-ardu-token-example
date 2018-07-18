#include <Arduboy2.h>
#include <swRTC.h>
#include <sha1.h>
#include <TOTP.h>
#include <Base32.h>

Arduboy2 arduboy;
Base32 base32;
swRTC rtc;

String totpCode;
bool secretSet;
int secretPosition;
String secret;
String date;
bool dateSet;
int datePosition;

byte* hmacKey = NULL;
byte secretBuf[16];

void setup() {

  secretSet = false;
  secretPosition = 0;
  //secret = "MMMMMMMMMMMMMMMM";
  secret = "C46MPM6ZTP3X6HQB";
  dateSet = false;
  datePosition = 0;
  
  rtc.stopRTC();
  rtc.setDate(17, 7, 2018);
  rtc.setTime(9, 00, 20);
  rtc.startRTC();

  int sec = rtc.getSeconds();
  int minu = rtc.getMinutes();
  int hours = rtc.getHours();
  int day = rtc.getDay();
  int mon = rtc.getMonth();
  int year = rtc.getYear();
  date = padNum(mon) + "/" + padNum(day) + "/" + padNum(year) + " " + padNum(hours) + ":" + padNum(minu) + ":" + padNum(sec);
  
  arduboy.begin();
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

void setSecret() {
  // Okta like: C46MPM6ZTP3X6HQB
  arduboy.setCursor(0, 0);
  arduboy.print("Set OKTA Secret:");
  arduboy.setCursor(0, 30);
  arduboy.println("u/d: cycle letters/#s");
  arduboy.println("l/r: set position");
  arduboy.println("A: save secret");

  if (arduboy.justPressed(RIGHT_BUTTON)) {
    secretPosition++;
    if (secretPosition >= secret.length()) {
      secretPosition = 0;
    }
  }

  if (arduboy.justPressed(LEFT_BUTTON)) {
    secretPosition--;
    if (secretPosition < 0) {
      secretPosition = secret.length() - 1;
    }
  }

  if (arduboy.justPressed(DOWN_BUTTON)) {
    secret = updateSecret(secret, secretPosition, -1);  
  }

  if (arduboy.justPressed(UP_BUTTON)) {
    secret = updateSecret(secret, secretPosition, 1);
  }

  if (arduboy.justPressed(A_BUTTON)) {
    secret.getBytes(secretBuf, secret.length() + 1);
    base32.fromBase32(secretBuf, sizeof(secretBuf), (byte*&)hmacKey);

    secretSet = true;
  }

  printWithInvertChar(secret, 0, 15, secretPosition, 1);
  
}

void setDate() {
  arduboy.setCursor(0, 0);
  arduboy.print("Set Date (GMT):");
  arduboy.setCursor(0, 30);
  arduboy.println("u/d: cycle #s");
  arduboy.println("l/r: set position");
  arduboy.println("A: save date");
  arduboy.println("B: set secret");

  if (arduboy.justPressed(RIGHT_BUTTON)) {
    datePosition++;
    if (datePosition == 2 or datePosition == 5 or datePosition == 10 or datePosition == 13 or datePosition == 16) {
      datePosition++;
    }
    if (datePosition >= date.length()) {
      datePosition = 0;
    }
  }

  if (arduboy.justPressed(LEFT_BUTTON)) {
    datePosition--;
    if (datePosition == 2 or datePosition == 5 or datePosition == 10 or datePosition == 13 or datePosition == 16) {
      datePosition--;
    }

    if (datePosition < 0) {
      datePosition = date.length() - 1;
    }
  }

  if (arduboy.justPressed(DOWN_BUTTON)) {
    date = updateDate(date, datePosition, -1);  
  }

  if (arduboy.justPressed(UP_BUTTON)) {
    date = updateDate(date, datePosition, 1);
  }

  if (arduboy.justPressed(B_BUTTON)) {
    secretSet = false;
  }

  if (arduboy.justPressed(A_BUTTON)) {
    int mm = date.substring(0,2).toInt();
    int dd = date.substring(3,5).toInt();
    int yyyy = date.substring(6,10).toInt();
    int HH = date.substring(11,13).toInt();
    int MM = date.substring(14,16).toInt();
    int SS = date.substring(17).toInt();

    rtc.stopRTC();
    rtc.setDate(dd, mm, yyyy);
    rtc.setTime(HH, MM, SS);
    rtc.startRTC();
    
    dateSet = true;
  }

  printWithInvertChar(date, 0, 15, datePosition, 1);
}

void showTotpCode() {

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

  int sec = rtc.getSeconds();
  int minu = rtc.getMinutes();
  int hours = rtc.getHours();
  int day = rtc.getDay();
  int mon = rtc.getMonth();
  int year = rtc.getYear();
  String dateStr = padNum(mon) + "/" + padNum(day) + "/" + padNum(year) + " " + padNum(hours) + ":" + padNum(minu) + ":" + padNum(sec);
  printWithInvertChar(dateStr, 0, 30, -1, 1);
}

