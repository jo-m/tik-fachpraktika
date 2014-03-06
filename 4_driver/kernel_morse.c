#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/vt_kern.h>

#define DEVNAME   "morse"
#define FIRSTMINOR  100

struct morse_item {
  u8 len;
  u8 code;
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

static unsigned int gap_code = 250; /* us between dots or dashes */
static unsigned int gap_char = 2000;  /* us between characters */

static unsigned int dot_len  = 250; /* length of a dot */
static unsigned int dash_len = 1000;  /* length of a dash */

static struct cdev *morse_chardev;
static dev_t morse_dev;

static struct tty_driver *console_driver;

static void led_on(int led)
{
  console_driver->ops->ioctl(vc_cons[fg_console].d->vc_tty, NULL,
           KDSETLED, led);
}

static void led_off(void)
{
  console_driver->ops->ioctl(vc_cons[fg_console].d->vc_tty, NULL,
           KDSETLED, 0);
}

static int validate(int ascii)
{
  /* Insert your solution from A1 */
  /* ... */
}

static void blink(int is_long)
{
  /* Insert your solution from A1 */
  /* ... */
}

static void morse_char(int ascii)
{
  /* Insert your solution from A1 */
  /* ... */
}

static void morse_handle(char *message, size_t len)
{
  int i = 0;
  console_driver = vc_cons[fg_console].d->vc_tty->driver;

  led_off();
  while (i < len) {
    morse_char(message[i]);
    msleep(gap_char);
    i++;
  }
  /* Reset leds to initial state */
  /* Should work, see setledstate in keyboard.c */
  led_on(0xFF);
}

static ssize_t morse_write(struct file *filp, const char __user * buf,
         size_t count, loff_t * f_pos)
{
  int ret;
  char *data;

  printk("Morsedev write called!\n");

  data = kmalloc(count, GFP_KERNEL);
  if (!data) {
    printk("No mem left on device!\n");
    return -ENOMEM;
  }
  ret = copy_from_user(data, buf, count);
  if (ret != 0) {
    printk ("Cannot copy data from user space: %i\n", ret);
    return -EIO;
  }
  morse_handle(data, count);
  kfree(data);

  return count;
}

#define BEEP_IOC_MAGIC 'B'
#define BEEP_IOC_MAXNR 4

static int morse_ioctl(struct inode *inode, struct file *filp,
           unsigned int cmd, unsigned long arg)
{
  printk("Morsedev ioctl called\n");
  return 0;
}

static struct file_operations morse_fops __read_mostly = {
  .owner  = THIS_MODULE,
  .write  = morse_write,
  .ioctl  = morse_ioctl,
};

static int __init morse_init(void)
{
  int ret;

  printk("Morsedev module init\n");
  ret = alloc_chrdev_region(&morse_dev, FIRSTMINOR, 1, DEVNAME);
  if (ret != 0) {
    printk("Unable to allocate chrdev_region: %d\n", ret);
    return ret;
  }

  printk("Morsedev major is %d\n", MAJOR(morse_dev));
  printk("Morsedev minor is %d\n", MINOR(morse_dev));

  morse_chardev = cdev_alloc();
  morse_chardev->ops = &morse_fops;
  morse_chardev->owner = THIS_MODULE;

  ret = cdev_add(morse_chardev, morse_dev, 1);
  if (ret < 0) {
    printk ("Registering chardev failed with %i\n", ret);
    unregister_chrdev_region(morse_dev, 1);
    return ret;
  }

  return 0;
}

static void __exit morse_cleanup(void)
{
  printk("Morsedev cleanup\n");

  cdev_del(morse_chardev);
  unregister_chrdev_region(morse_dev, 1);
}

module_init(morse_init);
module_exit(morse_cleanup);

MODULE_LICENSE ("GPL");

