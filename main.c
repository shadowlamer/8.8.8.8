#pragma opt_code_speed

#include <string.h>

#include "scr_addr.h"
#include "sincos.h"
#include "wall_sprites.h"
#include "map.h"

#define PIX_BUFFER_HEIGHT 128
#define PIX_BUFFER_SIZE (PIX_BUFFER_HEIGHT * SCR_WIDTH)
#define PIX_BUFFER_START (0xf000 - PIX_BUFFER_SIZE)
#define PIX_ATTR_BUFFER_HEIGHT (PIX_BUFFER_HEIGHT / 8)
#define PIX_ATTR_BUFFER_SIZE (SCR_WIDTH * PIX_ATTR_BUFFER_HEIGHT)
#define PIX_ATTR_BUFFER_START (PIX_BUFFER_START - (PIX_ATTR_BUFFER_SIZE))

#define MAX_DISTANCE 16
#define WALL_HEIGHT_COEFF (PIX_BUFFER_HEIGHT / 2 / MAX_DISTANCE)

#define NUM_WALL_COLORS 6

/// Sinclair joystick key masks
typedef enum {
  KEY_FIRE  = 0b00010000,
  KEY_DOWN  = 0b00001000,
  KEY_UP    = 0b00000100,
  KEY_RIGHT = 0b00000010,
  KEY_LEFT  = 0b00000001
} key_t;

__at (SCREEN_BUFFER_START) char screen_buf[0x1800];
__at (ATTR_SCREEN_BUFFER_START) char attr_buf[0x300];
__at (PIX_BUFFER_START) char pix_buffer[PIX_BUFFER_SIZE];
__at (PIX_ATTR_BUFFER_START) char pix_attr_buffer[PIX_ATTR_BUFFER_SIZE];

__sfr __at 0xfe joystick_keys_port;

static const unsigned char wall_attributes[NUM_WALL_COLORS] = {
 0b00000001,
 0b01000001,
 0b00001001,
 0b01001001,
 0b00001111,
 0b01001111
};

static unsigned int player_angle = 0;
static int player_x = 10 * 256;
static int player_y = 10 * 256;
static unsigned char key;


void copy_pix_buf();
void draw_wall_sprite(char *p_sprite,
  		      unsigned char x, 
                      unsigned char height);
unsigned char trace_ray(unsigned int angle);
char get_map_at(unsigned int x, unsigned int y);
void pixel(unsigned char x, unsigned char y);

int main() {
  while(1) {
    key = joystick_keys_port & 0x1f ^ 0x1f;
    
    if (key == KEY_LEFT)  player_angle -= 8;
    if (key == KEY_RIGHT) player_angle += 8;
    if (key == KEY_UP)    {
      player_x += COS(player_angle);
      player_y += SIN(player_angle);
    }
    if (key == KEY_DOWN)    {
      player_x -= COS(player_angle);
      player_y -= SIN(player_angle);
    }
    
    player_angle = player_angle & 0x00ff;
    
    for (unsigned char col = 0; col < SCR_WIDTH; col++) {
      draw_wall_sprite(NULL, col, trace_ray(col));
//      trace_ray(col);
    }
    copy_pix_buf();
  }
  return 0;
}

void copy_pix_buf() {
  char *p_buf = pix_buffer;

  memcpy(attr_buf, pix_attr_buffer, PIX_ATTR_BUFFER_SIZE);
  memset(pix_attr_buffer, 0x04, PIX_ATTR_BUFFER_SIZE);

  for (unsigned char i = 0; i < PIX_BUFFER_HEIGHT; i++) {
    memcpy(screen_line_addrs[i], p_buf, SCR_WIDTH);
    p_buf += SCR_WIDTH;
  }
  memset(pix_buffer, 0x00, PIX_BUFFER_SIZE / 2);
  memset(pix_buffer + PIX_BUFFER_SIZE / 2, 0xff, PIX_BUFFER_SIZE / 2);
}

void draw_wall_sprite(char *p_sprite,
                      unsigned char x, 
                      unsigned char height) {
  static char pattern = 0b01010101;
  unsigned char y = (PIX_BUFFER_HEIGHT / 2) - height;  
  char *p_buf = pix_buffer + ((SCR_WIDTH * y) + x);
  
  (void) p_sprite;
  for (unsigned char i = 0; i < (height * 2); i++) {
    *p_buf = pattern;
    p_buf += SCR_WIDTH;
    pattern = ~pattern;
  }
  p_buf = pix_attr_buffer + ((SCR_WIDTH * (y / 8)) + x);
  for (unsigned char i = 0; i < (height  / 4); i++) {
    unsigned char wall_color_index = height / 8;
    if (wall_color_index > NUM_WALL_COLORS - 1) wall_color_index = NUM_WALL_COLORS - 1;
    *p_buf = wall_attributes[wall_color_index];
    p_buf += SCR_WIDTH;
  }
}

unsigned char trace_ray(unsigned int angle) {
    unsigned char eff_angle = (angle + player_angle - (SCR_WIDTH / 2)) & 0xff;
    unsigned int ray = MAX_DISTANCE;
    int sin = SIN(eff_angle);
    int cos = COS(eff_angle);
    int x = player_x;
    int y = player_y;

  (void) angle;
  
  while (get_map_at(x, y) == 0 && ray > 0 && x > 0 && y > 0) {
        x += cos;
        y += sin;
        ray--;
    }
    return ray * WALL_HEIGHT_COEFF;
}

char get_map_at(unsigned int x, unsigned int y){
    unsigned char mapX = ( x >> 8 ) & 0xff;
    unsigned char mapY = ( y >> 8 ) & 0xff;
    return map[mapY][mapX];
}

void pixel(unsigned char x, unsigned char y) {
  unsigned char coarse_x = x / 8;
  unsigned char fine_x = x % 8;
  pix_buffer[y * SCR_WIDTH + coarse_x] |= 0x80 >> fine_x;
}

