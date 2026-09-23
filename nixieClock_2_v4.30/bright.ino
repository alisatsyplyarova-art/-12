void backlBrightTick() {
  if (BACKL_MODE == 0 && backlBrightTimer.isReady()) {
    if (backlMaxBright > 0) {
      if (backlBrightDirection) {
        if (!backlBrightFlag) {
          backlBrightFlag = true;
          backlBrightTimer.setInterval((float)BACKL_STEP / backlMaxBright / 2 * BACKL_TIME);
        }
        backlBrightCounter += BACKL_STEP;
        if (backlBrightCounter >= backlMaxBright) {
          backlBrightDirection = false;
          backlBrightCounter = backlMaxBright;
        }
      } else {
        backlBrightCounter -= BACKL_STEP;
        if (backlBrightCounter <= BACKL_MIN_BRIGHT) {
          backlBrightDirection = true;
          backlBrightCounter = BACKL_MIN_BRIGHT;
          backlBrightTimer.setInterval(BACKL_PAUSE);
          backlBrightFlag = false;
        }
      }
      setBackl(getPWM_CRT(backlBrightCounter));
    } else {
      setBackl(0);
    }
  }
}

void dotBrightTick() {
  // жёсткая защита: состояние точки принудительно поддерживается на каждом
  // тике в соответствии с текущим DOT_MODE, а не полагается только на
  // разовую установку в момент переключения кнопкой - если что-то ещё
  // попытается изменить пин точки, это состояние тут же восстановится
  if (DOT_MODE == 2) {
    dotBrightFlag = false;
    dotBrightCounter = 0;
    setPWM(DOT, 0);
    return;
  }
  if (DOT_MODE == 1) {
    dotBrightFlag = false;
    dotBrightCounter = dotMaxBright;
    setPWM(DOT, getPWM_CRT(dotMaxBright));
    return;
  }

  // DOT_MODE == 0 - мигание
  if (dotBrightFlag && dotBrightTimer.isReady()) {
    if (dotBrightDirection) {
      dotBrightCounter += dotBrightStep;
      if (dotBrightCounter >= dotMaxBright) {
        dotBrightDirection = false;
        dotBrightCounter = dotMaxBright;
      }
    } else {
      dotBrightCounter -= dotBrightStep;
      if (dotBrightCounter <= 0) {
        dotBrightDirection = true;
        dotBrightFlag = false;
        dotBrightCounter = 0;
      }
    }
    setPWM(DOT, getPWM_CRT(dotBrightCounter));
  }
}

void changeBright() {
  // установка дневной/ночной яркости; и та, и другая берутся из своих пресетов
#if (NIGHT_LIGHT == 1)
  if ( (hrs >= NIGHT_START && hrs <= 23)
       || (hrs >= 0 && hrs < NIGHT_END) ) {
    indiMaxBright = INDI_PRESETS_N[indiPresetIndexN];
    dotMaxBright = DOT_PRESETS_N[dotPresetIndexN];
    backlMaxBright = BACKL_PRESETS_N[backlPresetIndexN];
  } else {
    indiMaxBright = INDI_PRESETS[indiPresetIndex];
    dotMaxBright = DOT_PRESETS[dotPresetIndex];
    backlMaxBright = BACKL_PRESETS[backlPresetIndex];
  }
#else
  indiMaxBright = INDI_PRESETS[indiPresetIndex];
  dotMaxBright = DOT_PRESETS[dotPresetIndex];
  backlMaxBright = BACKL_PRESETS[backlPresetIndex];
#endif

  for (byte i = 0; i < 4; i++) {
    indiDimm[i] = indiMaxBright;
  }

  dotBrightStep = ceil((float)dotMaxBright * 2 / DOT_TIME * DOT_TIMER);
  if (dotBrightStep == 0) dotBrightStep = 1;

  if (backlMaxBright > 0)
    backlBrightTimer.setInterval((float)BACKL_STEP / backlMaxBright / 2 * BACKL_TIME);
  indiBrightCounter = indiMaxBright;

  // полностью определить состояние BACKL для любого режима (без побочных
  // эффектов на вызовы changeBright() по несвязанным поводам - например,
  // simple clamp сверху в режиме "дыхание" ничего не меняет, если максимум
  // не менялся, а просто подрезает счётчик, если новый максимум меньше)
  if (BACKL_MODE == 0) {
    if (backlMaxBright == 0) {
      backlBrightCounter = 0;
      backlBrightDirection = true;
      backlBrightFlag = false;
      setBackl(0);
    } else {
      if (backlBrightCounter > backlMaxBright) backlBrightCounter = backlMaxBright;
      setBackl(getPWM_CRT(backlBrightCounter));
    }
  } else if (BACKL_MODE == 1) {
    backlBrightCounter = backlMaxBright;
    backlBrightDirection = false;
    backlBrightFlag = false;
    setBackl(backlMaxBright);
  } else {
    backlBrightCounter = 0;
    backlBrightDirection = true;
    backlBrightFlag = false;
    setBackl(0);
  }

  //пересчитать яркость точки для режима "горит постоянно" (0 - мигает, 1 - горит, 2 - выкл)
  if (DOT_MODE == 1) setPWM(DOT, getPWM_CRT(dotMaxBright));
}
