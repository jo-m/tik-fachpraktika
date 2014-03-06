#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <linux/kd.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/types.h>

struct morse_item {
  uint8_t len;
  uint8_t code;
};

/* 0 = dot, 1 = dash, stored in hex */
static struct morse_item alphabet[] = {
  { /* 32   */ 0, 0x00 },
  { /* 33 ! */ 5, 0x06 }, //00110
  { /* 34 " */ 6, 0x12 }, //010010
  { /* 35 # */ 0, 0x00 },
  { /* 36 $ */ 0, 0x00 },
  { /* 37 % */ 0, 0x00 },
  { /* 38 & */ 0, 0x00 },
  { /* 39 ' */ 6, 0x1E }, //011110
  { /* 40 ( */ 0, 0x00 },
  { /* 41 ) */ 0, 0x00 },
  { /* 42 * */ 0, 0x00 },
  { /* 43 + */ 0, 0x00 },
  { /* 44 , */ 6, 0x33 }, //110011
  { /* 45 - */ 0, 0x00 },
  { /* 46 . */ 6, 0x16 }, //010101
  { /* 47 / */ 0, 0x00 },
  { /* 48 0 */ 5, 0x1F }, //11111
  { /* 49 1 */ 5, 0x0F }, //01111
  { /* 50 2 */ 5, 0x07 }, //00111
  { /* 51 3 */ 5, 0x03 }, //00011
  { /* 52 4 */ 5, 0x01 }, //00001
  { /* 53 5 */ 5, 0x00 }, //00000
  { /* 54 6 */ 5, 0x10 }, //10000
  { /* 55 7 */ 5, 0x18 }, //11000
  { /* 56 8 */ 5, 0x1C }, //11100
  { /* 57 9 */ 5, 0x1E }, //11110
  { /* 58 : */ 6, 0x38 }, //111000
  { /* 59 ; */ 0, 0x00 },
  { /* 60 < */ 0, 0x00 },
  { /* 61 = */ 5, 0x11 }, //10001
  { /* 62 > */ 0, 0x00 },
  { /* 63 ? */ 6, 0x0C }, //001100
  { /* 64 @ */ 0, 0x00 },
  { /* 65 A */ 2, 0x01 }, //01
  { /* 66 B */ 4, 0x08 }, //1000
  { /* 67 C */ 4, 0x0A }, //1010
  { /* 68 D */ 3, 0x04 }, //100
  { /* 69 E */ 1, 0x00 }, //0
  { /* 70 F */ 4, 0x02 }, //0010
  { /* 71 G */ 3, 0x06 }, //110
  { /* 72 H */ 4, 0x00 }, //0000
  { /* 73 I */ 2, 0x00 }, //00
  { /* 74 J */ 4, 0x07 }, //0111
  { /* 75 K */ 3, 0x05 }, //101
  { /* 76 L */ 4, 0x04 }, //0100
  { /* 77 M */ 2, 0x03 }, //11
  { /* 78 N */ 2, 0x02 }, //10
  { /* 79 O */ 3, 0x07 }, //111
  { /* 80 P */ 4, 0x06 }, //0110
  { /* 81 Q */ 4, 0x0D }, //1101
  { /* 82 R */ 3, 0x02 }, //010
  { /* 83 S */ 3, 0x00 }, //000
  { /* 84 T */ 1, 0x01 }, //1
  { /* 85 U */ 3, 0x01 }, //001
  { /* 86 V */ 4, 0x01 }, //0001
  { /* 87 W */ 3, 0x03 }, //011
  { /* 88 X */ 4, 0x09 }, //1001
  { /* 89 Y */ 4, 0x0B }, //1011
  { /* 90 Z */ 4, 0x0C }  //1100
};

static unsigned int gap_code = 250000;  /* us between dots or dashes */
static unsigned int gap_char = 2000000; /* us between characters */

static unsigned int dot_len  = 250000;  /* length of a dot */
static unsigned int dash_len = 1000000; /* length of a dash */

static int fd;

static void panic(const char *serror)
{
  printf("%s", serror);
  exit(1);
}

static void led_on(int led)
{
  /* Turn the led on */
  /* ... */
}

static void led_off(void)
{
  /* Turn the led on */
  /* ... */
}

static int validate(int ascii)
{
  /* Check that the input character corresponds to one that is
   * available in the morse alphabet. validate must always return
   * a vaild one! */
  /* ... */
}

static void blink(int is_long)
{
  /* Let the LED_NUM blink once, is_long = 1 -> it is a dash
   * is_long = 0 -> it is a dot; Use the led_on and led_off functions */
  /* ... */
}

static void morse_char(int ascii)
{
  /* Morse the ascii character. Validate it first (not all ascii
   * characters are available in the morse alphabet) with the
   * function validate */
  /* The binary of morse_item.code corresponds to the morsecode.
   * Hint: use a shift operator and the blink function. */
}

int main(int argc, char **argv)
{
  char *message;
  long int initial_leds = 0;
  int msg_len, i = 0, ret;

  if (argc != 2)
    panic("Usage: ./user_led \"test message\"\n");

  /* Get a file descriptor, with which we can access the
   * keyboard ioctl(2) store it in fd */
  /* ... */

  /* Get the LEDs that are turned on and store them in initial_leds */
  /* ... */

  led_off();

  message = argv[1];
  msg_len = strlen(message);

  for (i = 0; i < msg_len; ++i) {
    morse_char(message[i]);
    usleep(gap_char);
  }

  /* Set LEDs to original state */
  /* Note: the vmware might fail in doing so, even if your code is
   * correct ... */
  /* ... */

  close(fd);
  return 0;
}
