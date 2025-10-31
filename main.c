// Оптимизация генерируемого кода под скорость (важно для Z80)
#pragma opt_code_speed

#include <string.h>

// Заголовочные файлы с константами и данными
#include "scr_addr.h"      // Адреса экранной памяти ZX Spectrum
#include "sincos.h"        // Таблицы синуса и косинуса (256 значений, фиксированная точка)
#include "wall_sprites.h"  // Спрайты текстур стен
#include "map.h"           // Карта уровня (32x32)
#include "engine.h"


//#link "engine.c"



// === УПРАВЛЕНИЕ: Sinclair Joystick (порт 0xFE) ===
typedef enum {
  KEY_FIRE  = 0b00010000,
  KEY_DOWN  = 0b00001000,
  KEY_UP    = 0b00000100,
  KEY_RIGHT = 0b00000010,
  KEY_LEFT  = 0b00000001
} key_t;

// === РАЗМЕЩЕНИЕ БУФЕРОВ В ПАМЯТИ ===
// Экранная память ZX Spectrum: пиксели и атрибуты
__at (SCREEN_BUFFER_START) char screen_buf[0x1800];        // 6144 байт пикселей
__at (ATTR_SCREEN_BUFFER_START) char attr_buf[0x300];      // 768 байт атрибутов

// Внешние буферы для рендеринга (вне экранной области)
__at (PIX_BUFFER_START) char pix_buffer[PIX_BUFFER_SIZE];              // Пиксельный буфер 128x32
__at (PIX_ATTR_BUFFER_START) char pix_attr_buffer[PIX_ATTR_BUFFER_SIZE]; // Атрибутный буфер

// Порт ввода Sinclair Joystick (биты 0–4)
__sfr __at 0xfe joystick_keys_port;

// === ГЛОБАЛЬНОЕ СОСТОЯНИЕ ИГРОКА ===
static unsigned int player_angle = 0;     // Угол взгляда (0–255 = полный круг)
static int player_x = 2 * 256;            // Позиция игрока в фиксированной точке (8.8): 2.0
static int player_y = 2 * 256;
static unsigned char key;                 // Текущее нажатие клавиш

// === ГЛАВНАЯ ФУНКЦИЯ ===
int main() {
  static int px, py; // Временные координаты для проверки коллизий

  engine_init();
  engine_render(player_x, player_y, player_angle);
  
  // Основной игровой цикл
  while(1) {
    // Чтение состояния клавиш: инверсия, т.к. нажатие = 0 на ZX
    key = joystick_keys_port & 0x1f ^ 0x1f;

    // === УПРАВЛЕНИЕ ===
    if (key == KEY_LEFT)  player_angle -= 8;   // Поворот влево
    if (key == KEY_RIGHT) player_angle += 8;   // Поворот вправо
    if (key == KEY_UP) {
      // Движение вперёд: новая позиция = текущая + вектор направления
      px = player_x + COS(player_angle);
      py = player_y + SIN(player_angle);
    }
    if (key == KEY_DOWN) {
      // Движение назад
      px = player_x - COS(player_angle);
      py = player_y - SIN(player_angle);
    }

    // Проверка коллизии: если новая позиция — не стена, обновляем позицию
    if (get_map_at(px, py) == 0) {
      player_x = px;
      player_y = py;
    }

    // Нормализация угла в диапазон [0, 255]
    player_angle = player_angle & 0x00ff;

    if (key != 0x00) {
      engine_render(player_x, player_y, player_angle);
    }
  }
  return 0;
}

