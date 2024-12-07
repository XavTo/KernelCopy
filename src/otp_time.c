#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/ioctl.h>
#include <linux/device.h>
#include <linux/string.h>
#include <linux/jiffies.h> // pour le temps
#include <linux/ktime.h>
#include <linux/random.h>
#include <linux/crypto.h>
#include <crypto/hash.h> // pour le HMAC

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Xavier, Eleonore, Alexis");
MODULE_DESCRIPTION("Module OTP basé sur clé et temps (TOTP)");
MODULE_VERSION("1.0");

#define DEVICE_CLASS "timeotp_class"
#define DEVICE_BASE_NAME "timeotp"
#define MAX_DEVICES 1

#define OTP_IOC_MAGIC 'k'
#define OTP_IOC_SET_KEY _IOW(OTP_IOC_MAGIC, 1, char *)
#define OTP_IOC_SET_DURATION _IOW(OTP_IOC_MAGIC, 2, int)
#define OTP_DIGITS 6
#define OTP_DIGITS_POWTEN 1000000 // pow(10, OTP_DIGITS) : 1 et six 0
#define SHA1_DIGEST_SIZE 20

static dev_t dev_num_base;
static struct class* timeotp_class = NULL;

struct timeotp_device {
    struct cdev cdev;
    struct device* device;
    char key[64];
    int duration; // en secondes
    struct mutex lock;
};

static struct timeotp_device timeotp_dev;

static char *timeotp_devnode(const struct device *dev, umode_t *mode) {
    if (mode)
        *mode = 0666;
    return NULL;
}

static int timeotp_open(struct inode *inode, struct file *filep) {
    filep->private_data = &timeotp_dev;
    return 0;
}

static int timeotp_release(struct inode *inode, struct file *filep) {
    return 0;
}

// dynamic truncation de la méthode HOTP de la RFC 4226
static unsigned int truncate_to_otp(const unsigned char *hash, size_t hash_len) {
    // dynamic offset = 4 bits du bas
    unsigned int offset = hash[hash_len - 1] & 0x0F;
    unsigned int binary =
        ((hash[offset] & 0x7F) << 24) |
        ((hash[offset + 1] & 0xFF) << 16) |
        ((hash[offset + 2] & 0xFF) << 8) |
        (hash[offset + 3] & 0xFF);
    return binary % OTP_DIGITS_POWTEN; // tronque à OTP_DIGITS
}

static void generate_time_otp(struct timeotp_device *dev, char *buf, size_t size) {
    u64 now = ktime_get_real_seconds();
    u64 slot = now / (dev->duration ? dev->duration : 30); // défaut: 30 secondes
    unsigned char digest[SHA1_DIGEST_SIZE];

    struct crypto_shash *tfm;
    struct shash_desc *shash;

    // contexte crypto
    tfm = crypto_alloc_shash("hmac(sha1)", 0, 0);
    if (IS_ERR(tfm)) {
        snprintf(buf, size, "ERROR\n");
        return;
    }

    // alloue de l'espace pour hashing
    shash = kmalloc(sizeof(*shash) + crypto_shash_descsize(tfm), GFP_KERNEL);
    if (!shash) {
        crypto_free_shash(tfm);
        snprintf(buf, size, "ERROR\n");
        return;
    }

    shash->tfm = tfm;

    // calcule le HMAC-SHA-1(slot, key) puis tronque avec la méthode HOTP de la RFC 4226
    if (crypto_shash_setkey(tfm, dev->key, strlen(dev->key)) == 0 &&
        crypto_shash_init(shash) == 0 &&
        crypto_shash_update(shash, (u8 *)&slot, sizeof(slot)) == 0 &&
        crypto_shash_final(shash, digest) == 0) {
        unsigned int otp = truncate_to_otp(digest, SHA1_DIGEST_SIZE);
        snprintf(buf, size, "%0*u\n", OTP_DIGITS, otp); // formate l'OTP à OTP_DIGITS de longueur
    } else {
        snprintf(buf, size, "ERROR\n");
    }

    kfree(shash);
    crypto_free_shash(tfm);
}

static ssize_t timeotp_read(struct file *filep, char __user *buffer, size_t len, loff_t *offset) {
    struct timeotp_device *dev = filep->private_data;
    char otp_buf[64];

    if (*offset > 0)
        return 0;

    mutex_lock(&dev->lock);
    generate_time_otp(dev, otp_buf, sizeof(otp_buf));
    mutex_unlock(&dev->lock);

    size_t otp_len = strnlen(otp_buf, sizeof(otp_buf));
    if (otp_len == 0)
        return 0;

    if (otp_len > len)
        otp_len = len;

    if (copy_to_user(buffer, otp_buf, otp_len))
        return -EFAULT;

    *offset += otp_len;
    return otp_len;
}

static long timeotp_ioctl(struct file *filep, unsigned int cmd, unsigned long arg) {
    struct timeotp_device *dev = filep->private_data;

    switch (cmd) {
        case OTP_IOC_SET_KEY: {
            char kbuf[64];
            if (copy_from_user(kbuf, (char __user *)arg, sizeof(kbuf)-1))
                return -EFAULT;
            kbuf[sizeof(kbuf)-1] = '\0';

            mutex_lock(&dev->lock);
            strncpy(dev->key, kbuf, sizeof(dev->key)-1);
            dev->key[sizeof(dev->key)-1] = '\0';
            mutex_unlock(&dev->lock);
            printk(KERN_INFO "timeotp: Clé définie à '%s'\n", dev->key);
            break;
        }

        case OTP_IOC_SET_DURATION: {
            int d;
            if (copy_from_user(&d, (int __user *)arg, sizeof(d)))
                return -EFAULT;
            if (d <= 0)
                d = 30;
            mutex_lock(&dev->lock);
            dev->duration = d;
            mutex_unlock(&dev->lock);
            printk(KERN_INFO "timeotp: Durée définie à %d secondes\n", d);
            break;
        }

        default:
            return -EINVAL;
    }

    return 0;
}

static struct file_operations timeotp_fops = {
    .owner = THIS_MODULE,
    .open = timeotp_open,
    .release = timeotp_release,
    .read = timeotp_read,
    .unlocked_ioctl = timeotp_ioctl,
};

static int __init timeotp_init(void) {
    int ret;
    ret = alloc_chrdev_region(&dev_num_base, 0, MAX_DEVICES, "timeotpdev");
    if (ret < 0) {
        printk(KERN_ERR "timeotp: Impossible d'allouer un numéro majeur\n");
        return ret;
    }

    timeotp_class = class_create(DEVICE_CLASS);
    if (IS_ERR(timeotp_class)) {
        unregister_chrdev_region(dev_num_base, MAX_DEVICES);
        return PTR_ERR(timeotp_class);
    }

    timeotp_class->devnode = timeotp_devnode;

    cdev_init(&timeotp_dev.cdev, &timeotp_fops);
    timeotp_dev.cdev.owner = THIS_MODULE;
    mutex_init(&timeotp_dev.lock);
    timeotp_dev.key[0] = '\0';
    timeotp_dev.duration = 30; // défaut 30s

    ret = cdev_add(&timeotp_dev.cdev, dev_num_base, 1);
    if (ret < 0) {
        class_destroy(timeotp_class);
        unregister_chrdev_region(dev_num_base, MAX_DEVICES);
        return ret;
    }

    timeotp_dev.device = device_create(timeotp_class, NULL, dev_num_base, NULL, "timeotp0");
    if (IS_ERR(timeotp_dev.device)) {
        cdev_del(&timeotp_dev.cdev);
        class_destroy(timeotp_class);
        unregister_chrdev_region(dev_num_base, MAX_DEVICES);
        return PTR_ERR(timeotp_dev.device);
    }

    printk(KERN_INFO "timeotp: Module chargé, device /dev/timeotp0 créé\n");
    return 0;
}

static void __exit timeotp_exit(void) {
    device_destroy(timeotp_class, dev_num_base);
    cdev_del(&timeotp_dev.cdev);
    class_destroy(timeotp_class);
    unregister_chrdev_region(dev_num_base, MAX_DEVICES);
    printk(KERN_INFO "timeotp: Module déchargé\n");
}

module_init(timeotp_init);
module_exit(timeotp_exit);
