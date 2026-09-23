void settingsTick() {
  if (curMode == 1) {
    // ------- настройка времени и даты -------
    if (settingsTimeoutTimer.isReady()) {
      // бездействие SETTINGS_TIMEOUT (по умолчанию 2 минуты) - выход БЕЗ
      // сохранения, изменения времени/даты/года отбрасываются
      timeSetStage = 0;
      for (byte i = 0; i < 4; i++) anodeStates[i] = 1;
      syncTimeFromRTC();
      sendTime(hrs, mins);
      dateShowTimer.reset();   // отсчёт до автопоказа даты начинается заново от возврата в часы
      curMode = 0;
      return;
    }
    // timeSetStage: 0-часы, 1-минуты, 2-число, 3-месяц, 4-год
    if (blinkTimer.isReady()) {
      if (timeSetStage <= 1) sendTime(changeHrs, changeMins);
      else if (timeSetStage <= 3) sendDate(changeDay, changeMonth);
      else sendYear(changeYear);
      lampState = !lampState;
      if (lampState) {
        anodeStates[0] = 1;
        anodeStates[1] = 1;
        anodeStates[2] = 1;
        anodeStates[3] = 1;
      } else {
        switch (timeSetStage) {
          case 0:
          case 2:                                        // мигают часы или число (лампы 1-2)
            anodeStates[0] = 0;
            anodeStates[1] = 0;
            break;
          case 1:
          case 3:                                         // мигают минуты или месяц (лампы 3-4)
            anodeStates[2] = 0;
            anodeStates[3] = 0;
            break;
          default:                                         // 4 - год, мигают все 4 лампы разом
            anodeStates[0] = 0;
            anodeStates[1] = 0;
            anodeStates[2] = 0;
            anodeStates[3] = 0;
            break;
        }
      }
    }
  } else if (curMode == 2) {
    // режим настройки яркости:
    if (brightTimeoutTimer.isReady()) {
      // бездействие BRIGHT_TIMEOUT (по умолчанию 60 секунд) - выход БЕЗ
      // сохранения: восстанавливаем пресеты из EEPROM, отбрасывая
      // несохранённый "живой" предпросмотр
      EEPROM.get(4, indiPresetIndex);
      EEPROM.get(5, backlPresetIndex);
      EEPROM.get(6, indiPresetIndexN);
      EEPROM.get(7, backlPresetIndexN);
      EEPROM.get(8, dotPresetIndex);
      EEPROM.get(9, dotPresetIndexN);
      changeBright();
      for (byte i = 0; i < 4; i++) anodeStates[i] = 1;
      sendTime(hrs, mins);
      dateShowTimer.reset();   // отсчёт до автопоказа даты начинается заново от возврата в часы
      curMode = 0;
      return;
    }
    // 0/1-подсветка день/ночь, 2/3-лампы день/ночь, 4/5-точка день/ночь, 6-выход
    if (brightSubMode == 6) {
      // "выход" - мигают 1,3,4 лампы; вторая всегда погашена (как и в категориях)
      if (blinkTimer.isReady()) {
        lampState = !lampState;
        anodeStates[0] = lampState;
        anodeStates[2] = lampState;
        anodeStates[3] = lampState;
      }
      anodeStates[1] = 0;
    } else {
      // отображение: [категория 1-6] [погашена] [пресет: десятки] [пресет: единицы]
      byte presetNum;
      switch (brightSubMode) {
        case 0: presetNum = backlPresetIndex + 1;  break;   // день, подсветка
        case 1: presetNum = backlPresetIndexN + 1; break;   // ночь, подсветка
        case 2: presetNum = indiPresetIndex + 1;   break;   // день, лампы
        case 3: presetNum = indiPresetIndexN + 1;  break;   // ночь, лампы
        case 4: presetNum = dotPresetIndex + 1;    break;   // день, точка
        default: presetNum = dotPresetIndexN + 1;  break;   // ночь, точка (case 5)
      }
      for (byte i = 0; i < 4; i++) {
        anodeStates[i] = 1;
        indiDimm[i] = indiMaxBright;
      }
      anodeStates[1] = 0;                     // вторая лампа всегда погашена (разделитель)
      indiDigits[0] = brightSubMode + 1;       // номер категории 1-6
      indiDigits[2] = presetNum / 10;          // десятки номера пресета
      indiDigits[3] = presetNum % 10;          // единицы номера пресета
    }
  }
}

void buttonsTick() {
  btnSet.tick();
  btnL.tick();
  btnR.tick();

  if (curMode == 1) {
    // ------- настройка времени и даты -------
    // timeSetStage: 0-часы, 1-минуты, 2-число, 3-месяц, 4-год
    if (btnR.isClick()) {
      settingsTimeoutTimer.reset();   // есть активность - сбросить таймер автовыхода по бездействию
      switch (timeSetStage) {
        case 0:
          changeHrs++;
          if (changeHrs > 23) changeHrs = 0;
          break;
        case 1:
          changeMins++;
          if (changeMins > 59) {
            changeMins = 0;
            changeHrs++;
            if (changeHrs > 23) changeHrs = 0;
          }
          break;
        case 2:
          changeDay++;
          if (changeDay > daysInMonth(changeMonth, changeYear)) changeDay = 1;
          break;
        case 3:
          changeMonth++;
          if (changeMonth > 12) changeMonth = 0;    // 00 - показ даты выключен
          break;
        case 4:
          changeYear++;
          if (changeYear > 2099) changeYear = 2000;
          break;
      }
      clampChangeDay();   // если число стало больше, чем дней в выбранном месяце/году - уменьшить до максимума
      if (timeSetStage <= 1) sendTime(changeHrs, changeMins);
      else if (timeSetStage <= 3) sendDate(changeDay, changeMonth);
      else sendYear(changeYear);
    }
    if (btnL.isClick()) {
      settingsTimeoutTimer.reset();   // есть активность - сбросить таймер автовыхода по бездействию
      switch (timeSetStage) {
        case 0:
          changeHrs--;
          if (changeHrs < 0) changeHrs = 23;
          break;
        case 1:
          changeMins--;
          if (changeMins < 0) {
            changeMins = 59;
            changeHrs--;
            if (changeHrs < 0) changeHrs = 23;
          }
          break;
        case 2:
          changeDay--;
          if (changeDay < 1) changeDay = daysInMonth(changeMonth, changeYear);
          break;
        case 3:
          changeMonth--;
          if (changeMonth < 0) changeMonth = 12;    // 00 - показ даты выключен
          break;
        case 4:
          changeYear--;
          if (changeYear < 2000) changeYear = 2099;
          break;
      }
      clampChangeDay();   // если число стало больше, чем дней в выбранном месяце/году - уменьшить до максимума
      if (timeSetStage <= 1) sendTime(changeHrs, changeMins);
      else if (timeSetStage <= 3) sendDate(changeDay, changeMonth);
      else sendYear(changeYear);
    }
    if (btnSet.isClick()) {
      settingsTimeoutTimer.reset();   // есть активность - сбросить таймер автовыхода по бездействию
      // цикл: часы -> минуты -> число -> месяц -> год -> часы...
      // в упрощённом режиме дата/год недоступны - цикл останавливается на минутах
      byte maxStage = simplifiedMode ? 1 : 4;
      if (++timeSetStage > maxStage) timeSetStage = 0;
      if (timeSetStage <= 1) sendTime(changeHrs, changeMins);
      else if (timeSetStage <= 3) sendDate(changeDay, changeMonth);
      else sendYear(changeYear);
    }
    if (btnSet.isHolded()) {
      // сохранить время И дату, вернуться в часы
      hrs = changeHrs;
      mins = changeMins;
      secs = 0;
      if (changeMonth == 0) {
        // месяц 00 - показ даты выключается, сама дата (число/месяц/год) в RTC не трогается
        dateShowAllowed = false;
        EEPROM.put(10, dateShowAllowed);
        DateTime now = rtcNowSafe();
        rtc.adjust(DateTime(now.year(), now.month(), now.day(), hrs, mins, 0));
      } else {
        dateShowAllowed = true;
        EEPROM.put(10, dateShowAllowed);
        rtc.adjust(DateTime(changeYear, changeMonth, changeDay, hrs, mins, 0));
      }
      dateShowing = false;   // если показ даты был выключен во время показа - сразу прервать его
      changeBright();
      for (byte i = 0; i < 4; i++) anodeStates[i] = 1;
      sendTime(hrs, mins);
      // После ручной установки времени сбрасываем состояние эффекта.
      flipInit = false;
      newTimeFlag = false;
      indiBrightDirection = false;
      indiBrightCounter = indiMaxBright;
      flipTimer.reset();
      timeSetStage = 0;
      dateShowTimer.reset();   // отсчёт до автопоказа даты начинается заново от возврата в часы
      curMode = 0;
    }
  } else if (curMode == 0 || curMode == 4) {
    // curMode == 4 - идёт неблокирующий показ номера режима (см. startModeInfo()):
    // быстрые переключатели ниже продолжают работать и во время него, чтобы кнопки
    // не "терялись" при быстром переключении - новое нажатие сразу подхватывается
    // и перезапускает показ с новым номером, без вынужденного ожидания

    // переключение эффектов цифр
    if (btnR.isClick()) {
      if (++FLIP_EFFECT >= FLIP_EFFECT_NUM) FLIP_EFFECT = 0;
      EEPROM.put(0, FLIP_EFFECT);
      flipTimer.setInterval(FLIP_SPEED[FLIP_EFFECT]);
      flipInit = false;   // сброс состояния предыдущего эффекта - без этого, если предыдущая
                           // анимация была прервана на середине, новый эффект пропускал бы
                           // свою инициализацию и завис бы на "мусорных" данных предыдущего
      startModeInfo(FLIP_EFFECT + 1);   // неблокирующий показ номера эффекта (нумерация с 1)
    }

    // переключение эффектов подсветки
    if (btnL.isClick()) {
      if (++BACKL_MODE >= 3) BACKL_MODE = 0;
      EEPROM.put(1, BACKL_MODE);
      if (BACKL_MODE == 1) {
        setBackl(backlMaxBright);
      } else if (BACKL_MODE == 2) {
        setBackl(0);
      }
      startModeInfo(BACKL_MODE + 1);   // неблокирующий показ номера режима подсветки (нумерация с 1)
    }

    // переключение глюков
    if (btnL.isHolded()) {
      GLITCH_ALLOWED = !GLITCH_ALLOWED;
      EEPROM.put(2, GLITCH_ALLOWED);
    }

    // переключение режима точки (удержание правой кнопки "+") - недоступно в упрощённом режиме
    if (!simplifiedMode && btnR.isHolded()) {
      if (++DOT_MODE >= 3) DOT_MODE = 0;
      EEPROM.put(3, DOT_MODE);
      dotBrightFlag = false;   // прервать цикл мигания точки - без этого он может по инерции
                                // продолжить работать и перезаписать состояние, выставленное ниже
      if (DOT_MODE == 1) {
        setPWM(DOT, getPWM_CRT(dotMaxBright));
      } else if (DOT_MODE == 2) {
        setPWM(DOT, 0);
      }
      startModeInfo(DOT_MODE + 1);   // неблокирующий показ номера режима точки (нумерация с 1)
    }

    // клик "выбор" - войти в настройку яркости (недоступно в упрощённом режиме)
    if (!simplifiedMode && btnSet.isClick()) {
      modeInfoShowing = false;   // если шёл показ номера режима - прерываем его
      curMode = 2;
      brightSubMode = 0;
      brightTimeoutTimer.reset();
      dateShowTimer.reset();   // не считать время в меню яркости как часть интервала до автопоказа даты
      for (byte i = 0; i < 4; i++) anodeStates[i] = 1;
    }
    // удержание "выбора" - войти в настройку времени и даты
    if (btnSet.isHolded()) {
      modeInfoShowing = false;   // если шёл показ номера режима - прерываем его
      curMode = 1;
      settingsTimeoutTimer.reset();
      dateShowTimer.reset();   // не считать время в настройке как часть интервала до автопоказа даты
      changeHrs = hrs;
      changeMins = mins;
      {
        DateTime now = rtcNowSafe();
        changeDay = now.day();
        changeMonth = dateShowAllowed ? now.month() : 0;   // 00, если показ даты сейчас выключен
        changeYear = now.year();
      }
      clampChangeDay();
      timeSetStage = 0;
      for (byte i = 0; i < 4; i++) anodeStates[i] = 1;
    }
  } else if (curMode == 2) {
    // ------- режим настройки яркости -------
    // 0/1-подсветка день/ночь, 2/3-лампы день/ночь, 4/5-точка день/ночь, 6-выход
    if (btnR.isClick()) {
      brightTimeoutTimer.reset();   // есть активность - сбросить таймер автовыхода по бездействию
      switch (brightSubMode) {
        case 0:
          if (++backlPresetIndex >= 5) backlPresetIndex = 0;
          applyBrightPreview();
          break;
        case 1:
          if (++backlPresetIndexN >= 5) backlPresetIndexN = 0;
          applyBrightPreview();
          break;
        case 2:
          if (++indiPresetIndex >= 5) indiPresetIndex = 0;
          applyBrightPreview();
          break;
        case 3:
          if (++indiPresetIndexN >= 5) indiPresetIndexN = 0;
          applyBrightPreview();
          break;
        case 4:
          if (++dotPresetIndex >= 5) dotPresetIndex = 0;
          applyBrightPreview();
          break;
        case 5:
          if (++dotPresetIndexN >= 5) dotPresetIndexN = 0;
          applyBrightPreview();
          break;
        case 6:
          // "выход" - подтверждение и сохранение
          EEPROM.put(4, indiPresetIndex);
          EEPROM.put(5, backlPresetIndex);
          EEPROM.put(6, indiPresetIndexN);
          EEPROM.put(7, backlPresetIndexN);
          EEPROM.put(8, dotPresetIndex);
          EEPROM.put(9, dotPresetIndexN);
          changeBright();
          for (byte i = 0; i < 4; i++) anodeStates[i] = 1;
          sendTime(hrs, mins);
          dateShowTimer.reset();   // отсчёт до автопоказа даты начинается заново от возврата в часы
          curMode = 0;
          break;
      }
    }
    if (btnL.isClick()) {
      brightTimeoutTimer.reset();   // есть активность - сбросить таймер автовыхода по бездействию
      switch (brightSubMode) {
        case 0:
          if (backlPresetIndex == 0) backlPresetIndex = 4; else backlPresetIndex--;
          applyBrightPreview();
          break;
        case 1:
          if (backlPresetIndexN == 0) backlPresetIndexN = 4; else backlPresetIndexN--;
          applyBrightPreview();
          break;
        case 2:
          if (indiPresetIndex == 0) indiPresetIndex = 4; else indiPresetIndex--;
          applyBrightPreview();
          break;
        case 3:
          if (indiPresetIndexN == 0) indiPresetIndexN = 4; else indiPresetIndexN--;
          applyBrightPreview();
          break;
        case 4:
          if (dotPresetIndex == 0) dotPresetIndex = 4; else dotPresetIndex--;
          applyBrightPreview();
          break;
        case 5:
          if (dotPresetIndexN == 0) dotPresetIndexN = 4; else dotPresetIndexN--;
          applyBrightPreview();
          break;
        case 6:
          // "выход" - подтверждение и сохранение
          EEPROM.put(4, indiPresetIndex);
          EEPROM.put(5, backlPresetIndex);
          EEPROM.put(6, indiPresetIndexN);
          EEPROM.put(7, backlPresetIndexN);
          EEPROM.put(8, dotPresetIndex);
          EEPROM.put(9, dotPresetIndexN);
          changeBright();
          for (byte i = 0; i < 4; i++) anodeStates[i] = 1;
          sendTime(hrs, mins);
          dateShowTimer.reset();   // отсчёт до автопоказа даты начинается заново от возврата в часы
          curMode = 0;
          break;
      }
    }
    if (btnSet.isClick()) {
      brightTimeoutTimer.reset();   // есть активность - сбросить таймер автовыхода по бездействию
      // цикл: 0/1-подсветка д/н -> 2/3-лампы д/н -> 4/5-точка д/н -> 6-выход
      if (++brightSubMode >= 7) brightSubMode = 0;
    }
    if (btnSet.isHolded()) {
      // сохранить пресеты и вернуться в часы
      EEPROM.put(4, indiPresetIndex);
      EEPROM.put(5, backlPresetIndex);
      EEPROM.put(6, indiPresetIndexN);
      EEPROM.put(7, backlPresetIndexN);
      EEPROM.put(8, dotPresetIndex);
      EEPROM.put(9, dotPresetIndexN);
      changeBright();
      for (byte i = 0; i < 4; i++) anodeStates[i] = 1;
      sendTime(hrs, mins);
      dateShowTimer.reset();   // отсчёт до автопоказа даты начинается заново от возврата в часы
      curMode = 0;
    }
  } else if (curMode == 3) {
    // ------- автопоказ даты в режиме часов -------
    // нажатие любой кнопки сразу прерывает показ даты и возвращает к часам
    if (btnSet.isClick() || btnSet.isHolded() ||
        btnL.isClick()  || btnL.isHolded()   ||
        btnR.isClick()  || btnR.isHolded()) {
      dateShowing = false;
      curMode = 0;
      sendTime(hrs, mins);
      dateShowTimer.reset();   // следующий показ - через полный интервал, а не сразу же
    }
  }
}
