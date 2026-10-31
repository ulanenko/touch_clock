#include "localization.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

static const char *const s_language_names[CLOCK_LANGUAGE_COUNT] = {
    [CLOCK_LANGUAGE_ENGLISH] = "English",
    [CLOCK_LANGUAGE_POLISH] = "Polski",
    [CLOCK_LANGUAGE_UKRAINIAN] = "Українська",
    [CLOCK_LANGUAGE_RUSSIAN] = "Русский",
    [CLOCK_LANGUAGE_DUTCH] = "Nederlands",
};

static const char s_language_options[] =
    "English\n"
    "Polski\n"
    "Українська\n"
    "Русский\n"
    "Nederlands";

static const char *const s_day_short[CLOCK_LANGUAGE_COUNT][7] = {
    [CLOCK_LANGUAGE_ENGLISH] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"},
    [CLOCK_LANGUAGE_POLISH] = {"Nd", "Pon", "Wt", "Śr", "Czw", "Pt", "Sob"},
    [CLOCK_LANGUAGE_UKRAINIAN] = {"Нд", "Пн", "Вт", "Ср", "Чт", "Пт", "Сб"},
    [CLOCK_LANGUAGE_RUSSIAN] = {"Вс", "Пн", "Вт", "Ср", "Чт", "Пт", "Сб"},
    [CLOCK_LANGUAGE_DUTCH] = {"zo", "ma", "di", "wo", "do", "vr", "za"},
};

static const char *const s_text_en[L10N_COUNT] = {
    [L10N_SETTINGS] = "Settings",
    [L10N_WIFI] = "Wi-Fi",
    [L10N_WIFI_SUBTITLE] = "Networks, clock, and time zone",
    [L10N_NIGHT_MODE] = "Night mode",
    [L10N_NIGHT_SUBTITLE] = "Schedule, face, and brightness",
    [L10N_OTHER] = "Other",
    [L10N_OTHER_SUBTITLE] = "Language, sound, and device info",
    [L10N_LANGUAGE] = "Language",
    [L10N_TIME_ZONE] = "Time zone",
    [L10N_CLOCK] = "Clock",
    [L10N_AUTO_SYNC] = "Auto sync",
    [L10N_MANUAL_TIME] = "Manual time",
    [L10N_SET_TIME] = "Set time",
    [L10N_NETWORKS] = "Networks",
    [L10N_NO_NETWORKS] = "No networks yet",
    [L10N_SHOWING_NETWORKS] = "Showing %u of %u networks",
    [L10N_SYNC_TIME] = "Sync time",
    [L10N_SAVED_NONE] = "Saved: none",
    [L10N_JOIN_NETWORK] = "Join network",
    [L10N_PASSWORD] = "Password",
    [L10N_CANCEL] = "Cancel",
    [L10N_CONNECT] = "Connect",
    [L10N_ENABLED] = "Enabled",
    [L10N_SCHEDULE] = "Schedule",
    [L10N_NIGHT_FACE] = "Night face",
    [L10N_BRIGHTNESS] = "Brightness",
    [L10N_MAIN_SLIDER_MATCH] = "Matches the main slider interaction",
    [L10N_SUNRISE_RAMP] = "Sunrise ramp",
    [L10N_SUNRISE_RAMP_SUBTITLE] = "Brighten during the pre-alarm sunrise",
    [L10N_NIGHT_DISABLED_HINT] = "Turn on Night mode to edit schedule, face, brightness, and sunrise ramp.",
    [L10N_NIGHT_SCHEDULE] = "Night schedule",
    [L10N_DONE] = "Done",
    [L10N_TOUCH_SOUND] = "Touch sound",
    [L10N_SOFTWARE_UPDATE] = "Software update",
    [L10N_PROGRESS] = "Progress",
    [L10N_MANIFEST_NOT_CONFIGURED] = "Manifest: not configured",
    [L10N_CHECK] = "Check",
    [L10N_INSTALL] = "Install",
    [L10N_UPDATING] = "Updating",
    [L10N_ABOUT_DEVICE] = "About this device",
    [L10N_DEVICE_ID] = "Device ID",
    [L10N_VERSION] = "Version",
    [L10N_BUILD] = "Build",
    [L10N_TARGET] = "Target",
    [L10N_ESP_IDF] = "ESP-IDF",
    [L10N_ALARMS] = "Alarms",
    [L10N_EDIT_ALARM] = "Edit alarm",
    [L10N_ALARM] = "Alarm",
    [L10N_SNOOZE_DURATION] = "Snooze duration",
    [L10N_SNOOZE] = "Snooze",
    [L10N_MINUTE_ABBR] = "min",
    [L10N_VOLUME] = "Volume",
    [L10N_ALARM_SOUND] = "Alarm sound",
    [L10N_ASCENDING_ALARM] = "Ascending alarm",
    [L10N_RAMP_UP] = "Ramp up",
    [L10N_MATH_TO_TURN_OFF] = "Math to turn off",
    [L10N_WRONG_ANSWER] = "Wrong answer. Try again.",
    [L10N_DELETE] = "Delete",
    [L10N_SAVE] = "Save",
    [L10N_OFF] = "Off",
    [L10N_ONCE] = "Once",
    [L10N_EVERY_DAY] = "Every day",
    [L10N_WEEKDAYS] = "Weekdays",
    [L10N_WEEKENDS] = "Weekends",
    [L10N_NO_ALARMS_SCHEDULED] = "No alarms scheduled",
    [L10N_ALARM_RINGING_NOW] = "Alarm is ringing now",
    [L10N_SNOOZED_UNTIL] = "Snoozed until",
    [L10N_NEXT_UP] = "Next up",
    [L10N_RESTORED] = "Restored",
    [L10N_CANCELLED] = "Cancelled",
    [L10N_WAKING_AGAIN_IN] = "Waking up again in",
    [L10N_WAKING_IN] = "Waking you up in",
    [L10N_THEMES] = "Themes",
    [L10N_FACE_THEMES] = "Face themes",
    [L10N_THEME_1] = "Theme 1",
    [L10N_THEME_2] = "Theme 2",
    [L10N_THEME_3] = "Theme 3",
};

static const char *const s_text_pl[L10N_COUNT] = {
    [L10N_SETTINGS] = "Ustawienia",
    [L10N_WIFI] = "Wi-Fi",
    [L10N_WIFI_SUBTITLE] = "Sieci, zegar i strefa czasu",
    [L10N_NIGHT_MODE] = "Tryb nocny",
    [L10N_NIGHT_SUBTITLE] = "Harmonogram, tarcza i jasność",
    [L10N_OTHER] = "Inne",
    [L10N_OTHER_SUBTITLE] = "Język, dźwięk i informacje",
    [L10N_LANGUAGE] = "Język",
    [L10N_TIME_ZONE] = "Strefa czasu",
    [L10N_CLOCK] = "Zegar",
    [L10N_AUTO_SYNC] = "Auto synchronizacja",
    [L10N_MANUAL_TIME] = "Czas ręczny",
    [L10N_SET_TIME] = "Ustaw czas",
    [L10N_NETWORKS] = "Sieci",
    [L10N_NO_NETWORKS] = "Brak sieci",
    [L10N_SHOWING_NETWORKS] = "Pokazano %u z %u sieci",
    [L10N_SYNC_TIME] = "Synchronizuj czas",
    [L10N_SAVED_NONE] = "Zapisane: brak",
    [L10N_JOIN_NETWORK] = "Dołącz do sieci",
    [L10N_PASSWORD] = "Hasło",
    [L10N_CANCEL] = "Anuluj",
    [L10N_CONNECT] = "Połącz",
    [L10N_ENABLED] = "Włączone",
    [L10N_SCHEDULE] = "Harmonogram",
    [L10N_NIGHT_FACE] = "Tarcza nocna",
    [L10N_BRIGHTNESS] = "Jasność",
    [L10N_MAIN_SLIDER_MATCH] = "Tak jak główny suwak",
    [L10N_SUNRISE_RAMP] = "Rozjaśnianie",
    [L10N_SUNRISE_RAMP_SUBTITLE] = "Rozjaśnia przed alarmem",
    [L10N_NIGHT_DISABLED_HINT] = "Włącz tryb nocny, aby edytować harmonogram, tarczę, jasność i rozjaśnianie.",
    [L10N_NIGHT_SCHEDULE] = "Harmonogram nocny",
    [L10N_DONE] = "Gotowe",
    [L10N_TOUCH_SOUND] = "Dźwięk dotyku",
    [L10N_SOFTWARE_UPDATE] = "Aktualizacja",
    [L10N_PROGRESS] = "Postęp",
    [L10N_MANIFEST_NOT_CONFIGURED] = "Manifest: brak konfiguracji",
    [L10N_CHECK] = "Sprawdź",
    [L10N_INSTALL] = "Instaluj",
    [L10N_UPDATING] = "Aktualizacja",
    [L10N_ABOUT_DEVICE] = "O urządzeniu",
    [L10N_DEVICE_ID] = "ID urządzenia",
    [L10N_VERSION] = "Wersja",
    [L10N_BUILD] = "Build",
    [L10N_TARGET] = "Target",
    [L10N_ESP_IDF] = "ESP-IDF",
    [L10N_ALARMS] = "Alarmy",
    [L10N_EDIT_ALARM] = "Edytuj alarm",
    [L10N_ALARM] = "Alarm",
    [L10N_SNOOZE_DURATION] = "Czas drzemki",
    [L10N_SNOOZE] = "Drzemka",
    [L10N_MINUTE_ABBR] = "min",
    [L10N_VOLUME] = "Głośność",
    [L10N_ALARM_SOUND] = "Dźwięk alarmu",
    [L10N_ASCENDING_ALARM] = "Narastający alarm",
    [L10N_RAMP_UP] = "Narastanie",
    [L10N_MATH_TO_TURN_OFF] = "Matematyka przy wyłączaniu",
    [L10N_WRONG_ANSWER] = "Błędna odpowiedź. Spróbuj ponownie.",
    [L10N_DELETE] = "Usuń",
    [L10N_SAVE] = "Zapisz",
    [L10N_OFF] = "Wył.",
    [L10N_ONCE] = "Raz",
    [L10N_EVERY_DAY] = "Codziennie",
    [L10N_WEEKDAYS] = "Dni robocze",
    [L10N_WEEKENDS] = "Weekendy",
    [L10N_NO_ALARMS_SCHEDULED] = "Brak alarmów",
    [L10N_ALARM_RINGING_NOW] = "Alarm teraz dzwoni",
    [L10N_SNOOZED_UNTIL] = "Drzemka do",
    [L10N_NEXT_UP] = "Następny",
    [L10N_RESTORED] = "Przywrócono",
    [L10N_CANCELLED] = "Anulowano",
    [L10N_WAKING_AGAIN_IN] = "Ponowne budzenie za",
    [L10N_WAKING_IN] = "Budzenie za",
    [L10N_THEMES] = "Motywy",
    [L10N_FACE_THEMES] = "Motywy tarczy",
    [L10N_THEME_1] = "Motyw 1",
    [L10N_THEME_2] = "Motyw 2",
    [L10N_THEME_3] = "Motyw 3",
};

static const char *const s_text_uk[L10N_COUNT] = {
    [L10N_SETTINGS] = "Налаштування",
    [L10N_WIFI] = "Wi-Fi",
    [L10N_WIFI_SUBTITLE] = "Мережі, годинник і часовий пояс",
    [L10N_NIGHT_MODE] = "Нічний режим",
    [L10N_NIGHT_SUBTITLE] = "Розклад, циферблат і яскравість",
    [L10N_OTHER] = "Інше",
    [L10N_OTHER_SUBTITLE] = "Мова, звук та інформація",
    [L10N_LANGUAGE] = "Мова",
    [L10N_TIME_ZONE] = "Часовий пояс",
    [L10N_CLOCK] = "Годинник",
    [L10N_AUTO_SYNC] = "Автосинхронізація",
    [L10N_MANUAL_TIME] = "Ручний час",
    [L10N_SET_TIME] = "Встановити час",
    [L10N_NETWORKS] = "Мережі",
    [L10N_NO_NETWORKS] = "Мереж поки немає",
    [L10N_SHOWING_NETWORKS] = "Показано %u з %u мереж",
    [L10N_SYNC_TIME] = "Синхронізувати час",
    [L10N_SAVED_NONE] = "Збережено: немає",
    [L10N_JOIN_NETWORK] = "Підключитися до мережі",
    [L10N_PASSWORD] = "Пароль",
    [L10N_CANCEL] = "Скасувати",
    [L10N_CONNECT] = "Підключити",
    [L10N_ENABLED] = "Увімкнено",
    [L10N_SCHEDULE] = "Розклад",
    [L10N_NIGHT_FACE] = "Нічний циферблат",
    [L10N_BRIGHTNESS] = "Яскравість",
    [L10N_MAIN_SLIDER_MATCH] = "Як головний повзунок",
    [L10N_SUNRISE_RAMP] = "Світанок",
    [L10N_SUNRISE_RAMP_SUBTITLE] = "Підсвічує перед будильником",
    [L10N_NIGHT_DISABLED_HINT] = "Увімкніть нічний режим, щоб редагувати розклад, циферблат, яскравість і світанок.",
    [L10N_NIGHT_SCHEDULE] = "Нічний розклад",
    [L10N_DONE] = "Готово",
    [L10N_TOUCH_SOUND] = "Звук дотику",
    [L10N_SOFTWARE_UPDATE] = "Оновлення",
    [L10N_PROGRESS] = "Прогрес",
    [L10N_MANIFEST_NOT_CONFIGURED] = "Маніфест: не налаштовано",
    [L10N_CHECK] = "Перевірити",
    [L10N_INSTALL] = "Встановити",
    [L10N_UPDATING] = "Оновлення",
    [L10N_ABOUT_DEVICE] = "Про пристрій",
    [L10N_DEVICE_ID] = "ID пристрою",
    [L10N_VERSION] = "Версія",
    [L10N_BUILD] = "Збірка",
    [L10N_TARGET] = "Ціль",
    [L10N_ESP_IDF] = "ESP-IDF",
    [L10N_ALARMS] = "Будильники",
    [L10N_EDIT_ALARM] = "Редагувати будильник",
    [L10N_ALARM] = "Будильник",
    [L10N_SNOOZE_DURATION] = "Тривалість відкладання",
    [L10N_SNOOZE] = "Відкласти",
    [L10N_MINUTE_ABBR] = "хв",
    [L10N_VOLUME] = "Гучність",
    [L10N_ALARM_SOUND] = "Звук будильника",
    [L10N_ASCENDING_ALARM] = "Наростаючий будильник",
    [L10N_RAMP_UP] = "Наростання",
    [L10N_MATH_TO_TURN_OFF] = "Математика для вимкнення",
    [L10N_WRONG_ANSWER] = "Неправильна відповідь. Спробуйте ще.",
    [L10N_DELETE] = "Видалити",
    [L10N_SAVE] = "Зберегти",
    [L10N_OFF] = "Вимк.",
    [L10N_ONCE] = "Раз",
    [L10N_EVERY_DAY] = "Щодня",
    [L10N_WEEKDAYS] = "Будні",
    [L10N_WEEKENDS] = "Вихідні",
    [L10N_NO_ALARMS_SCHEDULED] = "Немає будильників",
    [L10N_ALARM_RINGING_NOW] = "Будильник дзвонить",
    [L10N_SNOOZED_UNTIL] = "Відкладено до",
    [L10N_NEXT_UP] = "Наступний",
    [L10N_RESTORED] = "Відновлено",
    [L10N_CANCELLED] = "Скасовано",
    [L10N_WAKING_AGAIN_IN] = "Повторне будіння через",
    [L10N_WAKING_IN] = "Будіння через",
    [L10N_THEMES] = "Теми",
    [L10N_FACE_THEMES] = "Теми циферблата",
    [L10N_THEME_1] = "Тема 1",
    [L10N_THEME_2] = "Тема 2",
    [L10N_THEME_3] = "Тема 3",
};

static const char *const s_text_ru[L10N_COUNT] = {
    [L10N_SETTINGS] = "Настройки",
    [L10N_WIFI] = "Wi-Fi",
    [L10N_WIFI_SUBTITLE] = "Сети, часы и часовой пояс",
    [L10N_NIGHT_MODE] = "Ночной режим",
    [L10N_NIGHT_SUBTITLE] = "Расписание, циферблат и яркость",
    [L10N_OTHER] = "Другое",
    [L10N_OTHER_SUBTITLE] = "Язык, звук и информация",
    [L10N_LANGUAGE] = "Язык",
    [L10N_TIME_ZONE] = "Часовой пояс",
    [L10N_CLOCK] = "Часы",
    [L10N_AUTO_SYNC] = "Автосинхронизация",
    [L10N_MANUAL_TIME] = "Ручное время",
    [L10N_SET_TIME] = "Установить время",
    [L10N_NETWORKS] = "Сети",
    [L10N_NO_NETWORKS] = "Сетей пока нет",
    [L10N_SHOWING_NETWORKS] = "Показано %u из %u сетей",
    [L10N_SYNC_TIME] = "Синхронизировать",
    [L10N_SAVED_NONE] = "Сохранено: нет",
    [L10N_JOIN_NETWORK] = "Подключиться к сети",
    [L10N_PASSWORD] = "Пароль",
    [L10N_CANCEL] = "Отмена",
    [L10N_CONNECT] = "Подключить",
    [L10N_ENABLED] = "Включено",
    [L10N_SCHEDULE] = "Расписание",
    [L10N_NIGHT_FACE] = "Ночной циферблат",
    [L10N_BRIGHTNESS] = "Яркость",
    [L10N_MAIN_SLIDER_MATCH] = "Как основной ползунок",
    [L10N_SUNRISE_RAMP] = "Рассвет",
    [L10N_SUNRISE_RAMP_SUBTITLE] = "Подсветка перед будильником",
    [L10N_NIGHT_DISABLED_HINT] = "Включите ночной режим, чтобы изменить расписание, циферблат, яркость и рассвет.",
    [L10N_NIGHT_SCHEDULE] = "Ночное расписание",
    [L10N_DONE] = "Готово",
    [L10N_TOUCH_SOUND] = "Звук касания",
    [L10N_SOFTWARE_UPDATE] = "Обновление",
    [L10N_PROGRESS] = "Прогресс",
    [L10N_MANIFEST_NOT_CONFIGURED] = "Манифест: не настроен",
    [L10N_CHECK] = "Проверить",
    [L10N_INSTALL] = "Установить",
    [L10N_UPDATING] = "Обновление",
    [L10N_ABOUT_DEVICE] = "Об устройстве",
    [L10N_DEVICE_ID] = "ID устройства",
    [L10N_VERSION] = "Версия",
    [L10N_BUILD] = "Сборка",
    [L10N_TARGET] = "Цель",
    [L10N_ESP_IDF] = "ESP-IDF",
    [L10N_ALARMS] = "Будильники",
    [L10N_EDIT_ALARM] = "Изменить будильник",
    [L10N_ALARM] = "Будильник",
    [L10N_SNOOZE_DURATION] = "Длительность повтора",
    [L10N_SNOOZE] = "Отложить",
    [L10N_MINUTE_ABBR] = "мин",
    [L10N_VOLUME] = "Громкость",
    [L10N_ALARM_SOUND] = "Звук будильника",
    [L10N_ASCENDING_ALARM] = "Нарастающий будильник",
    [L10N_RAMP_UP] = "Нарастание",
    [L10N_MATH_TO_TURN_OFF] = "Математика для выключения",
    [L10N_WRONG_ANSWER] = "Неверный ответ. Попробуйте еще.",
    [L10N_DELETE] = "Удалить",
    [L10N_SAVE] = "Сохранить",
    [L10N_OFF] = "Выкл.",
    [L10N_ONCE] = "Раз",
    [L10N_EVERY_DAY] = "Каждый день",
    [L10N_WEEKDAYS] = "Будни",
    [L10N_WEEKENDS] = "Выходные",
    [L10N_NO_ALARMS_SCHEDULED] = "Будильников нет",
    [L10N_ALARM_RINGING_NOW] = "Будильник звонит",
    [L10N_SNOOZED_UNTIL] = "Отложено до",
    [L10N_NEXT_UP] = "Следующий",
    [L10N_RESTORED] = "Восстановлено",
    [L10N_CANCELLED] = "Отменено",
    [L10N_WAKING_AGAIN_IN] = "Повторный сигнал через",
    [L10N_WAKING_IN] = "Сигнал через",
    [L10N_THEMES] = "Темы",
    [L10N_FACE_THEMES] = "Темы циферблата",
    [L10N_THEME_1] = "Тема 1",
    [L10N_THEME_2] = "Тема 2",
    [L10N_THEME_3] = "Тема 3",
};

static const char *const s_text_nl[L10N_COUNT] = {
    [L10N_SETTINGS] = "Instellingen",
    [L10N_WIFI] = "Wi-Fi",
    [L10N_WIFI_SUBTITLE] = "Netwerken, klok en tijdzone",
    [L10N_NIGHT_MODE] = "Nachtmodus",
    [L10N_NIGHT_SUBTITLE] = "Schema, wijzerplaat en helderheid",
    [L10N_OTHER] = "Overig",
    [L10N_OTHER_SUBTITLE] = "Taal, geluid en apparaatinformatie",
    [L10N_LANGUAGE] = "Taal",
    [L10N_TIME_ZONE] = "Tijdzone",
    [L10N_CLOCK] = "Klok",
    [L10N_AUTO_SYNC] = "Automatisch",
    [L10N_MANUAL_TIME] = "Handmatige tijd",
    [L10N_SET_TIME] = "Tijd instellen",
    [L10N_NETWORKS] = "Netwerken",
    [L10N_NO_NETWORKS] = "Nog geen netwerken",
    [L10N_SHOWING_NETWORKS] = "%u van %u netwerken getoond",
    [L10N_SYNC_TIME] = "Tijd syncen",
    [L10N_SAVED_NONE] = "Opgeslagen: geen",
    [L10N_JOIN_NETWORK] = "Netwerk verbinden",
    [L10N_PASSWORD] = "Wachtwoord",
    [L10N_CANCEL] = "Annuleren",
    [L10N_CONNECT] = "Verbinden",
    [L10N_ENABLED] = "Ingeschakeld",
    [L10N_SCHEDULE] = "Schema",
    [L10N_NIGHT_FACE] = "Nachtwijzerplaat",
    [L10N_BRIGHTNESS] = "Helderheid",
    [L10N_MAIN_SLIDER_MATCH] = "Zelfde gedrag als hoofdschuif",
    [L10N_SUNRISE_RAMP] = "Zonsopgang",
    [L10N_SUNRISE_RAMP_SUBTITLE] = "Verheldert voor het alarm",
    [L10N_NIGHT_DISABLED_HINT] = "Zet Nachtmodus aan om schema, wijzerplaat, helderheid en zonsopgang te bewerken.",
    [L10N_NIGHT_SCHEDULE] = "Nachtschema",
    [L10N_DONE] = "Klaar",
    [L10N_TOUCH_SOUND] = "Aanraakgeluid",
    [L10N_SOFTWARE_UPDATE] = "Software-update",
    [L10N_PROGRESS] = "Voortgang",
    [L10N_MANIFEST_NOT_CONFIGURED] = "Manifest: niet geconfigureerd",
    [L10N_CHECK] = "Controleer",
    [L10N_INSTALL] = "Installeer",
    [L10N_UPDATING] = "Bijwerken",
    [L10N_ABOUT_DEVICE] = "Over dit apparaat",
    [L10N_DEVICE_ID] = "Apparaat-ID",
    [L10N_VERSION] = "Versie",
    [L10N_BUILD] = "Build",
    [L10N_TARGET] = "Target",
    [L10N_ESP_IDF] = "ESP-IDF",
    [L10N_ALARMS] = "Alarmen",
    [L10N_EDIT_ALARM] = "Alarm bewerken",
    [L10N_ALARM] = "Alarm",
    [L10N_SNOOZE_DURATION] = "Snoozeduur",
    [L10N_SNOOZE] = "Snooze",
    [L10N_MINUTE_ABBR] = "min",
    [L10N_VOLUME] = "Volume",
    [L10N_ALARM_SOUND] = "Alarmgeluid",
    [L10N_ASCENDING_ALARM] = "Oplopend alarm",
    [L10N_RAMP_UP] = "Oplopen",
    [L10N_MATH_TO_TURN_OFF] = "Som om uit te zetten",
    [L10N_WRONG_ANSWER] = "Verkeerd antwoord. Probeer opnieuw.",
    [L10N_DELETE] = "Verwijderen",
    [L10N_SAVE] = "Opslaan",
    [L10N_OFF] = "Uit",
    [L10N_ONCE] = "Eenmalig",
    [L10N_EVERY_DAY] = "Elke dag",
    [L10N_WEEKDAYS] = "Werkdagen",
    [L10N_WEEKENDS] = "Weekenden",
    [L10N_NO_ALARMS_SCHEDULED] = "Geen alarmen gepland",
    [L10N_ALARM_RINGING_NOW] = "Alarm gaat nu af",
    [L10N_SNOOZED_UNTIL] = "Gesnoozed tot",
    [L10N_NEXT_UP] = "Volgende",
    [L10N_RESTORED] = "Hersteld",
    [L10N_CANCELLED] = "Geannuleerd",
    [L10N_WAKING_AGAIN_IN] = "Opnieuw wekken over",
    [L10N_WAKING_IN] = "Wekken over",
    [L10N_THEMES] = "Thema's",
    [L10N_FACE_THEMES] = "Wijzerplaatthema's",
    [L10N_THEME_1] = "Thema 1",
    [L10N_THEME_2] = "Thema 2",
    [L10N_THEME_3] = "Thema 3",
};

static const char *const *const s_tables[CLOCK_LANGUAGE_COUNT] = {
    [CLOCK_LANGUAGE_ENGLISH] = s_text_en,
    [CLOCK_LANGUAGE_POLISH] = s_text_pl,
    [CLOCK_LANGUAGE_UKRAINIAN] = s_text_uk,
    [CLOCK_LANGUAGE_RUSSIAN] = s_text_ru,
    [CLOCK_LANGUAGE_DUTCH] = s_text_nl,
};

typedef struct {
    const char *key;
    const char *text[CLOCK_LANGUAGE_COUNT];
} l10n_status_entry_t;

static const l10n_status_entry_t s_runtime_statuses[] = {
    {
        "Time synchronized",
        {
            [CLOCK_LANGUAGE_ENGLISH] = "Time synchronized",
            [CLOCK_LANGUAGE_POLISH] = "Czas zsynchronizowany",
            [CLOCK_LANGUAGE_UKRAINIAN] = "Час синхронізовано",
            [CLOCK_LANGUAGE_RUSSIAN] = "Время синхронизировано",
            [CLOCK_LANGUAGE_DUTCH] = "Tijd gesynchroniseerd",
        },
    },
    {
        "Wi-Fi started",
        {
            [CLOCK_LANGUAGE_ENGLISH] = "Wi-Fi started",
            [CLOCK_LANGUAGE_POLISH] = "Wi-Fi uruchomione",
            [CLOCK_LANGUAGE_UKRAINIAN] = "Wi-Fi запущено",
            [CLOCK_LANGUAGE_RUSSIAN] = "Wi-Fi запущен",
            [CLOCK_LANGUAGE_DUTCH] = "Wi-Fi gestart",
        },
    },
    {
        "Reconnecting...",
        {
            [CLOCK_LANGUAGE_ENGLISH] = "Reconnecting...",
            [CLOCK_LANGUAGE_POLISH] = "Ponowne łączenie...",
            [CLOCK_LANGUAGE_UKRAINIAN] = "Повторне підключення...",
            [CLOCK_LANGUAGE_RUSSIAN] = "Повторное подключение...",
            [CLOCK_LANGUAGE_DUTCH] = "Opnieuw verbinden...",
        },
    },
    {
        "Disconnected",
        {
            [CLOCK_LANGUAGE_ENGLISH] = "Disconnected",
            [CLOCK_LANGUAGE_POLISH] = "Rozłączono",
            [CLOCK_LANGUAGE_UKRAINIAN] = "Відключено",
            [CLOCK_LANGUAGE_RUSSIAN] = "Отключено",
            [CLOCK_LANGUAGE_DUTCH] = "Verbinding verbroken",
        },
    },
    {
        "Scan failed",
        {
            [CLOCK_LANGUAGE_ENGLISH] = "Scan failed",
            [CLOCK_LANGUAGE_POLISH] = "Skanowanie nieudane",
            [CLOCK_LANGUAGE_UKRAINIAN] = "Сканування не вдалося",
            [CLOCK_LANGUAGE_RUSSIAN] = "Сканирование не удалось",
            [CLOCK_LANGUAGE_DUTCH] = "Scannen mislukt",
        },
    },
    {
        "Scan complete",
        {
            [CLOCK_LANGUAGE_ENGLISH] = "Scan complete",
            [CLOCK_LANGUAGE_POLISH] = "Skanowanie zakończone",
            [CLOCK_LANGUAGE_UKRAINIAN] = "Сканування завершено",
            [CLOCK_LANGUAGE_RUSSIAN] = "Сканирование завершено",
            [CLOCK_LANGUAGE_DUTCH] = "Scannen voltooid",
        },
    },
    {
        "Scanning...",
        {
            [CLOCK_LANGUAGE_ENGLISH] = "Scanning...",
            [CLOCK_LANGUAGE_POLISH] = "Skanowanie...",
            [CLOCK_LANGUAGE_UKRAINIAN] = "Сканування...",
            [CLOCK_LANGUAGE_RUSSIAN] = "Сканирование...",
            [CLOCK_LANGUAGE_DUTCH] = "Scannen...",
        },
    },
    {
        "Scan task failed",
        {
            [CLOCK_LANGUAGE_ENGLISH] = "Scan task failed",
            [CLOCK_LANGUAGE_POLISH] = "Zadanie skanowania nieudane",
            [CLOCK_LANGUAGE_UKRAINIAN] = "Завдання сканування не вдалося",
            [CLOCK_LANGUAGE_RUSSIAN] = "Задача сканирования не удалась",
            [CLOCK_LANGUAGE_DUTCH] = "Scantaak mislukt",
        },
    },
    {
        "Wi-Fi idle",
        {
            [CLOCK_LANGUAGE_ENGLISH] = "Wi-Fi idle",
            [CLOCK_LANGUAGE_POLISH] = "Wi-Fi bezczynne",
            [CLOCK_LANGUAGE_UKRAINIAN] = "Wi-Fi очікує",
            [CLOCK_LANGUAGE_RUSSIAN] = "Wi-Fi ожидает",
            [CLOCK_LANGUAGE_DUTCH] = "Wi-Fi inactief",
        },
    },
    {
        "Wi-Fi not configured",
        {
            [CLOCK_LANGUAGE_ENGLISH] = "Wi-Fi not configured",
            [CLOCK_LANGUAGE_POLISH] = "Wi-Fi nieskonfigurowane",
            [CLOCK_LANGUAGE_UKRAINIAN] = "Wi-Fi не налаштовано",
            [CLOCK_LANGUAGE_RUSSIAN] = "Wi-Fi не настроен",
            [CLOCK_LANGUAGE_DUTCH] = "Wi-Fi niet ingesteld",
        },
    },
    {
        "Connecting to %s",
        {
            [CLOCK_LANGUAGE_ENGLISH] = "Connecting to %s",
            [CLOCK_LANGUAGE_POLISH] = "Łączenie z %s",
            [CLOCK_LANGUAGE_UKRAINIAN] = "Підключення до %s",
            [CLOCK_LANGUAGE_RUSSIAN] = "Подключение к %s",
            [CLOCK_LANGUAGE_DUTCH] = "Verbinden met %s",
        },
    },
    {
        "Connected: %s",
        {
            [CLOCK_LANGUAGE_ENGLISH] = "Connected: %s",
            [CLOCK_LANGUAGE_POLISH] = "Połączono: %s",
            [CLOCK_LANGUAGE_UKRAINIAN] = "Підключено: %s",
            [CLOCK_LANGUAGE_RUSSIAN] = "Подключено: %s",
            [CLOCK_LANGUAGE_DUTCH] = "Verbonden: %s",
        },
    },
    {
        "Downloading...",
        {
            [CLOCK_LANGUAGE_ENGLISH] = "Downloading...",
            [CLOCK_LANGUAGE_POLISH] = "Pobieranie...",
            [CLOCK_LANGUAGE_UKRAINIAN] = "Завантаження...",
            [CLOCK_LANGUAGE_RUSSIAN] = "Загрузка...",
            [CLOCK_LANGUAGE_DUTCH] = "Downloaden...",
        },
    },
    {
        "Validating...",
        {
            [CLOCK_LANGUAGE_ENGLISH] = "Validating...",
            [CLOCK_LANGUAGE_POLISH] = "Weryfikacja...",
            [CLOCK_LANGUAGE_UKRAINIAN] = "Перевірка...",
            [CLOCK_LANGUAGE_RUSSIAN] = "Проверка...",
            [CLOCK_LANGUAGE_DUTCH] = "Controleren...",
        },
    },
    {
        "Wi-Fi unavailable",
        {
            [CLOCK_LANGUAGE_ENGLISH] = "Wi-Fi unavailable",
            [CLOCK_LANGUAGE_POLISH] = "Wi-Fi niedostępne",
            [CLOCK_LANGUAGE_UKRAINIAN] = "Wi-Fi недоступне",
            [CLOCK_LANGUAGE_RUSSIAN] = "Wi-Fi недоступен",
            [CLOCK_LANGUAGE_DUTCH] = "Wi-Fi niet beschikbaar",
        },
    },
    {
        "Manual time mode",
        {
            [CLOCK_LANGUAGE_ENGLISH] = "Manual time mode",
            [CLOCK_LANGUAGE_POLISH] = "Ręczny tryb czasu",
            [CLOCK_LANGUAGE_UKRAINIAN] = "Ручний режим часу",
            [CLOCK_LANGUAGE_RUSSIAN] = "Ручной режим времени",
            [CLOCK_LANGUAGE_DUTCH] = "Handmatige tijd",
        },
    },
    {
        "Syncing time...",
        {
            [CLOCK_LANGUAGE_ENGLISH] = "Syncing time...",
            [CLOCK_LANGUAGE_POLISH] = "Synchronizacja czasu...",
            [CLOCK_LANGUAGE_UKRAINIAN] = "Синхронізація часу...",
            [CLOCK_LANGUAGE_RUSSIAN] = "Синхронизация времени...",
            [CLOCK_LANGUAGE_DUTCH] = "Tijd synchroniseren...",
        },
    },
    {
        "Ready to check for updates",
        {
            [CLOCK_LANGUAGE_ENGLISH] = "Ready to check for updates",
            [CLOCK_LANGUAGE_POLISH] = "Gotowe do sprawdzenia aktualizacji",
            [CLOCK_LANGUAGE_UKRAINIAN] = "Готово до перевірки оновлень",
            [CLOCK_LANGUAGE_RUSSIAN] = "Готово к проверке обновлений",
            [CLOCK_LANGUAGE_DUTCH] = "Klaar om updates te controleren",
        },
    },
    {
        "OTA URL not configured",
        {
            [CLOCK_LANGUAGE_ENGLISH] = "OTA URL not configured",
            [CLOCK_LANGUAGE_POLISH] = "Adres OTA nieskonfigurowany",
            [CLOCK_LANGUAGE_UKRAINIAN] = "URL OTA не налаштовано",
            [CLOCK_LANGUAGE_RUSSIAN] = "URL OTA не настроен",
            [CLOCK_LANGUAGE_DUTCH] = "OTA-URL niet ingesteld",
        },
    },
    {
        "Checking manifest...",
        {
            [CLOCK_LANGUAGE_ENGLISH] = "Checking manifest...",
            [CLOCK_LANGUAGE_POLISH] = "Sprawdzanie manifestu...",
            [CLOCK_LANGUAGE_UKRAINIAN] = "Перевірка маніфесту...",
            [CLOCK_LANGUAGE_RUSSIAN] = "Проверка манифеста...",
            [CLOCK_LANGUAGE_DUTCH] = "Manifest controleren...",
        },
    },
    {
        "Update available",
        {
            [CLOCK_LANGUAGE_ENGLISH] = "Update available",
            [CLOCK_LANGUAGE_POLISH] = "Aktualizacja dostępna",
            [CLOCK_LANGUAGE_UKRAINIAN] = "Оновлення доступне",
            [CLOCK_LANGUAGE_RUSSIAN] = "Обновление доступно",
            [CLOCK_LANGUAGE_DUTCH] = "Update beschikbaar",
        },
    },
    {
        "Already up to date",
        {
            [CLOCK_LANGUAGE_ENGLISH] = "Already up to date",
            [CLOCK_LANGUAGE_POLISH] = "Już aktualne",
            [CLOCK_LANGUAGE_UKRAINIAN] = "Уже актуально",
            [CLOCK_LANGUAGE_RUSSIAN] = "Уже актуально",
            [CLOCK_LANGUAGE_DUTCH] = "Al bijgewerkt",
        },
    },
    {
        "Update check failed",
        {
            [CLOCK_LANGUAGE_ENGLISH] = "Update check failed",
            [CLOCK_LANGUAGE_POLISH] = "Sprawdzenie aktualizacji nieudane",
            [CLOCK_LANGUAGE_UKRAINIAN] = "Перевірка оновлення не вдалася",
            [CLOCK_LANGUAGE_RUSSIAN] = "Проверка обновления не удалась",
            [CLOCK_LANGUAGE_DUTCH] = "Updatecontrole mislukt",
        },
    },
    {
        "Manifest failed",
        {
            [CLOCK_LANGUAGE_ENGLISH] = "Manifest failed",
            [CLOCK_LANGUAGE_POLISH] = "Błąd manifestu",
            [CLOCK_LANGUAGE_UKRAINIAN] = "Помилка маніфесту",
            [CLOCK_LANGUAGE_RUSSIAN] = "Ошибка манифеста",
            [CLOCK_LANGUAGE_DUTCH] = "Manifest mislukt",
        },
    },
    {
        "No newer update",
        {
            [CLOCK_LANGUAGE_ENGLISH] = "No newer update",
            [CLOCK_LANGUAGE_POLISH] = "Brak nowszej aktualizacji",
            [CLOCK_LANGUAGE_UKRAINIAN] = "Немає новішого оновлення",
            [CLOCK_LANGUAGE_RUSSIAN] = "Нет более нового обновления",
            [CLOCK_LANGUAGE_DUTCH] = "Geen nieuwere update",
        },
    },
    {
        "OTA slots missing",
        {
            [CLOCK_LANGUAGE_ENGLISH] = "OTA slots missing",
            [CLOCK_LANGUAGE_POLISH] = "Brak slotów OTA",
            [CLOCK_LANGUAGE_UKRAINIAN] = "Немає слотів OTA",
            [CLOCK_LANGUAGE_RUSSIAN] = "Нет слотов OTA",
            [CLOCK_LANGUAGE_DUTCH] = "OTA-slots ontbreken",
        },
    },
    {
        "Update installed; rebooting",
        {
            [CLOCK_LANGUAGE_ENGLISH] = "Update installed; rebooting",
            [CLOCK_LANGUAGE_POLISH] = "Aktualizacja zainstalowana; restart",
            [CLOCK_LANGUAGE_UKRAINIAN] = "Оновлення встановлено; перезапуск",
            [CLOCK_LANGUAGE_RUSSIAN] = "Обновление установлено; перезапуск",
            [CLOCK_LANGUAGE_DUTCH] = "Update geinstalleerd; herstart",
        },
    },
    {
        "Update failed",
        {
            [CLOCK_LANGUAGE_ENGLISH] = "Update failed",
            [CLOCK_LANGUAGE_POLISH] = "Aktualizacja nieudana",
            [CLOCK_LANGUAGE_UKRAINIAN] = "Оновлення не вдалося",
            [CLOCK_LANGUAGE_RUSSIAN] = "Обновление не удалось",
            [CLOCK_LANGUAGE_DUTCH] = "Update mislukt",
        },
    },
    {
        "Starting update...",
        {
            [CLOCK_LANGUAGE_ENGLISH] = "Starting update...",
            [CLOCK_LANGUAGE_POLISH] = "Uruchamianie aktualizacji...",
            [CLOCK_LANGUAGE_UKRAINIAN] = "Запуск оновлення...",
            [CLOCK_LANGUAGE_RUSSIAN] = "Запуск обновления...",
            [CLOCK_LANGUAGE_DUTCH] = "Update starten...",
        },
    },
    {
        "Checking...",
        {
            [CLOCK_LANGUAGE_ENGLISH] = "Checking...",
            [CLOCK_LANGUAGE_POLISH] = "Sprawdzanie...",
            [CLOCK_LANGUAGE_UKRAINIAN] = "Перевірка...",
            [CLOCK_LANGUAGE_RUSSIAN] = "Проверка...",
            [CLOCK_LANGUAGE_DUTCH] = "Controleren...",
        },
    },
    {
        "Update task failed",
        {
            [CLOCK_LANGUAGE_ENGLISH] = "Update task failed",
            [CLOCK_LANGUAGE_POLISH] = "Zadanie aktualizacji nieudane",
            [CLOCK_LANGUAGE_UKRAINIAN] = "Завдання оновлення не вдалося",
            [CLOCK_LANGUAGE_RUSSIAN] = "Задача обновления не удалась",
            [CLOCK_LANGUAGE_DUTCH] = "Updatetaak mislukt",
        },
    },
};

clock_language_t l10n_sanitize_language(clock_language_t language)
{
    if (language >= CLOCK_LANGUAGE_COUNT) {
        return CLOCK_LANGUAGE_ENGLISH;
    }

    return language;
}

const char *l10n_text(clock_language_t language, l10n_text_id_t text_id)
{
    const char *text;

    language = l10n_sanitize_language(language);
    if (text_id >= L10N_COUNT) {
        return "";
    }

    text = s_tables[language][text_id];
    if (text == NULL) {
        text = s_text_en[text_id];
    }

    return text != NULL ? text : "";
}

const char *l10n_language_name(clock_language_t language)
{
    language = l10n_sanitize_language(language);
    return s_language_names[language];
}

const char *l10n_language_options(void)
{
    return s_language_options;
}

void l10n_runtime_status(clock_language_t language, const char *status, char *out, size_t size)
{
    clock_language_t safe_language = l10n_sanitize_language(language);

    if (size == 0) {
        return;
    }
    out[0] = '\0';
    if (status == NULL) {
        return;
    }
    for (size_t i = 0; i < sizeof(s_runtime_statuses) / sizeof(s_runtime_statuses[0]); ++i) {
        const l10n_status_entry_t *e = &s_runtime_statuses[i];
        const char *text = e->text[safe_language] != NULL ? e->text[safe_language] : e->key;
        const char *arg = strstr(e->key, "%s");
        size_t head;
        size_t tail;
        size_t len;

        if (arg == NULL) {
            if (strcmp(status, e->key) == 0) {
                snprintf(out, size, "%s", text);
                return;
            }
            continue;
        }
        /* "Connecting to %s": the text round the name matches, the name is kept. */
        head = (size_t)(arg - e->key);
        tail = strlen(arg + 2);
        len = strlen(status);
        if (len >= head + tail && strncmp(status, e->key, head) == 0 &&
            strcmp(status + len - tail, arg + 2) == 0) {
            char name[64];

            snprintf(name, sizeof(name), "%.*s", (int)(len - head - tail), status + head);
            snprintf(out, size, text, name);
            return;
        }
    }
    snprintf(out, size, "%s", status);
}

const char *l10n_day_short(clock_language_t language, int day)
{
    clock_language_t safe_language = l10n_sanitize_language(language);

    if (day < 0 || day >= 7) {
        return "";
    }

    return s_day_short[safe_language][day];
}
