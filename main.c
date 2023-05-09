#include <string.h>


#include "scr_addr.h"
#include "sincos.h"
#include "wall_sprites.h"
#include "map.h"

#define BUF_HEIGHT 100

/// Sinclair joystick key masks
typedef enum {
  KEY_FIRE  = 0b00010000,
  KEY_DOWN  = 0b00001000,
  KEY_UP    = 0b00000100,
  KEY_RIGHT = 0b00000010,
  KEY_LEFT  = 0b00000001
} key_t;

__at (SCREEN_BUFFER_START) char screen_buf[0x1800];

__sfr __at 0xfe joystick_keys_port;

static char pix_buffer[SCR_WIDTH * BUF_HEIGHT]; 

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
    
    if (key == KEY_LEFT)  player_angle -= 2;
    if (key == KEY_RIGHT) player_angle += 2;
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
  for (unsigned char i = 0; i < BUF_HEIGHT; i++) {
    memcpy(screen_line_addrs[i], p_buf, SCR_WIDTH);
    p_buf += SCR_WIDTH;
  }
  memset(pix_buffer, 0x00, SCR_WIDTH * BUF_HEIGHT);
}

void draw_wall_sprite(char *p_sprite,
                      unsigned char x, 
                      unsigned char height) {
  unsigned char y = (BUF_HEIGHT / 2) - height;  
  char *p_buf = pix_buffer + ((SCR_WIDTH * y) + x);
  
  (void) p_sprite;
  for (unsigned char i = 0; i < (height * 2); i++) {
    *p_buf = 0xff;
    p_buf += SCR_WIDTH;
  }
}

unsigned char trace_ray(unsigned int angle) {
    unsigned char eff_angle = (angle + player_angle - (SCR_WIDTH / 2)) & 0xff;
    unsigned int ray = 32;
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
    return ray;
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

