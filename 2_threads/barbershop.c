#include <unistd.h>
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
  int customers_shop_max;
  sem_t customers_in_shop;
  sem_t customers_on_sofa;
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

int xsem_getvalue(sem_t *sem) {
  int val;
  sem_getvalue(sem, &val);
  return val;
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
    sem_post(&s->cut_barber_ready);

    /* I am waiting for a client */
    printf("barber %d waiting for customer\n", std->thread_num);
    sem_wait(&s->cut_customer_ready);

    /* I am cutting hair */
    printf("barber %d cutting hair\n", std->thread_num);
    sleep(1);
    /* Tell customer we're done */
    sem_post(&s->cut_done);

    /* Wait for cash desk until free */
    sem_wait(&s->cashdesk);
    printf("barber %d went to cash desk, waits for customer "
           "to pay\n", std->thread_num);
    /* Make the payment */
    sem_post(&s->pay_barber_ready);
    sem_wait(&s->pay_customer_ready);
    printf("barber %d got payment\n", std->thread_num);
    /* Free cash desk */
    sem_post(&s->cashdesk);
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
  while(-1 == sem_trywait(&s->customers_in_shop)) {
    printf("customer %d going for a drink\n", std->thread_num);
    sleep(1);
  }

  /* Wait on the sofa */
  sem_wait(&s->customers_on_sofa);
  printf("customer %d sitting on sofa, waits for barber\n",
         std->thread_num);

  /* Wait for a barber */
  sem_post(&s->cut_customer_ready);
  sem_wait(&s->cut_barber_ready);

  /* Leave the sofa */
  sem_post(&s->customers_on_sofa);
  printf("customer %d went up from sofa\n", std->thread_num);

  /* Wait until barber is done */
  sem_wait(&s->cut_done);

  /* Pay for the hair cut */
  sem_post(&s->pay_customer_ready);
  sem_wait(&s->pay_barber_ready);
  printf("customer %d payed money\n", std->thread_num);

  /* Leave the shop */
  sem_post(&s->customers_in_shop);

  printf("customer %d left the shop, %d still in the shop\n",
         std->thread_num, s->customers_shop_max - xsem_getvalue(&s->customers_in_shop));

  printf("customer %d died\n", std->thread_num);
  pthread_exit(0);
}

static void shop_create(struct shop *s)
{
  s->barbers = xmalloc(s->barbers_num * sizeof(*(s->barbers)));
  s->barbers_priv = xmalloc(s->barbers_num * sizeof(*(s->barbers_priv)));
  s->customers = xmalloc(s->customers_num * sizeof(*(s->customers)));
  s->customers_priv = xmalloc(s->customers_num * sizeof(*(s->customers_priv)));

  sem_init(&s->customers_in_shop, 0, s->customers_shop_max);
  sem_init(&s->customers_on_sofa, 0, 4);
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
  sem_destroy(&s->customers_on_sofa);
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
  s.customers_num = 40;
  s.customers_shop_max = 20;

  shop_create(&s);
  shop_run(&s);
  shop_cleanup(&s);

  return 0;
}
