void loop() {
  if (dotTimer.isReady()) calculateTime();        // каждые 500 мс пересчёт и отправка времени
  if (newTimeFlag && curMode == 0) flipTick();    // перелистывание цифр
  dotBrightTick();                                // плавное мигание точки
  backlBrightTick();                              // плавное мигание подсветки ламп
  if (GLITCH_ALLOWED && curMode == 0) glitchTick();  // глюки (только в режиме часов)
  dateShowTick();                                 // периодический автопоказ даты
  modeInfoTick();                                 // неблокирующий показ номера режима (эффект/подсветка/точка)
  buttonsTick();                                  // кнопки
  settingsTick();                                 // настройки
}
