void setup() {
  //Serial.begin(9600);
  // случайное зерно для генератора случайных чисел
  randomSeed(analogRead(6) + analogRead(7));

  // настройка пинов на выход
  pinMode(DECODER0, OUTPUT);
  pinMode(DECODER1, OUTPUT);
  pinMode(DECODER2, OUTPUT);
  pinMode(DECODER3, OUTPUT);
  pinMode(KEY0, OUTPUT);
  pinMode(KEY1, OUTPUT);
  pinMode(KEY2, OUTPUT);
  pinMode(KEY3, OUTPUT);
  pinMode(PIEZO, OUTPUT);
  pinMode(GEN, OUTPUT);
  pinMode(DOT, OUTPUT);
  pinMode(BACKL, OUTPUT);

  // задаем частоту ШИМ на 9 и 10 выводах 31 кГц
  TCCR1B = TCCR1B & 0b11111000 | 1;    // ставим делитель 1

  // включаем ШИМ
  setPWM(9, DUTY);

  // Timer2 используется ИСКЛЮЧИТЕЛЬНО для прерывания мультиплексирования
  // индикаторов (ISR(TIMER2_COMPA_vect) в isr.ino) - OCR2A задаёт МОМЕНТ
  // СРАБАТЫВАНИЯ этого прерывания и здесь же жёстко зафиксирован.
  // ВАЖНО: пин BACKL (11) физически совпадает с OC2A этого же таймера,
  // поэтому яркость подсветки НЕЛЬЗЯ регулировать аппаратным ШИМ через
  // OCR2A (это исказило бы период самого прерывания) - вместо этого
  // подсветка регулируется программным ШИМ прямо внутри ISR (см.
  // setBackl() и backlDuty/backlAccum в isr.ino). setPWM()/setPin() для
  // пина BACKL больше не используются.
  TCCR2B = (TCCR2B & B11111000) | 2;    // делитель 8
  TCCR2A |= (1 << WGM21);   // включить CTC режим для COMPA
  OCR2A = 255;              // период прерывания: 16МГц / (8 * 256) = 7.8 кГц
  TIMSK2 |= (1 << OCIE2A);  // включить прерывания по совпадению COMPA

  // ---------- ТЕСТ ЛАМП И СБРОС НАСТРОЕК ПРИ ВКЛЮЧЕНИИ ----------
  // если при подаче питания зажата кнопка "-" (BTN2) - запускаем тест индикаторов
  // если зажата кнопка "+" (BTN3) - показываем обратный отсчёт 3-2-1-0 и
  // сбрасываем все настройки в EEPROM к значениям прошивки (кнопки можно
  // держать одновременно - сработает и то, и другое)
  // если зажата кнопка "выбор" (BTN1) - переключаем полный/упрощённый режим
  // управления (см. SIMPLIFIED_MODE_ADDR); никак не связано с двумя другими
  // кнопками, можно держать в любых сочетаниях
  pinMode(BTN1, INPUT_PULLUP);
  pinMode(BTN2, INPUT_PULLUP);
  pinMode(BTN3, INPUT_PULLUP);
  boolean toggleControlMode = (digitalRead(BTN1) == LOW);
  boolean runLampTest = (digitalRead(BTN2) == LOW);
  boolean resetSettings = (digitalRead(BTN3) == LOW);
  if (resetSettings) {
    showResetCountdown();
  }
  if (runLampTest) {
    showFirmwareVersion();   // сначала на 1 секунду - номер прошивки
    lampTest();              // затем сам тест ламп
  }

  // ---------- RTC -----------
  // Ждём, пока модуль реально откликнется по I2C - begin() у RTClib
  // возвращает false, если устройство не отвечает на своей шине/адресе.
  // Без этой проверки код продолжал бы работу "вслепую": rtc.now() в
  // такой ситуации читает мусор из несуществующего/непрочитанного
  // устройства (отсюда бессмысленные значения времени/даты на индикаторах).
  byte rtcRetries = 0;
  rtcReady = false;
  while (!rtcReady && rtcRetries < 30) {
    byte oldTIMSK2 = TIMSK2;
    TIMSK2 &= ~(1 << OCIE2A);
    rtcReady = rtc.begin();
    TIMSK2 = oldTIMSK2;
    if (!rtcReady) {
      delay(150);
      rtcRetries++;
    }
  }
  if (!rtcReady) {
    // RTC не отвечает: не вызываем lostPower/adjust и не читаем мусор.
    rtcHasGoodTime = false;
  }

  // Определяем, залита ли только что НОВАЯ прошивка (в т.ч. просто
  // заново скомпилированная): сравниваем отметку времени компиляции
  // текущего скетча с той, что сохранена в EEPROM с прошлого раза.
  const char compileStamp[] = __DATE__ " " __TIME__;
  char storedStamp[sizeof(compileStamp)];
  EEPROM.get(COMPILE_STAMP_ADDR, storedStamp);
  boolean freshFlash = (memcmp(storedStamp, compileStamp, sizeof(compileStamp)) != 0);

  if (rtcReady && (rtcLostPower() || freshFlash)) {
    // синхронизация с компьютером по времени компиляции скетча + компенсация
    // задержки между компиляцией и реальным стартом на плате (см. TIME_SYNC_OFFSET).
    // Год берём НЕ из разбора __DATE__ (парсинг года через
    // DateTime(F(__DATE__), F(__TIME__)) в некоторых версиях RTClib работает
    // ненадёжно), а из отдельной константы SYNC_YEAR - месяц, число, часы,
    // минуты и секунды по-прежнему берутся из времени компиляции.
    DateTime compileDT = DateTime(F(__DATE__), F(__TIME__));
    DateTime syncDT(SYNC_YEAR, compileDT.month(), compileDT.day(),
                    compileDT.hour(), compileDT.minute(), compileDT.second());
    rtc.adjust(syncDT + TimeSpan(TIME_SYNC_OFFSET));
  }
  if (freshFlash) {
    EEPROM.put(COMPILE_STAMP_ADDR, compileStamp);   // запоминаем новую отметку компиляции
  }

  DateTime now = rtcNowSafe();
  // Дополнительная защита от старого/неверного года в RTC (например,
  // если чип по какой-то причине не подхватил синхронизацию выше и
  // остался на старом годе вроде 2024): принудительно поднимаем год до
  // SYNC_YEAR при КАЖДОМ включении, если текущий год в RTC меньше него.
  // Месяц, число и время при этом не трогаем.
  if (rtcReady && now.year() < SYNC_YEAR) {
    rtc.adjust(DateTime(SYNC_YEAR, now.month(), now.day(), now.hour(), now.minute(), now.second()));
    now = rtcNowSafe();   // перечитать после коррекции
  }
  secs = now.second();
  mins = now.minute();
  hrs = now.hour();

  // EEPROM
  // маркер 103 (было 102 до v4.11) - когда в v4.11 добавился адрес 11
  // (режим управления - полный/упрощённый), платы с уже прошитой памятью
  // не проходили бы через блок первого запуска, и этот байт читался бы как
  // "мусор" из старого содержимого EEPROM. Повышение маркера форсирует
  // полную переинициализацию всех настроек один раз при обновлении прошивки.
  if (EEPROM.read(1023) != 103 || resetSettings) {   // первый запуск / обновление прошивки / сброс кнопкой "+"
    EEPROM.put(1023, 103);
    EEPROM.put(0, FLIP_EFFECT);
    EEPROM.put(1, BACKL_MODE);
    EEPROM.put(2, GLITCH_ALLOWED);
    EEPROM.put(3, DOT_MODE);
    EEPROM.put(4, indiPresetIndex);
    EEPROM.put(5, backlPresetIndex);
    EEPROM.put(6, indiPresetIndexN);
    EEPROM.put(7, backlPresetIndexN);
    EEPROM.put(8, dotPresetIndex);
    EEPROM.put(9, dotPresetIndexN);
    EEPROM.put(10, dateShowAllowed);
    EEPROM.put(11, simplifiedMode);
  }
  EEPROM.get(0, FLIP_EFFECT);
  EEPROM.get(1, BACKL_MODE);
  EEPROM.get(2, GLITCH_ALLOWED);
  EEPROM.get(3, DOT_MODE);
  EEPROM.get(4, indiPresetIndex);
  EEPROM.get(5, backlPresetIndex);
  EEPROM.get(6, indiPresetIndexN);
  EEPROM.get(7, backlPresetIndexN);
  EEPROM.get(8, dotPresetIndex);
  EEPROM.get(9, dotPresetIndexN);
  EEPROM.get(10, dateShowAllowed);
  EEPROM.get(11, simplifiedMode);

  // Защита настроек EEPROM: если память содержит недопустимые значения,
  // возвращаем безопасные значения по умолчанию и сохраняем их.
  if (FLIP_EFFECT >= FLIP_EFFECT_NUM) { FLIP_EFFECT = 1; EEPROM.put(0, FLIP_EFFECT); }
  if (BACKL_MODE > 2) { BACKL_MODE = 0; EEPROM.put(1, BACKL_MODE); }
  if (GLITCH_ALLOWED > 1) { GLITCH_ALLOWED = 1; EEPROM.put(2, GLITCH_ALLOWED); }
  if (DOT_MODE > 2) { DOT_MODE = 0; EEPROM.put(3, DOT_MODE); }
  if (indiPresetIndex >= 5) { indiPresetIndex = 4; EEPROM.put(4, indiPresetIndex); }
  if (backlPresetIndex >= 5) { backlPresetIndex = 4; EEPROM.put(5, backlPresetIndex); }
  if (indiPresetIndexN >= 5) { indiPresetIndexN = 4; EEPROM.put(6, indiPresetIndexN); }
  if (backlPresetIndexN >= 5) { backlPresetIndexN = 4; EEPROM.put(7, backlPresetIndexN); }
  if (dotPresetIndex >= 5) { dotPresetIndex = 4; EEPROM.put(8, dotPresetIndex); }
  if (dotPresetIndexN >= 5) { dotPresetIndexN = 4; EEPROM.put(9, dotPresetIndexN); }
  if (dateShowAllowed > 1) { dateShowAllowed = 1; EEPROM.put(10, dateShowAllowed); }
  if (simplifiedMode > 1) { simplifiedMode = 1; EEPROM.put(11, simplifiedMode); }

  // переключение полного/упрощённого режима управления (см. SIMPLIFIED_MODE_ADDR
  // выше) - после того, как текущий режим уже загружен и проверен
  if (toggleControlMode) {
    simplifiedMode = !simplifiedMode;
    EEPROM.put(SIMPLIFIED_MODE_ADDR, simplifiedMode);
    showControlModeToggle(simplifiedMode);
  }

  /*if (EEPROM.read(100) != 66) {   // проверка на первый запуск. 66 от балды
    EEPROM.write(100, 66);
    EEPROM.write(0, 0);     // часы будильника
    EEPROM.write(1, 0);     // минуты будильника
    }
    alm_hrs = EEPROM.read(0);
    alm_mins = EEPROM.read(1);*/

  sendTime(hrs, mins);  // отправить время на индикаторы
  changeBright();       // изменить яркость согласно времени суток

  // старт "дыхания" подсветки от максимума (а не от нуля вверх) - чтобы
  // первый цикл дыхания после включения выглядел так же, как и все
  // последующие, а не с необычно длинным начальным разгоном
  if (BACKL_MODE == 0) {
    backlBrightCounter = backlMaxBright;
    backlBrightDirection = false;
    backlBrightFlag = false;
    setBackl(backlMaxBright);
  }

  // установить яркость на индикаторы
  for (byte i = 0; i < 4; i++)
    indiDimm[i] = indiMaxBright;

  // расчёт шага яркости точки
  dotBrightStep = ceil((float)dotMaxBright * 2 / DOT_TIME * DOT_TIMER);
  if (dotBrightStep == 0) dotBrightStep = 1;

  // дыхание подсветки
  if (backlMaxBright > 0)
    backlBrightTimer.setInterval((float)BACKL_STEP / backlMaxBright / 2 * BACKL_TIME);

  // стартовый период глюков
  glitchTimer.setInterval(random(GLITCH_MIN * 1000L, GLITCH_MAX * 1000L));
  indiBrightCounter = indiMaxBright;

  // скорость режима при запуске
  flipTimer.setInterval(FLIP_SPEED[FLIP_EFFECT]);
  //almTimer.stop();
}
