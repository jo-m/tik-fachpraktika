#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <signal.h>

// gcc -Wall -o makethreads makethreads.c -lpthread && ./makethreads

void handle_sigint(int signum)
{
  printf("terminating.\n");
  exit(EXIT_SUCCESS);
}

void err_exit(const int c) {
  printf("Invalid or not enough arguments received (Error code: %d)\n", errno);
  puts("    Usage: ./makethreads <numthreads>");
  exit(1);
}

void *hello_thread(void *arg) {
  while(1) {
    printf("Thread <%d>: Hello World!\n", (int)arg);
    sleep(1);
  }
}

int main(const int argc, char const **argv) {
  int i;
  if(argc < 2) err_exit(1);
  const long numthreads = strtol(argv[1], NULL, 10);
  if(0 != errno) err_exit(2);
  pthread_t threadinfo;

  for(i = 0; i < numthreads; i++)
    pthread_create(&threadinfo, NULL, hello_thread, (void *)i);

  signal(SIGINT, handle_sigint);

  while(1)
    sleep(1);

  return EXIT_SUCCESS;
}
