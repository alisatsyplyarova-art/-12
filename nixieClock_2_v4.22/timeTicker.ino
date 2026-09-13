void calculateTime() {
  dotFlag = !dotFlag;
  if (dotFlag) {
    if (DOT_MODE == 0) {           // мигание точки только в режиме "мигает"
      dotBrightFlag = true;
      dotBrightDirection = true;
      dotBrightCounter = 0;
    }
    secs++;
    if (secs > 59) {
      newTimeFlag = true;   // флаг что нужно поменять время
      secs = 0;
      mins++;
      minsCount++;

      if (minsCount >= 15) {            // каждые 15 мин
        minsCount = 0;
        DateTime now = rtc.now();       // синхронизация с RTC
        // Если чтение выглядит явно "мусорным" (сбой I2C в момент опроса) -
        // пропускаем этот раз, продолжаем считать время локально дальше;
        // следующая попытка синхронизации будет через 15 минут.
        if (now.hour() < 24 && now.minute() < 60 && now.second() < 60) {
          secs = now.second();
          mins = now.minute();
          hrs = now.hour();
        }
      }

      if (curMode == 0 && mins % BURN_PERIOD == 0) burnIndicators();     // чистим чистим! (только в режиме часов)

      /*if (!alm_flag && alm_mins == mins && alm_hrs == hrs && true) {
        mode = 0;
        alm_flag = true;
        almTimer.start();
        almTimer.reset();
        }*/
    }
    if (mins > 59) {
      mins = 0;
      hrs++;
      if (hrs > 23) hrs = 0;
      changeBright();
    }
    if (newTimeFlag) setNewTime();         // обновляем массив времени

    /*
        if (mode == 0) sendTime(hrs, mins);

        if (alm_flag) {
          if (almTimer.isReady() || true ) {
            alm_flag = false;
            almTimer.stop();
            mode = 0;
            noTone(PIEZO);
          }
        }
    */
  }

  /*
    // мигать на будильнике
    if (alm_flag) {
      if (!dotFlag) {
        noTone(PIEZO);
        for (byte i = 1; i < 7; i++) digitsDraw[i] = 10;
      } else {
        tone(PIEZO, FREQ);
        sendTime(hrs, mins);
      }
    }
  */
}
