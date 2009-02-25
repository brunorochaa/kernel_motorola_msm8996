/*
 * Plan 9 style capability device implementation for the Linux Kernel
 *
 * Copyright 2008, 2009 Ashwin Ganti <ashwin.ganti@gmail.com>
 *
 * Released under the GPLv2
 *
 */
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/proc_fs.h>
#include <linux/fcntl.h>
#include <linux/cdev.h>
#include <linux/syscalls.h>
#include <asm/system.h>
#include <asm/uaccess.h>
#include <linux/list.h>
#include <linux/err.h>
#include <linux/mm.h>
#include <linux/string.h>
#include <linux/crypto.h>
#include <linux/highmem.h>
#include <linux/jiffies.h>
#include <linux/timex.h>
#include <linux/interrupt.h>
#include <linux/scatterlist.h>
#include <linux/crypto.h>
#include <linux/sched.h>
#include <linux/cred.h>

#ifndef CAP_MAJOR
#define CAP_MAJOR 0
#endif

#ifndef CAP_NR_DEVS
#define CAP_NR_DEVS 2		/* caphash and capuse */
#endif

#ifndef CAP_NODE_SIZE
#define CAP_NODE_SIZE 20
#endif

#define MAX_DIGEST_SIZE  20

struct cap_node {
	char data[CAP_NODE_SIZE];
	struct list_head list;
};

struct cap_dev {
	struct cap_node *head;
	int node_size;
	unsigned long size;
	struct semaphore sem;
	struct cdev cdev;
};

int cap_trim(struct cap_dev *);
ssize_t cap_write(struct file *, const char __user *, size_t, loff_t *);
char *cap_hash(char *plain_text, unsigned int plain_text_size, char *key, unsigned int key_size);
void hex_dump(unsigned char * buf, unsigned int len);

int cap_major = CAP_MAJOR;
int cap_minor = 0;
int cap_nr_devs = CAP_NR_DEVS;
int cap_node_size = CAP_NODE_SIZE;

module_param(cap_major, int, S_IRUGO);
module_param(cap_minor, int, S_IRUGO);
module_param(cap_nr_devs, int, S_IRUGO);

MODULE_AUTHOR("Ashwin Ganti");
MODULE_LICENSE("GPL");

struct cap_dev *cap_devices;

void hexdump(unsigned char *buf, unsigned int len)
{
	while (len--)
		printk("%02x", *buf++);
	printk("\n");
}

int cap_trim(struct cap_dev *dev)
{
	struct cap_node *tmp;
	struct list_head *pos, *q;
	if (dev->head != NULL) {
		list_for_each_safe(pos, q, &(dev->head->list)) {
			tmp = list_entry(pos, struct cap_node, list);
			list_del(pos);
			kfree(tmp);
		}
	}
	return 0;
}

int cap_open(struct inode *inode, struct file *filp)
{
	struct cap_dev *dev;
	dev = container_of(inode->i_cdev, struct cap_dev, cdev);
	filp->private_data = dev;

	/* trim to 0 the length of the device if open was write-only */
	if ((filp->f_flags & O_ACCMODE) == O_WRONLY) {
		if (down_interruptible(&dev->sem))
			return -ERESTARTSYS;
		cap_trim(dev);
		up(&dev->sem);
	}
	/* initialise the head if it is NULL */
	if (dev->head == NULL) {
		dev->head =
		    (struct cap_node *) kmalloc(sizeof(struct cap_node),
						GFP_KERNEL);
		INIT_LIST_HEAD(&(dev->head->list));
	}
	return 0;
}

int cap_release(struct inode *inode, struct file *filp)
{
	return 0;
}

ssize_t
cap_write(struct file * filp, const char __user * buf,
	  size_t count, loff_t * f_pos)
{
	struct cap_node *node_ptr, *tmp;
	struct list_head *pos;
	struct cap_dev *dev = filp->private_data;
	ssize_t retval = -ENOMEM;
	struct cred *new;
	int len, target_int, source_int, flag = 0;
	char *user_buf, *user_buf_running, *source_user, *target_user,
	    *rand_str, *hash_str, *result;

	if (down_interruptible(&dev->sem))
		return -ERESTARTSYS;

	node_ptr =
	    (struct cap_node *) kmalloc(sizeof(struct cap_node),
					GFP_KERNEL);
	user_buf = (char *) kmalloc(count, GFP_KERNEL);
	memset(user_buf, 0, count);

	if (copy_from_user(user_buf, buf, count)) {
		retval = -EFAULT;
		goto out;
	}

	/* If the minor number is 0 ( /dev/caphash ) then simply add the
	 * hashed capability supplied by the user to the list of hashes
	 */
	if (0 == iminor(filp->f_dentry->d_inode)) {
		printk(KERN_INFO "Capability being written to /dev/caphash : \n");
		hexdump(user_buf, count);
		memcpy(node_ptr->data, user_buf, count);
		list_add(&(node_ptr->list), &(dev->head->list));
	} else {
		/* break the supplied string into tokens with @ as the delimiter
		   If the string is "user1@user2@randomstring" we need to split it
		   and hash 'user1@user2' using 'randomstring' as the key
		 */
		user_buf_running = kstrdup(user_buf, GFP_KERNEL);
		source_user = strsep(&user_buf_running, "@");
		target_user = strsep(&user_buf_running, "@");
		rand_str = strsep(&user_buf_running, "@");

		/* hash the string user1@user2 with rand_str as the key */
		len = strlen(source_user) + strlen(target_user) + 1;
		hash_str = (char *) kmalloc(len, GFP_KERNEL);
		memset(hash_str, 0, len);
		strcat(hash_str, source_user);
		strcat(hash_str, "@");
		strcat(hash_str, target_user);

		printk(KERN_ALERT "the source user is %s \n", source_user);
		printk(KERN_ALERT "the target user is %s \n", target_user);

		result =
		    cap_hash(hash_str, len, rand_str, strlen(rand_str));
		if (NULL == result) {
			retval = -EFAULT;
			goto out;
		}
		memcpy(node_ptr->data, result, CAP_NODE_SIZE);
		/* Change the process's uid if the hash is present in the
		 * list of hashes
		 */
		list_for_each(pos, &(cap_devices->head->list)) {
			/* Change the user id of the process if the hashes match  */
			if (0 ==
			    memcmp(result,
				   list_entry(pos, struct cap_node,
					      list)->data,
				   CAP_NODE_SIZE)) {
				target_int = (unsigned int)
				    simple_strtol(target_user, NULL, 0);
				source_int = (unsigned int)
				    simple_strtol(source_user, NULL, 0);
				flag = 1;

				/* Check whether the process writing to capuse is actually owned by
				 * the source owner
				 */
				if (source_int != current_uid()) {
					printk(KERN_ALERT
					       "Process is not owned by the source user of the capability.\n");
					retval = -EFAULT;
					goto out;
				}
				/* What all id's need to be changed here? uid, euid, fsid, savedids ??
				 * Currently I am changing the effective user id
				 * since most of the authorisation decisions are based on it
				 */
				new = prepare_creds();
				if (!new) {
					retval = -ENOMEM;
					goto out;
				}
				new->uid = (uid_t) target_int;
				new->euid = (uid_t) target_int;
				retval = commit_creds(new);
				if (retval)
					goto out;

				/* Remove the capability from the list and break */
				tmp =
				    list_entry(pos, struct cap_node, list);
				list_del(pos);
				kfree(tmp);
				break;
			}
		}
		if (0 == flag) {
			/* The capability is not present in the list of the hashes stored, hence return failure */
			printk(KERN_ALERT
			       "Invalid capabiliy written to /dev/capuse \n");
			retval = -EFAULT;
			goto out;
		}
	}
	*f_pos += count;
	retval = count;
	/* update the size */
	if (dev->size < *f_pos)
		dev->size = *f_pos;

      out:
	up(&dev->sem);
	return retval;
}

struct file_operations cap_fops = {
	.owner = THIS_MODULE,
	.write = cap_write,
	.open = cap_open,
	.release = cap_release,
};


void cap_cleanup_module(void)
{
	int i;
	dev_t devno = MKDEV(cap_major, cap_minor);
	if (cap_devices) {
		for (i = 0; i < cap_nr_devs; i++) {
			cap_trim(cap_devices + i);
			cdev_del(&cap_devices[i].cdev);
		}
		kfree(cap_devices);
	}
	unregister_chrdev_region(devno, cap_nr_devs);

}


static void cap_setup_cdev(struct cap_dev *dev, int index)
{
	int err, devno = MKDEV(cap_major, cap_minor + index);
	cdev_init(&dev->cdev, &cap_fops);
	dev->cdev.owner = THIS_MODULE;
	dev->cdev.ops = &cap_fops;
	err = cdev_add(&dev->cdev, devno, 1);
	if (err)
		printk(KERN_NOTICE "Error %d adding cap%d", err, index);
}


int cap_init_module(void)
{
	int result, i;
	dev_t dev = 0;

	if (cap_major) {
		dev = MKDEV(cap_major, cap_minor);
		result = register_chrdev_region(dev, cap_nr_devs, "cap");
	} else {
		result = alloc_chrdev_region(&dev, cap_minor, cap_nr_devs,
					     "cap");
		cap_major = MAJOR(dev);
	}

	if (result < 0) {
		printk(KERN_WARNING "cap: can't get major %d\n",
		       cap_major);
		return result;
	}

	cap_devices =
	    kmalloc(cap_nr_devs * sizeof(struct cap_dev), GFP_KERNEL);
	if (!cap_devices) {
		result = -ENOMEM;
		goto fail;
	}
	memset(cap_devices, 0, cap_nr_devs * sizeof(struct cap_dev));

	/* Initialize each device. */
	for (i = 0; i < cap_nr_devs; i++) {
		cap_devices[i].node_size = cap_node_size;
		init_MUTEX(&cap_devices[i].sem);
		cap_setup_cdev(&cap_devices[i], i);
	}

	return 0;

      fail:
	cap_cleanup_module();
	return result;
}

module_init(cap_init_module);
module_exit(cap_cleanup_module);

char *cap_hash(char *plain_text, unsigned int plain_text_size,
	       char *key, unsigned int key_size)
{
	struct scatterlist sg;
	char *result = (char *) kmalloc(MAX_DIGEST_SIZE, GFP_KERNEL);
	struct crypto_hash *tfm;
	struct hash_desc desc;
	int ret;

	tfm = crypto_alloc_hash("hmac(sha1)", 0, CRYPTO_ALG_ASYNC);
	if (IS_ERR(tfm)) {
		printk("failed to load transform for hmac(sha1): %ld\n",
		       PTR_ERR(tfm));
		kfree(result);
		return NULL;
	}

	desc.tfm = tfm;
	desc.flags = 0;

	memset(result, 0, MAX_DIGEST_SIZE);
	sg_set_buf(&sg, plain_text, plain_text_size);

	ret = crypto_hash_setkey(tfm, key, key_size);
	if (ret) {
		printk("setkey() failed ret=%d\n", ret);
		kfree(result);
		result = NULL;
		goto out;
	}

	ret = crypto_hash_digest(&desc, &sg, plain_text_size, result);
	if (ret) {
		printk("digest () failed ret=%d\n", ret);
		kfree(result);
		result = NULL;
		goto out;
	}

	printk("crypto hash digest size %d\n",
	       crypto_hash_digestsize(tfm));
	hexdump(result, MAX_DIGEST_SIZE);

      out:
	crypto_free_hash(tfm);
	return result;
}
