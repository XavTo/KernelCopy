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
#include <linux/random.h> // Pour get_random_bytes

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Xavier, Eleonore, Alexis");
MODULE_DESCRIPTION("Module OTP basé sur une liste avec support multiple de périphériques");
MODULE_VERSION("1.1");

#define DEVICE_CLASS "otp_class"
#define DEVICE_BASE_NAME "otpdev"
#define OTP_IOC_MAGIC 'k'
#define OTP_IOC_ADD _IOW(OTP_IOC_MAGIC, 1, char *)
#define OTP_IOC_DEL _IOW(OTP_IOC_MAGIC, 2, char *)
#define OTP_IOC_LIST _IOR(OTP_IOC_MAGIC, 3, char *)
#define MAX_DEVICES 5

static dev_t dev_num_base;
static struct class* otp_class = NULL;

struct otp_entry {
    char password[32];
    struct list_head list;
};

struct otp_device {
    struct cdev cdev;
    struct list_head otp_list_head;
    struct mutex list_mutex;
    struct device* device;
};

static struct otp_device otp_devices[MAX_DEVICES];

// Callback devnode pour définir les permissions des périphériques (0666)
static char *otp_devnode(const struct device *dev, umode_t *mode) {
    if (mode)
        *mode = 0666; // Lecture/Écriture pour tous (tests)
    return NULL;
}

static int otp_open(struct inode *inodep, struct file *filep) {
    struct otp_device *otp_dev = container_of(inodep->i_cdev, struct otp_device, cdev);
    filep->private_data = otp_dev;
    return 0;
}

static int otp_release(struct inode *inodep, struct file *filep) {
    return 0;
}

static ssize_t otp_read(struct file *filep, char __user *buffer, size_t len, loff_t *offset) {
    struct otp_device *otp_dev = filep->private_data;
    struct otp_entry *entry;
    char otp_buf[64];
    size_t otp_len = 0;
    int count = 0;
    int random_index;
    int i = 0;

    if (*offset > 0)
        return 0;

    mutex_lock(&otp_dev->list_mutex);

    if (list_empty(&otp_dev->otp_list_head)) {
        mutex_unlock(&otp_dev->list_mutex);
        return 0;
    }

    // Compter le nombre d'entrées
    list_for_each_entry(entry, &otp_dev->otp_list_head, list) {
        count++;
    }

    // Générer un index aléatoire
    if (count == 1) {
        random_index = 0;
    } else {
        get_random_bytes(&random_index, sizeof(random_index));
        random_index = abs(random_index) % count;
    }

    // Trouver l'entrée correspondant à random_index
    list_for_each_entry(entry, &otp_dev->otp_list_head, list) {
        if (i == random_index)
            break;
        i++;
    }

    // Copier le mot de passe dans le buffer
    otp_len = snprintf(otp_buf, sizeof(otp_buf), "%s\n", entry->password);

    mutex_unlock(&otp_dev->list_mutex);

    if (otp_len == 0)
        return 0;

    if (otp_len > len)
        otp_len = len;

    if (copy_to_user(buffer, otp_buf, otp_len))
        return -EFAULT;
    
    printk(KERN_INFO "otp: Mot de passe généré avec succès\n");

    *offset += otp_len;
    return otp_len;
}

static long otp_ioctl(struct file *filep, unsigned int cmd, unsigned long arg) {
    struct otp_device *otp_dev = filep->private_data;
    char kbuf[64];
    struct otp_entry *new_entry, *entry, *tmp;
    char __user *user_arg = (char __user *)arg;

    switch (cmd) {
        case OTP_IOC_ADD:
            if (copy_from_user(kbuf, user_arg, sizeof(kbuf) - 1))
                return -EFAULT;
            kbuf[sizeof(kbuf) - 1] = '\0';

            new_entry = kmalloc(sizeof(*new_entry), GFP_KERNEL);
            if (!new_entry)
                return -ENOMEM;

            strncpy(new_entry->password, kbuf, sizeof(new_entry->password) - 1);
            new_entry->password[sizeof(new_entry->password) - 1] = '\0';

            mutex_lock(&otp_dev->list_mutex);
            list_add_tail(&new_entry->list, &otp_dev->otp_list_head);
            mutex_unlock(&otp_dev->list_mutex);

            printk(KERN_INFO "otp: Mot de passe ajouté: %s\n", new_entry->password);
            break;

        case OTP_IOC_DEL:
            if (copy_from_user(kbuf, user_arg, sizeof(kbuf) - 1))
                return -EFAULT;
            kbuf[sizeof(kbuf) - 1] = '\0';

            mutex_lock(&otp_dev->list_mutex);
            list_for_each_entry_safe(entry, tmp, &otp_dev->otp_list_head, list) {
                if (strncmp(entry->password, kbuf, sizeof(entry->password)) == 0) {
                    list_del(&entry->list);
                    kfree(entry);
                    printk(KERN_INFO "otp: Mot de passe supprimé: %s\n", kbuf);
                    break;
                }
            }
            mutex_unlock(&otp_dev->list_mutex);
            break;

        case OTP_IOC_LIST: {
            char *list_buf;
            size_t buf_size = 1024;
            size_t pos = 0;
            struct otp_entry *e;

            list_buf = kmalloc(buf_size, GFP_KERNEL);
            if (!list_buf)
                return -ENOMEM;
            memset(list_buf, 0, buf_size);

            mutex_lock(&otp_dev->list_mutex);
            list_for_each_entry(e, &otp_dev->otp_list_head, list) {
                int n = snprintf(list_buf + pos, buf_size - pos, "%s\n", e->password);
                if (n < 0 || n >= (int)(buf_size - pos))
                    break;
                pos += n;
            }
            mutex_unlock(&otp_dev->list_mutex);

            if (copy_to_user(user_arg, list_buf, pos)) {
                kfree(list_buf);
                return -EFAULT;
            }

            kfree(list_buf);
            return pos;
        }

        default:
            return -EINVAL;
    }

    return 0;
}

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = otp_open,
    .read = otp_read,
    .unlocked_ioctl = otp_ioctl,
    .release = otp_release,
};

static int __init otp_init(void) {
    int ret, i;

    ret = alloc_chrdev_region(&dev_num_base, 0, MAX_DEVICES, "otpdev");
    if (ret < 0) {
        printk(KERN_ERR "otp: Impossible d'allouer un numéro majeur\n");
        return ret;
    }

    otp_class = class_create(DEVICE_CLASS);
    if (IS_ERR(otp_class)) {
        unregister_chrdev_region(dev_num_base, MAX_DEVICES);
        return PTR_ERR(otp_class);
    }

    otp_class->devnode = otp_devnode;

    for (i = 0; i < MAX_DEVICES; i++) {
        cdev_init(&otp_devices[i].cdev, &fops);
        otp_devices[i].cdev.owner = THIS_MODULE;

        ret = cdev_add(&otp_devices[i].cdev, dev_num_base + i, 1);
        if (ret < 0) {
            printk(KERN_ERR "otp: Impossible d'ajouter le cdev pour le device %d\n", i);
            while (--i >= 0) {
                device_destroy(otp_class, dev_num_base + i);
                cdev_del(&otp_devices[i].cdev);
            }
            class_destroy(otp_class);
            unregister_chrdev_region(dev_num_base, MAX_DEVICES);
            return ret;
        }

        INIT_LIST_HEAD(&otp_devices[i].otp_list_head);
        mutex_init(&otp_devices[i].list_mutex);

        otp_devices[i].device = device_create(otp_class, NULL, dev_num_base + i, NULL, "otpdev%d", i);
        if (IS_ERR(otp_devices[i].device)) {
            cdev_del(&otp_devices[i].cdev);
            while (--i >= 0) {
                device_destroy(otp_class, dev_num_base + i);
                cdev_del(&otp_devices[i].cdev);
            }
            class_destroy(otp_class);
            unregister_chrdev_region(dev_num_base, MAX_DEVICES);
            return PTR_ERR(otp_devices[i].device);
        }

        printk(KERN_INFO "otp: Device /dev/otpdev%d créé avec succès\n", i);
    }

    return 0;
}

static void __exit otp_exit(void) {
    int i;
    for (i = 0; i < MAX_DEVICES; i++) {
        device_destroy(otp_class, dev_num_base + i);
        cdev_del(&otp_devices[i].cdev);

        struct otp_entry *entry, *tmp;
        mutex_lock(&otp_devices[i].list_mutex);
        list_for_each_entry_safe(entry, tmp, &otp_devices[i].otp_list_head, list) {
            list_del(&entry->list);
            kfree(entry);
        }
        mutex_unlock(&otp_devices[i].list_mutex);
        mutex_destroy(&otp_devices[i].list_mutex);
    }

    class_destroy(otp_class);
    unregister_chrdev_region(dev_num_base, MAX_DEVICES);
    printk(KERN_INFO "otp: Module déchargé avec succès\n");
}

module_init(otp_init);
module_exit(otp_exit);
