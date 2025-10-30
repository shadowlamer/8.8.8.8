// Оптимизация генерируемого кода под скорость (важно для Z80)
#pragma opt_code_speed

#include <string.h>

// Заголовочные файлы с константами и данными
#include "scr_addr.h"      // Адреса экранной памяти ZX Spectrum
#include "sincos.h"        // Таблицы синуса и косинуса (256 значений, фиксированная точка)
#include "wall_sprites.h"  // Спрайты текстур стен
#include "map.h"           // Карта уровня (32x32)

// === ПАРАМЕТРЫ РЕНДЕРА ===
#define PIX_BUFFER_HEIGHT 128                     // Высота буфера пикселей (половина экрана вверх/вниз)
#define PIX_BUFFER_SIZE (PIX_BUFFER_HEIGHT * SCR_WIDTH)  // Общий размер буфера (128 * 32 = 4096 байт)
#define PIX_BUFFER_START (0xf000 - PIX_BUFFER_SIZE)      // Расположение буфера в верхней памяти (~0xE000)

#define PIX_ATTR_BUFFER_HEIGHT (PIX_BUFFER_HEIGHT / 8)   // Атрибуты: 1 атрибут = 8 строк
#define PIX_ATTR_BUFFER_SIZE (SCR_WIDTH * PIX_ATTR_BUFFER_HEIGHT)  // Размер буфера атрибутов
#define PIX_ATTR_BUFFER_START (PIX_BUFFER_START - PIX_ATTR_BUFFER_SIZE)  // Расположение атрибутов перед пиксельным буфером

#define MAX_DISTANCE 32                // Максимальная дистанция прорисовки (в шагах луча)
#define INIT_WALL_HEIGHT (127 << 8)    // Начальная высота стены в фиксированной точке (8.8): 127 * 256
#define MAX_PROJECTION_HEIGHT (PIX_BUFFER_HEIGHT / 2)  // Макс. высота проекции на экран = 64 пикселя

#define NUM_WALL_COLORS 6              // Количество текстур стен (на будущее; сейчас не используется)

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

// === ВСПОМОГАТЕЛЬНЫЕ МАССИВЫ ===
static int distance_deltas[MAX_DISTANCE];        // Разности высот стен между соседними дистанциями
static unsigned char wall_height_buffer[SCR_WIDTH]; // Высота стены для каждого столбца экрана (0–31)
static unsigned char old_wall_height_buffer[SCR_WIDTH]; // Высота стены для каждого столбца экрана (0–31)

// === ПРОТОТИПЫ ФУНКЦИЙ ===
void copy_pix_buf();                          // Копирует off-screen буфер на экран
void draw_wall_sprite(unsigned char x, unsigned char height, unsigned char old_height); // Рисует текстуру стены в буфере
void fill_wall_sprite(unsigned char x, unsigned char height); // (Не используется) Рисует шаблонную стену
unsigned char trace_ray(int angle);           // Пробрасывает луч и возвращает высоту стены
char get_map_at(unsigned int x, unsigned int y); // Получает значение карты по координатам
void pixel(unsigned char x, unsigned char y); // Устанавливает пиксель (не используется в основном цикле)
void calc_distance_deltas();                  // Предвычисляет таблицу высот по дистанции

// === ГЛАВНАЯ ФУНКЦИЯ ===
int main() {
  static int px, py; // Временные координаты для проверки коллизий

  // Переменные для сглаживания краёв стен
  unsigned char wall_chunk_start;
  unsigned char wall_chunk_height;
  unsigned char wall_chunk_size;
  unsigned char wall_height_delta;

  // Предвычисление таблицы высот в зависимости от дистанции
  calc_distance_deltas();

  // Очистка буфера: верх — чёрный (0x00), низ — белый (0xFF) → имитация пола и потолка
  memset(pix_buffer, 0x00, PIX_BUFFER_SIZE / 2);               // Верх: 0–63 строки
  memset(pix_buffer + PIX_BUFFER_SIZE / 2, 0xff, PIX_BUFFER_SIZE / 2); // Низ: 64–127 строки
  
  
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

    // === ПРОБРОС ЛУЧЕЙ ДЛЯ КАЖДОГО СТОЛБЦА ЭКРАНА ===
    for (unsigned char col = 0; col < SCR_WIDTH; col++) {
      wall_height_buffer[col] = trace_ray(col);
    }

    // === СГЛАЖИВАНИЕ КРАЁВ СТЕН (лево → право) ===
    wall_chunk_start = 0;
    wall_chunk_height = wall_height_buffer[wall_chunk_start];
    for (unsigned char col = 0; col < SCR_WIDTH; col++) {
      // Прерываем, если достигли края или стена выше (новый объект)
      if (wall_height_buffer[col] == 0 || wall_height_buffer[col] > wall_chunk_height) break;
      // Если высота уменьшилась — сглаживаем линейным градиентом
      if (wall_height_buffer[col] < wall_chunk_height) {
        wall_chunk_size = col - wall_chunk_start;
        // Избегаем деления на ноль; если chunk_size == 0, delta = 0
        wall_height_delta = wall_chunk_size ? (wall_chunk_height - wall_height_buffer[col]) / wall_chunk_size : 0;
        for (unsigned char i = 0; i < wall_chunk_size; i++) {
          wall_height_buffer[wall_chunk_start + i] = wall_chunk_height - wall_height_delta * i;
        }
        // Начинаем новый "чанк" с текущей колонки
        wall_chunk_start = col;
        wall_chunk_height = wall_height_buffer[col];
      }
    }

    // === СГЛАЖИВАНИЕ КРАЁВ СТЕН (право → лево) ===
    wall_chunk_start = SCR_WIDTH - 1;
    wall_chunk_height = wall_height_buffer[wall_chunk_start];
    for (unsigned char col = SCR_WIDTH - 1; col > 0; col--) {
      if (wall_height_buffer[col] == 0 || wall_height_buffer[col] > wall_chunk_height) break;
      if (wall_height_buffer[col] < wall_chunk_height) {
        wall_chunk_size = wall_chunk_start - col;
        wall_height_delta = wall_chunk_size ? (wall_chunk_height - wall_height_buffer[col]) / wall_chunk_size : 0;
        for (unsigned char i = 0; i < wall_chunk_size; i++) {
          wall_height_buffer[wall_chunk_start - i] = wall_chunk_height - wall_height_delta * i;
        }
        wall_chunk_start = col;
        wall_chunk_height = wall_height_buffer[col];
      }
    }

    // === РЕНДЕРИНГ СТЕН В ОФФСКРИН-БУФЕР ===
    for (unsigned char col = 0; col < SCR_WIDTH; col++) {
      if (wall_height_buffer[col] > 64) wall_height_buffer[col] = 64;
      if (wall_height_buffer[col] != old_wall_height_buffer[col]) {
        draw_wall_sprite(col, wall_height_buffer[col], old_wall_height_buffer[col]);
        old_wall_height_buffer[col] = wall_height_buffer[col];
      }
    }

    // === КОПИРОВАНИЕ БУФЕРА НА ЭКРАН ===
    copy_pix_buf();
  }
  return 0;
}

// === КОПИРОВАНИЕ ОФФСКРИН-БУФЕРА НА ЭКРАН ===
void copy_pix_buf() {
  char *p_buf = pix_buffer;

  // Копируем каждую строку буфера в соответствующую строку экрана
  for (unsigned char i = 0; i < PIX_BUFFER_HEIGHT; i++) {
    memcpy(screen_line_addrs[i], p_buf, SCR_WIDTH);
    p_buf += SCR_WIDTH;
  }

}

// === РИСОВАНИЕ ТЕКСТУРЫ СТЕНЫ ===
void draw_wall_sprite(unsigned char x, unsigned char height, unsigned char old_height) {
  unsigned char y, old_y;
  char *p_buf;
  char const *p_sprite;

  // Выбор текстуры в зависимости от высоты стены
  if (height <= 4) {
    p_sprite = corn_0;  // 8 пикселей
  } else if (height <= 8) {
    p_sprite = corn_1;  // 16
  } else if (height <= 16) {
    p_sprite = corn_2;  // 32
  } else if (height <= 32) {
    p_sprite = corn_3;  // 64
  } else if (height < MAX_PROJECTION_HEIGHT) {
    p_sprite = corn_4;  // 128 (но используется частично)
  } else {
    height = MAX_PROJECTION_HEIGHT - 1; // Ограничение до 63
    p_sprite = corn_5;  // Полная 128-пиксельная текстура
  }

  // Вертикальная позиция: центрирование относительно середины буфера (64)
  y = (PIX_BUFFER_HEIGHT / 2) - height;
  old_y = (PIX_BUFFER_HEIGHT / 2) - old_height;
  
  if (y > old_y) {
    p_buf = pix_buffer + ((SCR_WIDTH * old_y) + x);
    for (unsigned char i = 0; i < (y - old_y); i++) {
      *p_buf = 0x00;
      p_buf += SCR_WIDTH;   // Переход на следующую строку (внутри столбца)
    }
  } else {
    p_buf = pix_buffer + ((SCR_WIDTH * y) + x);
  }
    
  // Запись текстуры: каждый байт — одна строка (8 пикселей в ширину, но используется 1 пиксель = 1 бит)
  // Цикл: height * 2 — потому что текстуры хранятся как 128-байтные массивы даже для меньших высот
  for (unsigned char i = 0; i < (height * 2); i++) {
    *p_buf = *p_sprite;
    p_buf += SCR_WIDTH;   // Переход на следующую строку (внутри столбца)
    p_sprite++;
  }
  
  if (y > old_y) {
    for (unsigned char i = 0; i < (y - old_y); i++) {
      *p_buf = 0xff;
      p_buf += SCR_WIDTH;   // Переход на следующую строку (внутри столбца)
    }
  }

}

// === ЗАПОЛНЕНИЕ СТЕНЫ ШАБЛОНОМ (НЕ ИСПОЛЬЗУЕТСЯ) ===
void fill_wall_sprite(unsigned char x, unsigned char height) {
  char pattern = 0b01010101;
  unsigned char y = (PIX_BUFFER_HEIGHT / 2) - height;
  char *p_buf = pix_buffer + ((SCR_WIDTH * y) + x);

  if (y % 2) pattern = ~pattern;

  for (unsigned char i = 0; i < (height * 2); i++) {
    *p_buf = pattern;
    p_buf += SCR_WIDTH;
    pattern = ~pattern;
  }
}

// === ПРОБРОС ЛУЧА ===
unsigned char trace_ray(int angle) {
  // Вычисление абсолютного угла луча с учётом направления взгляда и смещения по экрану
  int eff_angle = (angle + player_angle - (SCR_WIDTH / 2)) & 0xff;
  int ray = INIT_WALL_HEIGHT;  // Начальная "высота" луча (в фиксированной точке)
  int sin = SIN(eff_angle);
  int cos = COS(eff_angle);
  int x = player_x;
  int y = player_y;

  const int *p_delta = distance_deltas;

  // Пошаговое продвижение луча
  for (unsigned char d = 0; d < MAX_DISTANCE && get_map_at(x, y) == 0; d++) {
    // Если "высота" луча упала ниже порога — стена слишком далеко (невидима)
    if (ray < *p_delta) return 0;
    ray -= *p_delta;  // Уменьшаем высоту на дельту для текущей дистанции
    x += cos;         // Продвигаемся по X
    y += sin;         // Продвигаемся по Y
    p_delta++;
  }

  // Преобразуем высоту из фиксированной точки в пиксели
  ray = ray >> 8;
  return (unsigned char)ray;
}

// === ПОЛУЧЕНИЕ ЗНАЧЕНИЯ КАРТЫ ===
char get_map_at(unsigned int x, unsigned int y) {
  // Преобразуем фиксированные координаты (8.8) в индексы карты (целые)
  unsigned char mapX = (x >> 8) & 0xff;
  unsigned char mapY = (y >> 8) & 0xff;
  return map[mapY][mapX];
}

// === УСТАНОВКА ОТДЕЛЬНОГО ПИКСЕЛЯ (НЕ ИСПОЛЬЗУЕТСЯ В ОСНОВНОМ ЦИКЛЕ) ===
void pixel(unsigned char x, unsigned char y) {
  unsigned char coarse_x = x / 8;   // Байт в строке
  unsigned char fine_x = x % 8;     // Бит внутри байта
  // Устанавливаем бит (ZX Spectrum: старший бит = левый пиксель)
  pix_buffer[y * SCR_WIDTH + coarse_x] |= 0x80 >> fine_x;
}

// === ПРЕДВЫЧИСЛЕНИЕ ТАБЛИЦЫ ВЫСОТ ПО ДИСТАНЦИИ ===
void calc_distance_deltas() {
  unsigned int old_height = INIT_WALL_HEIGHT;
  // Для каждой дистанции вычисляем, насколько уменьшается высота по сравнению с предыдущей
  for (unsigned int i = 1; i < MAX_DISTANCE - 1; i++) {
    unsigned int calculated_height = INIT_WALL_HEIGHT / i;
    distance_deltas[i] = old_height - calculated_height;
    old_height = calculated_height;
  }
  distance_deltas[0] = 0;                     // Нулевая дистанция — нет изменения
  distance_deltas[MAX_DISTANCE - 1] = 10000;  // Последнее значение — большое число (стена "бесконечно" далеко)
}
