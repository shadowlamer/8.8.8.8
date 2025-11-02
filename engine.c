#include <string.h>
#include "engine.h"

// Экранная память ZX Spectrum: пиксели и атрибуты
__at (SCREEN_BUFFER_START) char screen_buf[0x1800];        // 6144 байт пикселей
__at (ATTR_SCREEN_BUFFER_START) char attr_buf[0x300];      // 768 байт атрибутов

// Внешние буферы для рендеринга (вне экранной области)
__at (PIX_BUFFER_START) char pix_buffer[PIX_BUFFER_SIZE];              // Пиксельный буфер 128x32
__at (PIX_ATTR_BUFFER_START) char pix_attr_buffer[PIX_ATTR_BUFFER_SIZE]; // Атрибутный буфер

// === ВСПОМОГАТЕЛЬНЫЕ МАССИВЫ ===
static int distance_deltas[MAX_DISTANCE];        // Разности высот стен между соседними дистанциями
static unsigned char wall_height_buffer[SCR_WIDTH]; // Высота стены для каждого столбца экрана (0–31)
static unsigned char old_wall_height_buffer[SCR_WIDTH]; // Высота стены для каждого столбца экрана (0–31)


void engine_init() {
  // Предвычисление таблицы высот в зависимости от дистанции
  calc_distance_deltas();

  // Очистка буфера: верх — чёрный (0x00), низ — белый (0xFF) → имитация пола и потолка
  memset(pix_buffer, 0x00, PIX_BUFFER_SIZE / 2);               // Верх: 0–63 строки
  memset(pix_buffer + PIX_BUFFER_SIZE / 2, 0xff, PIX_BUFFER_SIZE / 2); // Низ: 64–127 строки
}


void engine_render(int player_x, int player_y, int player_angle) {
// === ПРОБРОС ЛУЧЕЙ ДЛЯ КАЖДОГО СТОЛБЦА ЭКРАНА ===
  // Переменные для сглаживания краёв стен
  unsigned char wall_chunk_start;
  unsigned char wall_chunk_height;
  unsigned char wall_chunk_size;
  unsigned char wall_height_delta;

  memset(attr_buf, 0x04, ATTR_SCREEN_BUFFER_SIZE);               // Верх: 0–63 строки
  
  for (unsigned char col = 0; col < SCR_WIDTH; col++) {
      wall_height_buffer[col] = trace_ray(col, player_x, player_y, player_angle);
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
      if (wall_height_buffer[col] > MAX_PROJECTION_HEIGHT) wall_height_buffer[col] = MAX_PROJECTION_HEIGHT;
      if (wall_height_buffer[col] != old_wall_height_buffer[col]) {
        draw_wall_sprite(col, wall_height_buffer[col], old_wall_height_buffer[col]);
        old_wall_height_buffer[col] = wall_height_buffer[col];
      }
    }

    // === КОПИРОВАНИЕ БУФЕРА НА ЭКРАН ===
    copy_pix_buf();
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
  unsigned char width;
  const t_sprite *p_sprite_descriptor;
  const char *p_sprite_data;
  const char *p_attr_data;
  
  // Выбор текстуры в зависимости от высоты стены
  if (height <= 4) {
    p_sprite_descriptor = &corn_mature_0;  // 8 пикселей
  } else if (height <= 8) {
    p_sprite_descriptor = &corn_mature_1;  // 16
  } else if (height <= 16) {
    p_sprite_descriptor = &corn_mature_2;  // 32Local (1)
  } else if (height <= 32) {
    p_sprite_descriptor = &corn_mature_3;  // 64
  } else {
    p_sprite_descriptor = &corn_mature_4;  // Полная 128-пиксельная текстура
  }

  width = p_sprite_descriptor->width;
  
  p_sprite_data = p_sprite_descriptor->p_sprite;
  p_sprite_data += x % p_sprite_descriptor->width;

  p_attr_data = p_sprite_descriptor->p_attributes;
  p_attr_data += x % p_sprite_descriptor->width;

  
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
  for (unsigned char i = 0; i < (height * 2); i++) {
    *p_buf = *p_sprite_data;
    p_buf += SCR_WIDTH;   // Переход на следующую строку (внутри столбца)
    p_sprite_data += width;
  }

  if (y > old_y) {
    for (unsigned char i = 0; i < (y - old_y); i++) {
      *p_buf = 0xff;
      p_buf += SCR_WIDTH;   // Переход на следующую строку (внутри столбца)
    }
  }
}

// === ПРОБРОС ЛУЧА ===
unsigned char trace_ray(int angle, int player_x, int player_y, int player_angle) {
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
