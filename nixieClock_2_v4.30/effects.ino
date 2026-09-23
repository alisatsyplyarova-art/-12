void flipTick() {
  if (FLIP_EFFECT == 0) {
    sendTime(hrs, mins);
    newTimeFlag = false;
  }
  else if (FLIP_EFFECT == 1) {
    if (!flipInit) {
      flipInit = true;
      // запоминаем, какие цифры поменялись и будем менять их яркость
      byte changedCount = 0;
      for (byte i = 0; i < 4; i++) {
        if (indiDigits[i] != newTime[i]) {
          flipIndics[i] = true;
          changedCount++;
        } else {
          flipIndics[i] = false;
        }
      }
      // Если ни одна цифра не изменилась, эффект не запускаем.
      if (changedCount == 0) {
        flipInit = false;
        newTimeFlag = false;
      }
    }
    if (flipInit && flipTimer.isReady()) {
      if (!indiBrightDirection) {
        indiBrightCounter--;            // уменьшаем яркость
        if (indiBrightCounter <= 0) {   // если яроксть меньше нуля
          indiBrightDirection = true;   // меняем направление изменения
          indiBrightCounter = 0;        // обнуляем яркость
          sendTime(hrs, mins);          // меняем цифры
        }
      } else {
        indiBrightCounter++;                        // увеличиваем яркость
        if (indiBrightCounter >= indiMaxBright) {   // достигли предела
          indiBrightDirection = false;              // меняем направление
          indiBrightCounter = indiMaxBright;        // устанавливаем максимум
          // выходим из цикла изменения
          flipInit = false;
          newTimeFlag = false;
        }
      }
      for (byte i = 0; i < 4; i++)
        if (flipIndics[i]) indiDimm[i] = indiBrightCounter;   // применяем яркость
    }
  }
  else if (FLIP_EFFECT == 2) {
    if (!flipInit) {
      flipInit = true;
      // запоминаем, какие цифры поменялись и будем менять их
      for (byte i = 0; i < 4; i++) {
        if (indiDigits[i] != newTime[i]) flipIndics[i] = true;
        else flipIndics[i] = false;
      }
    }

    if (flipTimer.isReady()) {
      byte flipCounter = 0;
      for (byte i = 0; i < 4; i++) {
        if (flipIndics[i]) {
          if (indiDigits[i] == 0) indiDigits[i] = 9;
          else indiDigits[i]--;
          if (indiDigits[i] == newTime[i]) flipIndics[i] = false;
        } else {
          flipCounter++;        // счётчик цифр, которые не надо менять
        }
      }
      if (flipCounter == 4) {   // если ни одну из 4 цифр менять не нужно
        // выходим из цикла изменения
        flipInit = false;
        newTimeFlag = false;
      }
    }

    //byte cathodeMask[] = {1, 0, 2, 9, 3, 8, 4, 7, 5, 6};  // порядок катодов in14
  }
  else if (FLIP_EFFECT == 3) {
    if (!flipInit) {
      flipInit = true;
      // запоминаем, какие цифры поменялись и будем менять их
      for (byte i = 0; i < 4; i++) {
        if (indiDigits[i] != newTime[i]) {
          flipIndics[i] = true;
          for (byte c = 0; c < 10; c++) {
            if (cathodeMask[c] == indiDigits[i]) startCathode[i] = c;
            if (cathodeMask[c] == newTime[i]) endCathode[i] = c;
          }
        }
        else flipIndics[i] = false;
      }
    }

    if (flipTimer.isReady()) {
      byte flipCounter = 0;
      for (byte i = 0; i < 4; i++) {
        if (flipIndics[i]) {
          if (startCathode[i] > endCathode[i]) {
            startCathode[i]--;
            indiDigits[i] = cathodeMask[startCathode[i]];
          } else if (startCathode[i] < endCathode[i]) {
            startCathode[i]++;
            indiDigits[i] = cathodeMask[startCathode[i]];
          } else {
            flipIndics[i] = false;
          }
        } else {
          flipCounter++;
        }
      }
      if (flipCounter == 4) {   // если ни одну из 4 цифр менять не нужно
        // выходим из цикла изменения
        flipInit = false;
        newTimeFlag = false;
      }
    }
  }
// --- train --- //
  else if (FLIP_EFFECT == 4) {
    if (!flipInit) {
      flipInit = true;
      currentLamp = 0;
      trainLeaving = true;
      flipTimer.reset();
    }
    if (flipTimer.isReady()) {
      if (trainLeaving) {
        for (byte i = 3; i > currentLamp; i--) {
          indiDigits[i] = indiDigits[i-1];
        }
        anodeStates[currentLamp] = 0;
        currentLamp++;
        if (currentLamp >= 4) {
          trainLeaving = false; //coming
          currentLamp = 0;
          //sendTime(hrs, mins);
        }
      }
      else { //trainLeaving == false
        for (byte i = currentLamp; i > 0; i--) {
          indiDigits[i] = indiDigits[i-1];
        }
        indiDigits[0] = newTime[3-currentLamp];
        anodeStates[currentLamp] = 1;
        currentLamp++;
        if (currentLamp >= 4) {
          flipInit = false;
          newTimeFlag = false;
        }
      }
    }
  }

// --- elastic band --- //
  else if (FLIP_EFFECT == 5) {
    if (!flipInit) {
      flipInit = true;
      flipEffectStages = 0;
      flipTimer.reset();
    }
    if (flipTimer.isReady()) {
      switch (flipEffectStages++) {
        case 1:
          anodeStates[3] = 0; break;
        case 2:
          anodeStates[2] = 0;
          indiDigits[3] = indiDigits[2];
          anodeStates[3] = 1; break;
        case 3:
          anodeStates[3] = 0; break;
        case 4:
          anodeStates[1] = 0;
          indiDigits[2] = indiDigits[1];
          anodeStates[2] = 1; break;
        case 5:
          anodeStates[2] = 0;
          indiDigits[3] = indiDigits[1];
          anodeStates[3] = 1; break;
        case 6:
          anodeStates[3] = 0; break;
        case 7:
          anodeStates[0] = 0;
          indiDigits[1] = indiDigits[0];
          anodeStates[1] = 1; break;
        case 8:
          anodeStates[1] = 0;
          indiDigits[2] = indiDigits[0];
          anodeStates[2] = 1; break;
        case 9:
          anodeStates[2] = 0;
          indiDigits[3] = indiDigits[0];
          anodeStates[3] = 1; break;
        case 10:
          anodeStates[3] = 0;
          //sendTime(hrs,mins);
		  break;
        case 11:
          indiDigits[0] = newTime[3];
          anodeStates[0] = 1; break;
        case 12:
          anodeStates[0] = 0;
          indiDigits[1] = newTime[3];
          anodeStates[1] = 1; break;
        case 13:
          anodeStates[1] = 0;
          indiDigits[2] = newTime[3];
          anodeStates[2] = 1; break;
        case 14:
          anodeStates[2] = 0;
          indiDigits[3] = newTime[3];
          anodeStates[3] = 1; break;
        case 15:
          indiDigits[0] = newTime[2];
          anodeStates[0] = 1; break;
        case 16:
          anodeStates[0] = 0;
          indiDigits[1] = newTime[2];
          anodeStates[1] = 1; break;
        case 17:
          anodeStates[1] = 0;
          indiDigits[2] = newTime[2];
          anodeStates[2] = 1; break;
        case 18:
          indiDigits[0] = newTime[1];
          anodeStates[0] = 1; break;
        case 19:
          anodeStates[0] = 0;
          indiDigits[1] = newTime[1];
          anodeStates[1] = 1; break;
        case 20:
          indiDigits[0] = newTime[0];
          anodeStates[0] = 1; break;
        case 21:
          flipInit = false;
          newTimeFlag = false;
      }
    }
  }

// --- игровой автомат (однорукий бандит) --- //
  else if (FLIP_EFFECT == 6) {
    if (!flipInit) {
      flipInit = true;
      // крутятся ВСЕ 4 лампы всегда, даже если цифра на какой-то из них не
      // меняется - как у настоящего игрового автомата крутятся все барабаны
      // сразу, а не только те, что должны показать новую цифру. Считаем для
      // каждой лампы, сколько шагов ей крутиться: расстояние до нужной
      // цифры (10, если цифра не меняется - тогда это будет ровно один
      // полный оборот) + несколько дополнительных полных оборотов (10 шагов
      // на оборот), причём каждая следующая лампа получает на 1 оборот
      // больше предыдущей - поэтому лампы останавливаются не одновременно,
      // а по очереди слева направо, как барабаны игрового автомата
      for (byte i = 0; i < 4; i++) {
        flipIndics[i] = true;
        byte dist = (indiDigits[i] + 10 - newTime[i]) % 10;
        if (dist == 0) dist = 10;
        slotSteps[i] = dist + 10 * (1 + i);
      }
    }

    if (flipTimer.isReady()) {
      byte flipCounter = 0;
      for (byte i = 0; i < 4; i++) {
        if (flipIndics[i]) {
          if (indiDigits[i] == 0) indiDigits[i] = 9;
          else indiDigits[i]--;
          slotSteps[i]--;
          if (slotSteps[i] == 0) flipIndics[i] = false;
        } else {
          flipCounter++;        // счётчик цифр, которые не надо менять
        }
      }
      if (flipCounter == 4) {   // если ни одну из 4 цифр менять не нужно
        flipInit = false;
        newTimeFlag = false;
      }
    }
  }
}
