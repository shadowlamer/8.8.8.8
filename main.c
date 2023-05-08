#include <string.h>

#include "scr_addr.h"
#include "wall_sprites.h"

#include "map.h"


__at (SCREEN_BUFFER_START) char screen_buf[0x1800];

__sfr __at 0xfe joystick_keys_port;

static char pix_buffer[0x800]; 


void copy_pix_buf();
void draw_wall_sprite(char *p_sprite,
  		      unsigned char x, 
                      unsigned char y, 
                      unsigned char height);

int main() {
  while(1) {
    for (unsigned char col = 0; col < SCR_WIDTH; col++) {
      draw_wall_sprite(NULL, col, 0, 30);
    }
    copy_pix_buf();
  }
  return 0;
}

void copy_pix_buf() {
  char *p_buf = pix_buffer;
  for (unsigned char i = 0; i < 64; i++) {
    memcpy(screen_line_addrs[i], p_buf, SCR_WIDTH);
    p_buf += SCR_WIDTH;
  }
}

void draw_wall_sprite(char *p_sprite,
                      unsigned char x, 
                      unsigned char y, 
                      unsigned char height) {
    
  char *p_buf = pix_buffer + ((SCR_WIDTH * y) + x);
  
  (void) p_sprite;
  for (unsigned char i = 0; i < height; i++) {
    *p_buf = 0xff;
    p_buf += SCR_WIDTH;
  }
}
