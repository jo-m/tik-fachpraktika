#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>

static void panic(const char* serror)
{
	fprintf(stderr, "%s:", serror);
	perror("");
	exit(1);
}

int main(void)
{
	key_t key;
	const int SIGNAL = -4;
	const int ENDSIGNAL = -5;
	int shmid, *shared_data, i, ret;
	int data[] = {20, 16, 0, 19, 15, 21, 11, 10, 22, 4, 24, 13, 17, 14,
		      18, 3, 12, 5, 8, 23, 2, 9, 25, 1, 26, 6, 7};

	key = ftok("producer.c", 'R');
	if (key < 0)
		panic("ftok error!");

	shmid = shmget(key, sizeof(*shared_data), 0666 | IPC_CREAT);
	if (shmid < 0)
		panic("shmget error!");

	shared_data = shmat(shmid, NULL, 0);
	if (shared_data == (int *) (-1))
		panic("Cannot attach to segment!");

	printf("Producer ready!\n");
	for (i = 0; i < sizeof(data) / sizeof(data[0]); ++i) {
		(*shared_data) = data[i];
		printf("Producer wrote: %d\n", *shared_data);
		while (*shared_data != SIGNAL)
			sleep(0);
	}
	*shared_data = ENDSIGNAL;

	ret = shmdt(shared_data);
	if (ret < 0)
		panic("Cannot detach from segment!");

	ret = shmctl(shmid, IPC_RMID, 0);
	if (ret < 0)
		panic("Cannot mark segement for removal!");

	return 0;
}

