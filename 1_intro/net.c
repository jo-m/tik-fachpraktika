#include <stdio.h>
#include <stdint.h>

// gcc -Wall net.c -o net && ./net

int main(void)
{
	uint8_t add[] = {129, 169,  25, 100};
	uint8_t sub[] = {255, 255, 240,   0};
	uint8_t net[4];

  for(int i = 0; i < 4; i++) {
    net[i] = add[i] & sub[i];
  }

	printf("net address: %u.%u.%u.%u\n", net[0], net[1], net[2], net[3]);

	return 0;
}
