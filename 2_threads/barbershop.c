#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <errno.h>
#include <signal.h>

struct shop_thread_priv_data;

struct shop {
  sem_t customers_in_shop;
  int customers_shop_max;
  int customers_curr;
  sem_t sofa;
  sem_t cashdesk;
  sem_t cut_customer_ready;
  sem_t cut_barber_ready;
  sem_t cut_done;
  sem_t pay_customer_ready;
  sem_t pay_barber_ready;
  pthread_t *barbers;
  struct shop_thread_priv_data *barbers_priv;
  int barbers_num;
  pthread_t *customers;
  int customers_num;
  struct shop_thread_priv_data *customers_priv;
};

struct shop_thread_priv_data {
  int thread_num;
  struct shop *shop;
};

static void panic(const char *serror)
{
  printf("%s", serror);
  exit(1);
}

static void *xmalloc(size_t size)
{
  void *ptr;
  if (size == 0)
    panic("Size is 0!\n");
  ptr = malloc(size);
  if (!ptr)
    panic("No mem left!\n");
  return ptr;
}

static void xfree(void *ptr)
{
  if (ptr == NULL)
    panic("Got Null-Pointer!\n");
  free(ptr);
}

void signal_quit(int signum)
{
  pthread_exit(0);
}

static void *barber_main(void *arg)
{
  struct shop *s;
  struct shop_thread_priv_data *std = arg;

  printf("barber %d alive\n", std->thread_num);
  s = std->shop;
  signal(SIGQUIT, signal_quit);

  /* Do the barber job */
  while (1) {
    /* I want to cut hair */ 
    /* ... */
    /* I am waiting for a client */
    printf("barber %d waiting for customer\n", std->thread_num);
    /* ... */
    /* I am cutting hair */
    printf("barber %d cutting hair\n", std->thread_num);
    sleep(3);
    /* Tell customer we're done */
    /* ... */
    /* Wait for cash desk until free */
    /* ... */
    printf("barber %d went to cash desk, waits for customer "
           "to pay\n", std->thread_num);
    /* Make the payment */
    /* ... */
    printf("barber %d got payment\n", std->thread_num);
    /* Free cash desk */
    /* ... */
  }

  printf("barber %d died\n", std->thread_num);
  pthread_exit(0);
}

static void *customer_main(void *arg)
{
  struct shop *s;
  struct shop_thread_priv_data *std = arg;

  printf("customer %d alive\n", std->thread_num);
  s = std->shop;

  /* Try to enter the shop */
  /* ... */

  /* Wait on the sofa */
  /* ... */
  printf("customer %d sitting on sofa, waits for barber\n",
         std->thread_num);

  /* Wait for a barber */
  /* ... */

  /* Leave the sofa */
  /* ... */
  printf("customer %d went up from sofa\n", std->thread_num);

  /* Wait until barber is done */
  /* ... */

  /* Pay for the hair cut */
  /* ... */
  printf("customer %d payed money\n", std->thread_num);

  /* Leave the shop */
  /* ... */

  printf("customer %d left the shop, %d still in the shop\n",
         std->thread_num, s->customers_curr);

  printf("customer %d died\n", std->thread_num);
  pthread_exit(0);
}

static void shop_create(struct shop *s)
{
  s->barbers = xmalloc(s->barbers_num * sizeof(*(s->barbers)));
  s->barbers_priv = xmalloc(s->barbers_num * sizeof(*(s->barbers_priv)));
  s->customers = xmalloc(s->customers_num * sizeof(*(s->customers)));
  s->customers_priv = xmalloc(s->customers_num * sizeof(*(s->customers_priv)));

  sem_init(&s->customers_in_shop, 0, 1);
  sem_init(&s->sofa, 0, 4);
  sem_init(&s->cashdesk, 0, 1);

  sem_init(&s->cut_customer_ready, 0, 0);
  sem_init(&s->cut_barber_ready, 0, 0);
  sem_init(&s->cut_done, 0, 0);
  sem_init(&s->pay_customer_ready, 0, 0);
  sem_init(&s->pay_barber_ready, 0, 0);
}

static void shop_run(struct shop *s)
{
  int i, ret;
  struct shop_thread_data *std;

  for (i = 0; i < s->barbers_num; ++i) {
    s->barbers_priv[i].thread_num = i;
    s->barbers_priv[i].shop = s;
    ret = pthread_create(&s->barbers[i], NULL, barber_main,
             &s->barbers_priv[i]);
    if (ret < 0)
      panic("Cannot create thread!\n");
  }
  for (i = 0; i < s->customers_num; ++i) {
    s->customers_priv[i].thread_num = i;
    s->customers_priv[i].shop = s;
    ret = pthread_create(&s->customers[i], NULL, customer_main,
             &s->customers_priv[i]);
    if (ret < 0)
      panic("Cannot create thread!\n");
  }
}

static void shop_cleanup(struct shop *s)
{
  int i;

  for (i = 0; i < s->customers_num; ++i)
    pthread_join(s->customers[i], NULL);
  for (i = 0; i < s->barbers_num; ++i)
    pthread_kill(s->barbers[i], SIGQUIT);
  for (i = 0; i < s->barbers_num; ++i)
    pthread_join(s->barbers[i], NULL);

  xfree(s->customers);
  xfree(s->customers_priv);
  xfree(s->barbers);
  xfree(s->barbers_priv);

  sem_destroy(&s->customers_in_shop);
  sem_destroy(&s->sofa);
  sem_destroy(&s->cashdesk);

  sem_destroy(&s->cut_customer_ready);
  sem_destroy(&s->cut_barber_ready);
  sem_destroy(&s->cut_done);
  sem_destroy(&s->pay_customer_ready);
  sem_destroy(&s->pay_barber_ready);
}

int main(int argc, char **argv)
{
  struct shop s;

  memset(&s, 0, sizeof(s));
  s.barbers_num = 4;
  s.customers_num = 20;
  s.customers_shop_max = 20;

  shop_create(&s);
  shop_run(&s);
  shop_cleanup(&s);

  return 0;
}
