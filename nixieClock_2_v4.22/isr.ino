// динамическая индикация в прерывании таймера 2
ISR(TIMER2_COMPA_vect) {
  // --- программный ШИМ подсветки (пин BACKL), метод накопления ошибки ---
  // BACKL физически = OC2A этого же таймера, поэтому аппаратный ШИМ на нём
  // использовать нельзя (см. пояснение в 1_setup.ino) - яркость регулируется
  // здесь вручную. Простое сравнение "фаза < скважность/4" давало только 64
  // различимых уровня (0-63) - при плавном "дыхании" это заметно ступеньками.
  // Вместо этого - накопитель (аналог Брезенхема/дельта-сигма модуляции):
  // на каждом тике прибавляем полную 8-битную скважность (0-255) и включаем
  // лампу, когда накопленная сумма "переполняет" 255. Получаем ПОЛНОЕ
  // разрешение 0-255 без ступенек и без потери частоты обновления.
  backlAccum += backlDuty;
  if (backlAccum >= 255) {
    backlAccum -= 255;
    setPin(BACKL, 1);
  } else {
    setPin(BACKL, 0);
  }

  // --- мультиплексирование индикаторов (как было) ---
  indiCounter[curIndi]++;             // счётчик индикатора
  if (indiCounter[curIndi] >= indiDimm[curIndi])  // если достигли порога диммирования
    setPin(opts[curIndi], 0);         // выключить текущий индикатор

  if (indiCounter[curIndi] > 25) {    // достигли порога в 25 единиц
    indiCounter[curIndi] = 0;         // сброс счетчика лампы
    if (++curIndi >= 4) curIndi = 0;  // смена лампы закольцованная

    // отправить цифру из массива indiDigits согласно типу лампы
    if (indiDimm[curIndi] > 0) {
      byte thisDig = digitMask[indiDigits[curIndi]];
      setPin(DECODER3, bitRead(thisDig, 0));
      setPin(DECODER1, bitRead(thisDig, 1));
      setPin(DECODER0, bitRead(thisDig, 2));
      setPin(DECODER2, bitRead(thisDig, 3));
      setPin(opts[curIndi], anodeStates[curIndi]);    // включить анод на текущую лампу
    }
  }
}
