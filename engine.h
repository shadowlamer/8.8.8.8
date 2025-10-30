#ifndef __ENGINE_H
#define __ENGINE_H

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

void copy_pix_buf();                          // Копирует off-screen буфер на экран
void draw_wall_sprite(unsigned char x, unsigned char height, unsigned char old_height); // Рисует текстуру стены в буфере
void fill_wall_sprite(unsigned char x, unsigned char height); // (Не используется) Рисует шаблонную стену
unsigned char trace_ray(int angle, int player_x, int player_y, int player_angle);           // Пробрасывает луч и возвращает высоту стены
char get_map_at(unsigned int x, unsigned int y); // Получает значение карты по координатам
void pixel(unsigned char x, unsigned char y); // Устанавливает пиксель (не используется в основном цикле)
void calc_distance_deltas();                  // Предвычисляет таблицу высот по дистанции

void engine_init();
void engine_render(int player_x, int player_y, int player_angle);

#endif // __ENGINE_H
