// библиотеки
#include "timer2Minim.h"
#include <GyverButton.h>
#include <Wire.h>
#include <RTClib.h>
#include <EEPROM.h>
#include <string.h>   // для memcmp() при сравнении отметки времени компиляции (см. 1_setup.ino)

// Явное объявление прототипа - обходной путь для известной проблемы
// автогенерации прототипов Arduino IDE: она не всегда правильно
// справляется с функциями, возвращающими тип из стороннего заголовка
// (здесь - DateTime из RTClib), и вставляет свой прототип туда, где
// этот тип ещё не виден. Если прототип уже объявлен явно (как здесь,
// сразу после #include <RTClib.h> выше), Arduino IDE не генерирует
// свой - и ошибка "DateTime does not name a type" не возникает.
DateTime rtcNowSafe();

#if RTC_MODULE == 1
RTC_DS1307 rtc;
// У RTC_DS1307 в этой библиотеке НЕТ метода lostPower() (только
// isrunning()) - оборачиваем в единую функцию, чтобы остальной код
// (1_setup.ino) не зависел от конкретного типа модуля.
inline bool rtcLostPower() { return !rtc.isrunning(); }
#elif RTC_MODULE == 2
RTC_PCF8563 rtc;
inline bool rtcLostPower() { return rtc.lostPower(); }
#elif RTC_MODULE == 3
RTC_PCF8523 rtc;
inline bool rtcLostPower() { return rtc.lostPower(); }
#else
RTC_DS3231 rtc;
inline bool rtcLostPower() { return rtc.lostPower(); }
#endif

// Единая защищённая точка чтения времени - используется ВЕЗДЕ в проекте
// вместо прямого rtc.now(). Если чтение по I2C сбоит (например, у
// некоторых экземпляров DS3231M это встречается), библиотека может
// вернуть физически невозможные значения (месяц 67 и т.п.) - именно
// так на индикаторах и появляется "ерунда". Здесь такие показания
// отбрасываются и подставляется последнее заведомо исправное значение,
// а не мусор.
DateTime lastGoodDT(2000, 1, 1, 0, 0, 0);
boolean rtcHasGoodTime = false;
boolean rtcReady = false;

DateTime rtcNowSafe() {
  // Не даём высокочастотному ISR мультиплексирования вмешиваться в I2C.
  byte oldTIMSK2 = TIMSK2;
  TIMSK2 &= ~(1 << OCIE2A);

  DateTime dt = rtc.now();
  TIMSK2 = oldTIMSK2;

  boolean ok = (dt.year() >= 2020 && dt.year() <= 2099 &&
                dt.month() >= 1 && dt.month() <= 12 &&
                dt.day() >= 1 && dt.day() <= 31 &&
                dt.hour() <= 23 && dt.minute() <= 59 && dt.second() <= 59);
  if (ok) {
    lastGoodDT = dt;
    rtcHasGoodTime = true;
    return dt;
  }
  // До первого успешного чтения не показываем заведомую заглушку 01.01.2000.
  return rtcHasGoodTime ? lastGoodDT : DateTime(2026, 1, 1, 0, 0, 0);
}

// таймеры
timerMinim dotTimer(500);                // полсекундный таймер для часов
timerMinim dotBrightTimer(DOT_TIMER);    // таймер шага яркости точки
timerMinim backlBrightTimer(30);         // таймер шага яркости подсветки
timerMinim almTimer((long)ALM_TIMEOUT * 1000);
timerMinim flipTimer(FLIP_SPEED[FLIP_EFFECT]);
timerMinim glitchTimer(1000);
timerMinim blinkTimer(500);
timerMinim modeInfoTimer(MODE_INFO_TIME);   // длительность неблокирующего показа номера режима (curMode == 4)
timerMinim brightTimeoutTimer(BRIGHT_TIMEOUT);      // бездействие в настройке яркости - автовыход без сохранения
timerMinim settingsTimeoutTimer(SETTINGS_TIMEOUT);  // бездействие в настройке времени/даты - автовыход без сохранения

// кнопки
GButton btnSet(BTN1, HIGH_PULL, NORM_OPEN);
GButton btnL(BTN2, HIGH_PULL, NORM_OPEN);
GButton btnR(BTN3, HIGH_PULL, NORM_OPEN);

// переменные
volatile int8_t indiDimm[4];      // величина диммирования (0-24)
volatile int8_t indiCounter[4];   // счётчик каждого индикатора (0-24)
volatile byte indiDigits[4];       // цифры, которые должны показать индикаторы (0-9)
volatile int8_t curIndi;          // текущий индикатор (0-3)
volatile byte backlDuty = 0;      // яркость подсветки 0-255 (программный ШИМ, см. isr.ino)
volatile uint16_t backlAccum = 0; // накопитель программного ШИМ подсветки (метод накопления ошибки, см. isr.ino)

boolean dotFlag;
int8_t hrs, mins, secs;
int8_t alm_hrs, alm_mins;
boolean changeFlag;
boolean blinkFlag;
byte indiMaxBright = INDI_BRIGHT, dotMaxBright = DOT_BRIGHT, backlMaxBright = BACKL_BRIGHT;
boolean alm_flag;
boolean dotBrightFlag, dotBrightDirection, backlBrightFlag, backlBrightDirection, indiBrightDirection;
int dotBrightCounter, backlBrightCounter, indiBrightCounter;
byte dotBrightStep;
boolean newTimeFlag;
boolean flipIndics[4];
byte newTime[4];
boolean flipInit;
byte startCathode[4], endCathode[4];
byte slotSteps[4];   // оставшееся число шагов до остановки лампы в эффекте "однорукий бандит" (FLIP_EFFECT==6)
byte glitchCounter, glitchMax, glitchIndic;
boolean glitchFlag, indiState;
byte curMode = 0;
byte minsCount = 0;
byte timeSetStage = 0;              // этап настройки в curMode==1: 0-часы, 1-минуты, 2-число, 3-месяц, 4-год
int8_t changeHrs, changeMins;
int8_t changeDay = 1, changeMonth = 1;   // редактируемые число/месяц в настройке даты
int16_t changeYear = SYNC_YEAR;          // редактируемый год в настройке даты (2000-2099); переопределяется реальным значением из RTC при входе в настройку

// ------- периодический показ даты (curMode == 3) -------
timerMinim dateShowTimer((uint32_t)DATE_SHOW_INTERVAL * 1000UL);          // интервал между показами даты, мс
timerMinim dateShowDurationTimer((uint32_t)DATE_SHOW_DURATION * 1000UL);  // длительность показа даты, мс
boolean dateShowing = false;        // true, пока на индикаторах показана дата
boolean dateShowAllowed = true;     // разрешение показа даты (выключается установкой месяца 00), хранится в EEPROM
boolean modeInfoShowing = false;    // true, пока на индикаторах показан номер режима (curMode == 4)
boolean simplifiedMode = true;      // упрощённый режим управления (по умолчанию включён), хранится в EEPROM
boolean lampState = false;
volatile boolean anodeStates[] = {1, 1, 1, 1};   // читается в ISR - должен быть volatile (см. ревью прошивки)
byte currentLamp, flipEffectStages;
bool trainLeaving;

const uint8_t CRTgamma[256] PROGMEM = {
  0,    0,    1,    1,    1,    1,    1,    1,
  1,    1,    1,    1,    1,    1,    1,    1,
  2,    2,    2,    2,    2,    2,    2,    2,
  3,    3,    3,    3,    3,    3,    4,    4,
  4,    4,    4,    5,    5,    5,    5,    6,
  6,    6,    7,    7,    7,    8,    8,    8,
  9,    9,    9,    10,   10,   10,   11,   11,
  12,   12,   12,   13,   13,   14,   14,   15,
  15,   16,   16,   17,   17,   18,   18,   19,
  19,   20,   20,   21,   22,   22,   23,   23,
  24,   25,   25,   26,   26,   27,   28,   28,
  29,   30,   30,   31,   32,   33,   33,   34,
  35,   35,   36,   37,   38,   39,   39,   40,
  41,   42,   43,   43,   44,   45,   46,   47,
  48,   49,   49,   50,   51,   52,   53,   54,
  55,   56,   57,   58,   59,   60,   61,   62,
  63,   64,   65,   66,   67,   68,   69,   70,
  71,   72,   73,   74,   75,   76,   77,   79,
  80,   81,   82,   83,   84,   85,   87,   88,
  89,   90,   91,   93,   94,   95,   96,   98,
  99,   100,  101,  103,  104,  105,  107,  108,
  109,  110,  112,  113,  115,  116,  117,  119,
  120,  121,  123,  124,  126,  127,  129,  130,
  131,  133,  134,  136,  137,  139,  140,  142,
  143,  145,  146,  148,  149,  151,  153,  154,
  156,  157,  159,  161,  162,  164,  165,  167,
  169,  170,  172,  174,  175,  177,  179,  180,
  182,  184,  186,  187,  189,  191,  193,  194,
  196,  198,  200,  202,  203,  205,  207,  209,
  211,  213,  214,  216,  218,  220,  222,  224,
  226,  228,  230,  232,  233,  235,  237,  239,
  241,  243,  245,  247,  249,  251,  253,  255,
};

byte getPWM_CRT(byte val) {
  return pgm_read_byte(&(CRTgamma[val]));
}

// быстрый digitalWrite
void setPin(uint8_t pin, uint8_t x) {
  switch (pin) { // откл pwm
    case 3:  // 2B
      bitClear(TCCR2A, COM2B1);
      break;
    case 5: // 0B
      bitClear(TCCR0A, COM0B1);
      break;
    case 6: // 0A
      bitClear(TCCR0A, COM0A1);
      break;
    case 9: // 1A
      bitClear(TCCR1A, COM1A1);
      break;
    case 10: // 1B
      bitClear(TCCR1A, COM1B1);
      break;
    case 11: // 2A
      bitClear(TCCR2A, COM2A1);
      break;
  }

  if (pin < 8) bitWrite(PORTD, pin, x);
  else if (pin < 14) bitWrite(PORTB, (pin - 8), x);
  else if (pin < 20) bitWrite(PORTC, (pin - 14), x);
  else return;
}

// быстрый analogWrite
void setPWM(uint8_t pin, uint16_t duty) {
  if (duty == 0) setPin(pin, LOW);
  else {
    switch (pin) {
      case 5:
        bitSet(TCCR0A, COM0B1);
        OCR0B = duty;
        break;
      case 6:
        bitSet(TCCR0A, COM0A1);
        OCR0A = duty;
        break;
      case 10:
        bitSet(TCCR1A, COM1B1);
        OCR1B = duty;
        break;
      case 9:
        bitSet(TCCR1A, COM1A1);
        OCR1A = duty;
        break;
      case 3:
        bitSet(TCCR2A, COM2B1);
        OCR2B = duty;
        break;
      // BACKL (11) сюда специально не включён: этот пин физически = OC2A
      // таймера 2, который занят под период ISR мультиплексирования
      // (см. isr.ino/1_setup.ino) - для BACKL используется setBackl()
      default:
        break;
    }
  }
}

// установить яркость подсветки (BACKL, пин 11) - программный ШИМ, см. ISR в isr.ino.
// Обычный setPWM()/setPin() для этого пина больше не используется (см. пояснение выше).
void setBackl(byte duty) {
  backlDuty = duty;
}
