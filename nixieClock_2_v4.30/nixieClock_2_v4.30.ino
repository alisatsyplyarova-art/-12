#include <GyverButton.h>
#include <Wire.h>
#include <RTClib.h>
#include <EEPROM.h>

#define FW_VERSION_MAJOR 4
#define FW_VERSION_MINOR 30



// ========================== НАСТРОЙКИ ==========================
// Тип платы и распиновка индикаторов.
// 0 - IN-12 turned (индикаторы стоят правильно)
// 1 - IN-12 (индикаторы перевёрнуты)
// 2 - IN-14 (обычная распиновка)
// 3 - пользовательская распиновка (текущая плата проекта)
#define BOARD_TYPE 3

// Скважность ШИМ генератора высокого напряжения.
// От этого значения зависит напряжение на анодах ламп.
// Для конкретной платы значение подбирается экспериментально.
#define DUTY 170

// ======================== МОДУЛЬ RTC ========================
// Выберите установленную микросхему часов реального времени:
// 0 - DS3231 (по умолчанию)
// 1 - DS1307
// 2 - PCF8563
// 3 - PCF8523
#define RTC_MODULE 0

// ================= СИНХРОНИЗАЦИЯ ВРЕМЕНИ =================
// При новой прошивке часы автоматически устанавливаются по времени
// компиляции скетча. При обычном включении питания время RTC не меняется.
// TIME_SYNC_OFFSET компенсирует задержку между компиляцией и запуском.
#define TIME_SYNC_OFFSET 10    // поправка после прошивки, секунд
#define SYNC_YEAR 2026         // год для автоматической синхронизации
#define COMPILE_STAMP_ADDR 20  // адрес EEPROM для отметки времени компиляции

// ================ РЕЖИМ УПРАВЛЕНИЯ =================
// 0 - упрощённый режим
// 1 - полный режим
// Режим можно переключить удержанием кнопки выбора при включении часов.
#define SIMPLIFIED_MODE_ADDR 11     // адрес EEPROM для хранения режима
#define CONTROL_MODE_INFO_TIME 1200 // время показа подтверждения режима, мс

// ============ АВТОВЫХОД ИЗ МЕНЮ БЕЗ СОХРАНЕНИЯ ============
// Если кнопки не нажимать указанное время, изменения отменяются.
#define BRIGHT_TIMEOUT 60000UL      // меню яркости, мс (60 секунд)
#define SETTINGS_TIMEOUT 120000UL   // настройка времени/даты, мс (2 минуты)

// ======================== ЭФФЕКТЫ ========================
// Начальный эффект перелистывания цифр.
// 0 - без эффекта
// 1 - плавное угасание
// 2 - перемотка по порядку числа
// 3 - перемотка по катодам
// 4 - поезд
// 5 - резинка
// 6 - однорукий бандит
byte FLIP_EFFECT = 1;

// ======================== ЯРКОСТЬ ========================
#define NIGHT_LIGHT 1       // 1 - менять яркость по времени суток, 0 - не менять
#define NIGHT_START 22      // начало ночного режима, час
#define NIGHT_END 7         // конец ночного режима, час

// Яркость цифр. Диапазон 1-24.
// На больших значениях может усиливаться свечение соседних цифр.
#define INDI_BRIGHT 22      // дневная яркость цифр
#define INDI_BRIGHT_N 22    // ночная яркость цифр

// Яркость разделительной точки. Диапазон 1-255.
#define DOT_BRIGHT 35       // дневная яркость точки
#define DOT_BRIGHT_N 15     // ночная яркость точки

// Яркость подсветки. Диапазон 0-255.
#define BACKL_BRIGHT 250    // дневная максимальная яркость
#define BACKL_BRIGHT_N 50   // ночная максимальная яркость, 0 - подсветка выключена
#define BACKL_MIN_BRIGHT 20 // минимальная яркость в режиме дыхания
#define BACKL_PAUSE 400     // пауза темноты между вспышками, мс

// ==================== ПРЕСЕТЫ ЯРКОСТИ ====================
// Яркость цифр: 5 пресетов в рабочем диапазоне от INDI_BRIGHT.
#define INDI_PRESET_MIN (byte)(INDI_BRIGHT - (INDI_BRIGHT * 50L / 100))
const byte INDI_PRESETS[5] = {
  INDI_PRESET_MIN,
  (byte)(INDI_PRESET_MIN + (INDI_BRIGHT - INDI_PRESET_MIN) * 1 / 4),
  (byte)(INDI_PRESET_MIN + (INDI_BRIGHT - INDI_PRESET_MIN) * 2 / 4),
  (byte)(INDI_PRESET_MIN + (INDI_BRIGHT - INDI_PRESET_MIN) * 3 / 4),
  INDI_BRIGHT
};

// Яркость подсветки: 5 пресетов от 0 до BACKL_BRIGHT.
const byte BACKL_PRESETS[5] = {
  0,
  (byte)(BACKL_BRIGHT / 4),
  (byte)(BACKL_BRIGHT / 2),
  (byte)(BACKL_BRIGHT * 3 / 4),
  BACKL_BRIGHT
};

// Яркость точки: 5 пресетов от 0 до DOT_BRIGHT.
const byte DOT_PRESETS[5] = {
  0,
  (byte)(DOT_BRIGHT / 4),
  (byte)(DOT_BRIGHT / 2),
  (byte)(DOT_BRIGHT * 3 / 4),
  DOT_BRIGHT
};

// Ночные пресеты яркости.
#define INDI_PRESET_N_MIN 1
const byte INDI_PRESETS_N[5] = {
  INDI_PRESET_N_MIN,
  (byte)(INDI_PRESET_N_MIN + (INDI_BRIGHT_N - INDI_PRESET_N_MIN) * 1 / 4),
  (byte)(INDI_PRESET_N_MIN + (INDI_BRIGHT_N - INDI_PRESET_N_MIN) * 2 / 4),
  (byte)(INDI_PRESET_N_MIN + (INDI_BRIGHT_N - INDI_PRESET_N_MIN) * 3 / 4),
  INDI_BRIGHT_N
};
const byte BACKL_PRESETS_N[5] = {
  0,
  (byte)(BACKL_BRIGHT_N / 4),
  (byte)(BACKL_BRIGHT_N / 2),
  (byte)(BACKL_BRIGHT_N * 3 / 4),
  BACKL_BRIGHT_N
};
const byte DOT_PRESETS_N[5] = {
  0,
  (byte)(DOT_BRIGHT_N / 4),
  (byte)(DOT_BRIGHT_N / 2),
  (byte)(DOT_BRIGHT_N * 3 / 4),
  DOT_BRIGHT_N
};

// ======================== ГЛЮКИ ========================
// Интервал случайного эффекта «глюк» в секундах.
#define GLITCH_MIN 30
#define GLITCH_MAX 120

// ========================= ДАТА =========================
// Автоматический показ даты в обычном режиме часов.
#define DATE_SHOW_ENABLE 1     // 1 - включить, 0 - выключить
#define DATE_SHOW_INTERVAL 300 // интервал между показами, секунд
#define DATE_SHOW_DURATION 3   // длительность показа даты, секунд
// Показ даты: ДД.ММ.
// Показ можно отключить без перепрошивки: в настройке даты установить месяц 00.

// ======================== МИГАНИЕ ========================
#define DOT_TIME 500       // период мигания точки, мс
#define DOT_TIMER 20       // шаг изменения яркости точки, мс
#define BACKL_STEP 2       // шаг изменения яркости подсветки
#define BACKL_TIME 5000    // период эффекта подсветки, мс

// =================== АНТИОТРАВЛЕНИЕ =====================
#define BURN_TIME 10       // период обхода индикаторов, мс
#define BURN_LOOPS 3       // количество циклов очистки
#define BURN_PERIOD 15     // период антиотравления, минут

// ================= ТЕСТ ИНДИКАТОРОВ ====================
// При включении с зажатой кнопкой BTN2 выполняется тест ламп.
#define FW_VERSION_SHOW_TIME 1000 // время показа версии перед тестом, мс
#define LAMPTEST_ALL_DELAY 1000   // задержка при проверке всех ламп, мс
#define LAMPTEST_ONE_DELAY 400    // задержка при проверке каждой лампы, мс
#define RESET_COUNTDOWN_DELAY 500 // задержка обратного отсчёта сброса, мс

// ================ ИНДИКАЦИЯ РЕЖИМА ====================
#define MODE_INFO_TIME 1500       // время показа номера выбранного режима, мс

// ================= ВНУТРЕННИЕ НАСТРОЙКИ =================
// Эти параметры обычно менять не требуется.
byte BACKL_MODE = 0;             // режим подсветки при запуске
byte DOT_MODE = 0;               // 0 - мигает, 1 - постоянно, 2 - выключена
byte indiPresetIndex = 4;        // дневной пресет ламп, 0-4
byte backlPresetIndex = 4;       // дневной пресет подсветки, 0-4
byte dotPresetIndex = 4;         // дневной пресет точки, 0-4
byte indiPresetIndexN = 4;       // ночной пресет ламп, 0-4
byte backlPresetIndexN = 4;      // ночной пресет подсветки, 0-4
byte dotPresetIndexN = 4;        // ночной пресет точки, 0-4
byte brightSubMode = 0;          // текущая категория меню яркости
byte FLIP_SPEED[] = {0, 130, 50, 40, 70, 70, 90}; // скорость эффектов, мс
byte FLIP_EFFECT_NUM = sizeof(FLIP_SPEED);         // количество эффектов
boolean GLITCH_ALLOWED = 1;      // 1 - глюки включены, 0 - выключены

// ======================== БУДИЛЬНИК =====================
#define ALM_TIMEOUT 30      // таймаут будильника
#define FREQ 900            // частота звука будильника, Гц

// ========================= ПИНЫ =========================
#define PIEZO 2   // пищалка
#define KEY0 3    // управление часами
#define KEY1 4    // управление часами
#define KEY2 5    // управление минутами
#define KEY3 6    // управление минутами
#define BTN1 7    // кнопка выбора
#define BTN2 8    // кнопка «-» / тест
#define GEN 9     // генератор высокого напряжения
#define DOT 10    // разделительная точка
#define BACKL 11  // подсветка
#define BTN3 12   // кнопка «+» / сброс

// Пины управления дешифратором.
#define DECODER0 A0
#define DECODER1 A1
#define DECODER2 A2
#define DECODER3 A3

#if (BOARD_TYPE == 0)
const byte digitMask[] = {7, 3, 6, 4, 1, 9, 8, 0, 5, 2};   // маска цифр для платы IN-12 turned
const byte opts[] = {KEY0, KEY1, KEY2, KEY3};                // порядок индикаторов                // порядок индикаторов слева направо
const byte cathodeMask[] = {1, 6, 2, 7, 5, 0, 4, 9, 8, 3}; // порядок катодов IN-12

#elif (BOARD_TYPE == 1)
const byte digitMask[] = {2, 8, 1, 9, 6, 4, 3, 5, 0, 7};   // маска цифр для платы IN-12
const byte opts[] = {KEY3, KEY2, KEY1, KEY0};              // порядок индикаторов справа налево
const byte cathodeMask[] = {1, 6, 2, 7, 5, 0, 4, 9, 8, 3};

#elif (BOARD_TYPE == 2)
const byte digitMask[] = {9, 8, 0, 5, 4, 7, 3, 6, 2, 1};   // маска цифр для платы IN-14
const byte opts[] = {KEY3, KEY2, KEY1, KEY0};
const byte cathodeMask[] = {1, 0, 2, 9, 3, 8, 4, 7, 5, 6}; // порядок катодов // порядок катодов IN-14

#elif (BOARD_TYPE == 3)
// Пользовательская распиновка текущей платы. При смене разводки меняются
// только эти три массива: digitMask, opts и cathodeMask.
const byte digitMask[] = {0, 8, 3, 9, 7, 6, 4, 2, 5, 1};   // маска цифр
const byte opts[] = {KEY0, KEY1, KEY2, KEY3};
const byte cathodeMask[] = {1, 0, 2, 9, 3, 8, 4, 7, 5, 6};
#endif
