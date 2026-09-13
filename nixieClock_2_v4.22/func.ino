


// Синхронизация программного времени с DS3231 после блокирующих процедур.
void syncTimeFromRTC() {
  DateTime now = rtc.now();
  hrs = now.hour();
  mins = now.minute();
  secs = now.second();
  minsCount = mins % 15;
  setNewTime();
  newTimeFlag = true;
}

void burnIndicators() {
  for (byte k = 0; k < BURN_LOOPS; k++) {
    for (byte d = 0; d < 10; d++) {
      for (byte i = 0; i < 4; i++) {
        if (indiDigits[i] == 0) indiDigits[i] = 9;
        else indiDigits[i]--;
      }
      delay(BURN_TIME);
    }
  }
  syncTimeFromRTC();
}

// показать номер прошивки на FW_VERSION_SHOW_TIME мс перед тестом ламп
// (запускается вместе с тестом - при включении с зажатой кнопкой "-").
// Формат: 1-я лампа погашена, 2-я - старшая цифра версии, точка-разделитель
// включена, 3-я и 4-я лампы - младшие две цифры версии.
void showFirmwareVersion() {
  anodeStates[0] = 0;
  anodeStates[1] = 1;
  anodeStates[2] = 1;
  anodeStates[3] = 1;
  indiDigits[1] = FW_VERSION_MAJOR;
  indiDigits[2] = FW_VERSION_MINOR / 10;
  indiDigits[3] = FW_VERSION_MINOR % 10;
  for (byte i = 0; i < 4; i++) indiDimm[i] = indiMaxBright;
  setPWM(DOT, 255);   // точка-разделитель на полную яркость, независимо от настроек DOT_MODE
  delay(FW_VERSION_SHOW_TIME);
  setPWM(DOT, 0);     // погасить точку обратно
  anodeStates[0] = 1; // вернуть 1-ю лампу во включенное состояние для дальнейшего теста
}

// тест ламп при включении (запускается, если при старте зажата кнопка "-")
void lampTest() {
  // 1) все лампы одновременно прогоняют цифры 0-9
  for (byte i = 0; i < 4; i++) anodeStates[i] = 1;
  for (byte d = 0; d < 10; d++) {
    for (byte i = 0; i < 4; i++) {
      indiDigits[i] = d;
      indiDimm[i] = indiMaxBright;
    }
    delay(LAMPTEST_ALL_DELAY);
  }

  // 2) лампы проверяются по очереди, остальные в это время погашены
  for (byte lamp = 0; lamp < 4; lamp++) {
    for (byte i = 0; i < 4; i++) anodeStates[i] = (i == lamp) ? 1 : 0;
    for (byte d = 0; d < 10; d++) {
      indiDigits[lamp] = d;
      indiDimm[lamp] = indiMaxBright;
      delay(LAMPTEST_ONE_DELAY);
    }
  }

  // возвращаем все лампы во включенное состояние для дальнейшей нормальной работы
  for (byte i = 0; i < 4; i++) anodeStates[i] = 1;
}

// обратный отсчёт 3-2-1-0 на всех лампах - подтверждение сброса настроек к заводским
void showResetCountdown() {
  for (byte i = 0; i < 4; i++) anodeStates[i] = 1;
  for (int8_t n = 3; n >= 0; n--) {
    for (byte i = 0; i < 4; i++) {
      indiDigits[i] = n;
      indiDimm[i] = indiMaxBright;
    }
    delay(RESET_COUNTDOWN_DELAY);
  }
}

// подтверждение переключения режима управления при включении питания:
// на всех лампах на CONTROL_MODE_INFO_TIME мс показывается 1 (упрощённый
// режим, по умолчанию) или 2 (полный режим)
void showControlModeToggle(boolean simplified) {
  for (byte i = 0; i < 4; i++) {
    anodeStates[i] = 1;
    indiDigits[i] = simplified ? 1 : 2;
    indiDimm[i] = indiMaxBright;
  }
  delay(CONTROL_MODE_INFO_TIME);
}

// запустить неблокирующий показ номера выбранного режима на всех лампах на
// MODE_INFO_TIME мс (curMode == 4). Не использует delay() - в отличие от
// старой блокирующей версии, кнопки во время показа продолжают опрашиваться
// и не "теряются" и не засчитываются не туда при быстром переключении.
void startModeInfo(byte num) {
  for (byte i = 0; i < 4; i++) {
    anodeStates[i] = 1;
    indiDigits[i] = num;
    indiDimm[i] = indiMaxBright;
  }
  modeInfoTimer.reset();
  modeInfoShowing = true;
  curMode = 4;
}

// завершение показа номера режима - вызывается из loop() (см. modeInfoTick())
// когда истёк MODE_INFO_TIME; возвращает часы к обычному показу времени
void finishModeInfo() {
  modeInfoShowing = false;
  curMode = 0;
  syncTimeFromRTC();   // пересинхронизация + запускает переход (флип) от номера режима к текущему времени
}

// проверка таймера неблокирующего показа номера режима, вызывается каждую
// итерацию loop()
void modeInfoTick() {
  if (modeInfoShowing && modeInfoTimer.isReady()) {
    finishModeInfo();
  }
}

// применить выбранный пресет яркости немедленно (живой предпросмотр в режиме настройки)
// brightSubMode: 0/1-подсветка день/ночь, 2/3-лампы день/ночь, 4/5-точка день/ночь, 6-выход
void applyBrightPreview() {
  boolean nightEdit = (brightSubMode == 1 || brightSubMode == 3 || brightSubMode == 5);
  if (!nightEdit) {
    indiMaxBright = INDI_PRESETS[indiPresetIndex];
    backlMaxBright = BACKL_PRESETS[backlPresetIndex];
    dotMaxBright = DOT_PRESETS[dotPresetIndex];
  } else {
    indiMaxBright = INDI_PRESETS_N[indiPresetIndexN];
    backlMaxBright = BACKL_PRESETS_N[backlPresetIndexN];
    dotMaxBright = DOT_PRESETS_N[dotPresetIndexN];
  }
  for (byte i = 0; i < 4; i++) indiDimm[i] = indiMaxBright;
  dotBrightStep = ceil((float)dotMaxBright * 2 / DOT_TIME * DOT_TIMER);
  if (dotBrightStep == 0) dotBrightStep = 1;

  if (backlMaxBright > 0)
    backlBrightTimer.setInterval((float)BACKL_STEP / backlMaxBright / 2 * BACKL_TIME);
  if (BACKL_MODE == 1) setBackl(backlMaxBright);   // постоянная подсветка - применить сразу
  if (DOT_MODE == 1) setPWM(DOT, getPWM_CRT(dotMaxBright)); // постоянная точка - применить сразу
  else if (DOT_MODE == 2) digitalWrite(DOT, 0);              // выключенная точка - выключить сразу
}
