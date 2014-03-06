#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#define MAX_STR_LEN	4096

static void panic(const char* serror)
{
	fprintf(stderr, "%s:", serror);
	perror("");
	exit(1);
}

static void test_data(int *data, size_t len)
{
	int i;
	off_t offset = 0;
	char argstr[MAX_STR_LEN];
	char finstr[MAX_STR_LEN + 32];

	if (len >= (MAX_STR_LEN / 3) - 1)
		panic("No mem left!\n");

	memset(argstr, 0, sizeof(argstr));
	memset(finstr, 0, sizeof(finstr));

	for (i = 0; i < len; ++i) {
		if (data[i] < 0 || data[i] >= 100)
			continue;
		offset += sprintf(argstr + offset, "%2d ", data[i]);
	}

	/* Okay here since we cannot run into an buffer overflow */
	strcpy(finstr, "./tester ");
	strcat(finstr, argstr);
	system(finstr);
}

int main(void)
{
	key_t key;
	const int SIGNAL = -4;
	int shmid, *shared_data, read[27], i, ret;

	/* Note: always check the error values and implement proper
	 * error handling! */

	/* Make the key */
	key = ftok("producer.c", 'R');
	if (key < 0)
		panic("ftok error!");

	shmid = shmget(key, sizeof(*shared_data), 0666 | IPC_CREAT);
	if (shmid < 0)
		panic("shmget error!");

	shared_data = shmat(shmid, NULL, 0);
	if (shared_data == (int *) (-1))
		panic("Cannot attach to segment!");

	printf("Consumer starts\n");
	for (i = 0; i < sizeof(read) / sizeof(read[0]); ++i) {
		/* Read value */
		read[i] = *shared_data;

		printf("Consumer read: %d\n", read[i]);
		/* Notify producer that data has been read */
		*shared_data = SIGNAL;

		/* Wait for the next value */
		while (*shared_data == SIGNAL)
			sleep(0);
	}
	/* Detach from segment */
	ret = shmdt(shared_data);
	if (ret < 0)
		panic("Cannot detach from segment!");

	ret = shmctl(shmid, IPC_RMID, 0);
	if (ret < 0)
		panic("Cannot mark segement for removal!");

	test_data(read, sizeof(read) / sizeof(read[0]));
	return 0;
}
