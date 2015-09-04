/*
 * Video capture interface for Linux version 2
 *
 *	A generic video device interface for the LINUX operating system
 *	using a set of device structures/vectors for low level operations.
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License
 *	as published by the Free Software Foundation; either version
 *	2 of the License, or (at your option) any later version.
 *
 * Authors:	Alan Cox, <alan@lxorguk.ukuu.org.uk> (version 1)
 *              Mauro Carvalho Chehab <mchehab@infradead.org> (version 2)
 *
 * Fixes:	20000516  Claudio Matsuoka <claudio@conectiva.com>
 *		- Added procfs support
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/kmod.h>
#include <linux/slab.h>
#include <asm/uaccess.h>

#include <media/v4l2-common.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>

#define VIDEO_NUM_DEVICES	256
#define VIDEO_NAME              "video4linux"


static ssize_t show_index(struct device *cd,
			 struct device_attribute *attr, char *buf)
{
	struct video_device *vdev = to_video_device(cd);

	return sprintf(buf, "%i\n", vdev->index);
}

static ssize_t show_debug(struct device *cd,
			 struct device_attribute *attr, char *buf)
{
	struct video_device *vdev = to_video_device(cd);

	return sprintf(buf, "%i\n", vdev->debug);
}

static ssize_t set_debug(struct device *cd, struct device_attribute *attr,
		   const char *buf, size_t len)
{
	struct video_device *vdev = to_video_device(cd);
	int res = 0;
	u16 value;

	res = kstrtou16(buf, 0, &value);
	if (res)
		return res;

	vdev->debug = value;
	return len;
}

static ssize_t show_name(struct device *cd,
			 struct device_attribute *attr, char *buf)
{
	struct video_device *vdev = to_video_device(cd);

	return sprintf(buf, "%.*s\n", (int)sizeof(vdev->name), vdev->name);
}

static struct device_attribute video_device_attrs[] = {
	__ATTR(name, S_IRUGO, show_name, NULL),
	__ATTR(debug, 0644, show_debug, set_debug),
	__ATTR(index, S_IRUGO, show_index, NULL),
	__ATTR_NULL
};

static struct video_device *video_device[VIDEO_NUM_DEVICES];
static DEFINE_MUTEX(videodev_lock);
static DECLARE_BITMAP(devnode_nums[VFL_TYPE_MAX], VIDEO_NUM_DEVICES);



#ifdef CONFIG_VIDEO_FIXED_MINOR_RANGES
static inline unsigned long *devnode_bits(int vfl_type)
{
	int idx = (vfl_type > VFL_TYPE_RADIO) ? VFL_TYPE_MAX - 1 : vfl_type;

	return devnode_nums[idx];
}
#else
static inline unsigned long *devnode_bits(int vfl_type)
{
	return devnode_nums[vfl_type];
}
#endif

static inline void devnode_set(struct video_device *vdev)
{
	set_bit(vdev->num, devnode_bits(vdev->vfl_type));
}

static inline void devnode_clear(struct video_device *vdev)
{
	clear_bit(vdev->num, devnode_bits(vdev->vfl_type));
}

static inline int devnode_find(struct video_device *vdev, int from, int to)
{
	return find_next_zero_bit(devnode_bits(vdev->vfl_type), to, from);
}

struct video_device *video_device_alloc(void)
{
	return kzalloc(sizeof(struct video_device), GFP_KERNEL);
}
EXPORT_SYMBOL(video_device_alloc);

void video_device_release(struct video_device *vdev)
{
	kfree(vdev);
}
EXPORT_SYMBOL(video_device_release);

void video_device_release_empty(struct video_device *vdev)
{
	
	
}
EXPORT_SYMBOL(video_device_release_empty);

static inline void video_get(struct video_device *vdev)
{
	get_device(&vdev->dev);
}

static inline void video_put(struct video_device *vdev)
{
	put_device(&vdev->dev);
}

static void v4l2_device_release(struct device *cd)
{
	struct video_device *vdev = to_video_device(cd);
	struct v4l2_device *v4l2_dev = vdev->v4l2_dev;

	mutex_lock(&videodev_lock);
	if (WARN_ON(video_device[vdev->minor] != vdev)) {
		
		mutex_unlock(&videodev_lock);
		return;
	}

	
	video_device[vdev->minor] = NULL;

	
	cdev_del(vdev->cdev);
	vdev->cdev = NULL;

	
	devnode_clear(vdev);

	mutex_unlock(&videodev_lock);

#if defined(CONFIG_MEDIA_CONTROLLER)
	if (v4l2_dev && v4l2_dev->mdev &&
	    vdev->vfl_type != VFL_TYPE_SUBDEV)
		media_device_unregister_entity(&vdev->entity);
#endif

	if (v4l2_dev && v4l2_dev->release == NULL)
		v4l2_dev = NULL;

	vdev->release(vdev);

	
	if (v4l2_dev)
		v4l2_device_put(v4l2_dev);
}

static struct class video_class = {
	.name = VIDEO_NAME,
	.dev_attrs = video_device_attrs,
};

struct video_device *video_devdata(struct file *file)
{
	return video_device[iminor(file_inode(file))];
}
EXPORT_SYMBOL(video_devdata);



static inline bool prio_is_valid(enum v4l2_priority prio)
{
	return prio == V4L2_PRIORITY_BACKGROUND ||
	       prio == V4L2_PRIORITY_INTERACTIVE ||
	       prio == V4L2_PRIORITY_RECORD;
}

void v4l2_prio_init(struct v4l2_prio_state *global)
{
	memset(global, 0, sizeof(*global));
}
EXPORT_SYMBOL(v4l2_prio_init);

int v4l2_prio_change(struct v4l2_prio_state *global, enum v4l2_priority *local,
		     enum v4l2_priority new)
{
	if (!prio_is_valid(new))
		return -EINVAL;
	if (*local == new)
		return 0;

	atomic_inc(&global->prios[new]);
	if (prio_is_valid(*local))
		atomic_dec(&global->prios[*local]);
	*local = new;
	return 0;
}
EXPORT_SYMBOL(v4l2_prio_change);

void v4l2_prio_open(struct v4l2_prio_state *global, enum v4l2_priority *local)
{
	v4l2_prio_change(global, local, V4L2_PRIORITY_DEFAULT);
}
EXPORT_SYMBOL(v4l2_prio_open);

void v4l2_prio_close(struct v4l2_prio_state *global, enum v4l2_priority local)
{
	if (prio_is_valid(local))
		atomic_dec(&global->prios[local]);
}
EXPORT_SYMBOL(v4l2_prio_close);

enum v4l2_priority v4l2_prio_max(struct v4l2_prio_state *global)
{
	if (atomic_read(&global->prios[V4L2_PRIORITY_RECORD]) > 0)
		return V4L2_PRIORITY_RECORD;
	if (atomic_read(&global->prios[V4L2_PRIORITY_INTERACTIVE]) > 0)
		return V4L2_PRIORITY_INTERACTIVE;
	if (atomic_read(&global->prios[V4L2_PRIORITY_BACKGROUND]) > 0)
		return V4L2_PRIORITY_BACKGROUND;
	return V4L2_PRIORITY_UNSET;
}
EXPORT_SYMBOL(v4l2_prio_max);

int v4l2_prio_check(struct v4l2_prio_state *global, enum v4l2_priority local)
{
	return (local < v4l2_prio_max(global)) ? -EBUSY : 0;
}
EXPORT_SYMBOL(v4l2_prio_check);


static ssize_t v4l2_read(struct file *filp, char __user *buf,
		size_t sz, loff_t *off)
{
	struct video_device *vdev = video_devdata(filp);
	int ret = -ENODEV;

	if (!vdev->fops->read)
		return -EINVAL;
	if (video_is_registered(vdev))
		ret = vdev->fops->read(filp, buf, sz, off);
	if (vdev->debug)
		printk(KERN_DEBUG "%s: read: %zd (%d)\n",
			video_device_node_name(vdev), sz, ret);
	return ret;
}

static ssize_t v4l2_write(struct file *filp, const char __user *buf,
		size_t sz, loff_t *off)
{
	struct video_device *vdev = video_devdata(filp);
	int ret = -ENODEV;

	if (!vdev->fops->write)
		return -EINVAL;
	if (video_is_registered(vdev))
		ret = vdev->fops->write(filp, buf, sz, off);
	if (vdev->debug)
		printk(KERN_DEBUG "%s: write: %zd (%d)\n",
			video_device_node_name(vdev), sz, ret);
	return ret;
}

static unsigned int v4l2_poll(struct file *filp, struct poll_table_struct *poll)
{
	struct video_device *vdev = video_devdata(filp);
	unsigned int res = POLLERR | POLLHUP;

	if (!vdev->fops->poll)
		return DEFAULT_POLLMASK;
	if (video_is_registered(vdev))
		res = vdev->fops->poll(filp, poll);
	if (vdev->debug)
		printk(KERN_DEBUG "%s: poll: %08x\n",
			video_device_node_name(vdev), res);
	return res;
}

static long v4l2_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct video_device *vdev = video_devdata(filp);
	int ret = -ENODEV;

	if (vdev->fops->unlocked_ioctl) {
		struct mutex *lock = v4l2_ioctl_get_lock(vdev, cmd);

		if (lock && mutex_lock_interruptible(lock))
			return -ERESTARTSYS;
		if (video_is_registered(vdev))
			ret = vdev->fops->unlocked_ioctl(filp, cmd, arg);
		if (lock)
			mutex_unlock(lock);
	} else if (vdev->fops->ioctl) {
		static DEFINE_MUTEX(v4l2_ioctl_mutex);
		struct mutex *m = vdev->v4l2_dev ?
			&vdev->v4l2_dev->ioctl_lock : &v4l2_ioctl_mutex;

		if (cmd != VIDIOC_DQBUF && mutex_lock_interruptible(m))
			return -ERESTARTSYS;
		if (video_is_registered(vdev))
			ret = vdev->fops->ioctl(filp, cmd, arg);
		if (cmd != VIDIOC_DQBUF)
			mutex_unlock(m);
	} else
		ret = -ENOTTY;

	return ret;
}

#ifdef CONFIG_MMU
#define v4l2_get_unmapped_area NULL
#else
static unsigned long v4l2_get_unmapped_area(struct file *filp,
		unsigned long addr, unsigned long len, unsigned long pgoff,
		unsigned long flags)
{
	struct video_device *vdev = video_devdata(filp);
	int ret;

	if (!vdev->fops->get_unmapped_area)
		return -ENOSYS;
	if (!video_is_registered(vdev))
		return -ENODEV;
	ret = vdev->fops->get_unmapped_area(filp, addr, len, pgoff, flags);
	if (vdev->debug)
		printk(KERN_DEBUG "%s: get_unmapped_area (%d)\n",
			video_device_node_name(vdev), ret);
	return ret;
}
#endif

static int v4l2_mmap(struct file *filp, struct vm_area_struct *vm)
{
	struct video_device *vdev = video_devdata(filp);
	int ret = -ENODEV;

	if (!vdev->fops->mmap)
		return -ENODEV;
	if (video_is_registered(vdev))
		ret = vdev->fops->mmap(filp, vm);
	if (vdev->debug)
		printk(KERN_DEBUG "%s: mmap (%d)\n",
			video_device_node_name(vdev), ret);
	return ret;
}

static int v4l2_open(struct inode *inode, struct file *filp)
{
	struct video_device *vdev;
	int ret = 0;

	
	mutex_lock(&videodev_lock);
	vdev = video_devdata(filp);
	
	if (vdev == NULL || !video_is_registered(vdev)) {
		mutex_unlock(&videodev_lock);
		return -ENODEV;
	}
	
	video_get(vdev);
	mutex_unlock(&videodev_lock);
	if (vdev->fops->open) {
		if (video_is_registered(vdev))
			ret = vdev->fops->open(filp);
		else
			ret = -ENODEV;
	}

	if (vdev->debug)
		printk(KERN_DEBUG "%s: open (%d)\n",
			video_device_node_name(vdev), ret);
	
	if (ret)
		video_put(vdev);
	return ret;
}

static int v4l2_release(struct inode *inode, struct file *filp)
{
	struct video_device *vdev = video_devdata(filp);
	int ret = 0;

	if (vdev->fops->release)
		ret = vdev->fops->release(filp);
	if (vdev->debug)
		printk(KERN_DEBUG "%s: release\n",
			video_device_node_name(vdev));

	video_put(vdev);
	return ret;
}

static const struct file_operations v4l2_fops = {
	.owner = THIS_MODULE,
	.read = v4l2_read,
	.write = v4l2_write,
	.open = v4l2_open,
	.get_unmapped_area = v4l2_get_unmapped_area,
	.mmap = v4l2_mmap,
	.unlocked_ioctl = v4l2_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = v4l2_compat_ioctl32,
#endif
	.release = v4l2_release,
	.poll = v4l2_poll,
	.llseek = no_llseek,
};

static int get_index(struct video_device *vdev)
{
	static DECLARE_BITMAP(used, VIDEO_NUM_DEVICES);
	int i;

	
	if (vdev->parent == NULL)
		return 0;

	bitmap_zero(used, VIDEO_NUM_DEVICES);

	for (i = 0; i < VIDEO_NUM_DEVICES; i++) {
		if (video_device[i] != NULL &&
		    video_device[i]->parent == vdev->parent) {
			set_bit(video_device[i]->index, used);
		}
	}

	return find_first_zero_bit(used, VIDEO_NUM_DEVICES);
}

#define SET_VALID_IOCTL(ops, cmd, op)			\
	if (ops->op)					\
		set_bit(_IOC_NR(cmd), valid_ioctls)

static void determine_valid_ioctls(struct video_device *vdev)
{
	DECLARE_BITMAP(valid_ioctls, BASE_VIDIOC_PRIVATE);
	const struct v4l2_ioctl_ops *ops = vdev->ioctl_ops;
	bool is_vid = vdev->vfl_type == VFL_TYPE_GRABBER;
	bool is_vbi = vdev->vfl_type == VFL_TYPE_VBI;
	bool is_radio = vdev->vfl_type == VFL_TYPE_RADIO;
	bool is_rx = vdev->vfl_dir != VFL_DIR_TX;
	bool is_tx = vdev->vfl_dir != VFL_DIR_RX;

	bitmap_zero(valid_ioctls, BASE_VIDIOC_PRIVATE);

	

	SET_VALID_IOCTL(ops, VIDIOC_QUERYCAP, vidioc_querycap);
	if (ops->vidioc_g_priority ||
			test_bit(V4L2_FL_USE_FH_PRIO, &vdev->flags))
		set_bit(_IOC_NR(VIDIOC_G_PRIORITY), valid_ioctls);
	if (ops->vidioc_s_priority ||
			test_bit(V4L2_FL_USE_FH_PRIO, &vdev->flags))
		set_bit(_IOC_NR(VIDIOC_S_PRIORITY), valid_ioctls);
	SET_VALID_IOCTL(ops, VIDIOC_STREAMON, vidioc_streamon);
	SET_VALID_IOCTL(ops, VIDIOC_STREAMOFF, vidioc_streamoff);
	if (vdev->ctrl_handler || ops->vidioc_queryctrl)
		set_bit(_IOC_NR(VIDIOC_QUERYCTRL), valid_ioctls);
	if (vdev->ctrl_handler || ops->vidioc_g_ctrl || ops->vidioc_g_ext_ctrls)
		set_bit(_IOC_NR(VIDIOC_G_CTRL), valid_ioctls);
	if (vdev->ctrl_handler || ops->vidioc_s_ctrl || ops->vidioc_s_ext_ctrls)
		set_bit(_IOC_NR(VIDIOC_S_CTRL), valid_ioctls);
	if (vdev->ctrl_handler || ops->vidioc_g_ext_ctrls)
		set_bit(_IOC_NR(VIDIOC_G_EXT_CTRLS), valid_ioctls);
	if (vdev->ctrl_handler || ops->vidioc_s_ext_ctrls)
		set_bit(_IOC_NR(VIDIOC_S_EXT_CTRLS), valid_ioctls);
	if (vdev->ctrl_handler || ops->vidioc_try_ext_ctrls)
		set_bit(_IOC_NR(VIDIOC_TRY_EXT_CTRLS), valid_ioctls);
	if (vdev->ctrl_handler || ops->vidioc_querymenu)
		set_bit(_IOC_NR(VIDIOC_QUERYMENU), valid_ioctls);
	SET_VALID_IOCTL(ops, VIDIOC_G_FREQUENCY, vidioc_g_frequency);
	SET_VALID_IOCTL(ops, VIDIOC_S_FREQUENCY, vidioc_s_frequency);
	SET_VALID_IOCTL(ops, VIDIOC_LOG_STATUS, vidioc_log_status);
#ifdef CONFIG_VIDEO_ADV_DEBUG
	set_bit(_IOC_NR(VIDIOC_DBG_G_CHIP_INFO), valid_ioctls);
	set_bit(_IOC_NR(VIDIOC_DBG_G_REGISTER), valid_ioctls);
	set_bit(_IOC_NR(VIDIOC_DBG_S_REGISTER), valid_ioctls);
#endif
	SET_VALID_IOCTL(ops, VIDIOC_DBG_G_CHIP_IDENT, vidioc_g_chip_ident);
	
	SET_VALID_IOCTL(ops, VIDIOC_DQEVENT, vidioc_subscribe_event);
	SET_VALID_IOCTL(ops, VIDIOC_SUBSCRIBE_EVENT, vidioc_subscribe_event);
	SET_VALID_IOCTL(ops, VIDIOC_UNSUBSCRIBE_EVENT, vidioc_unsubscribe_event);
	if (ops->vidioc_enum_freq_bands || ops->vidioc_g_tuner || ops->vidioc_g_modulator)
		set_bit(_IOC_NR(VIDIOC_ENUM_FREQ_BANDS), valid_ioctls);

	if (is_vid) {
		
		if ((is_rx && (ops->vidioc_enum_fmt_vid_cap ||
			       ops->vidioc_enum_fmt_vid_cap_mplane ||
			       ops->vidioc_enum_fmt_vid_overlay)) ||
		    (is_tx && (ops->vidioc_enum_fmt_vid_out ||
			       ops->vidioc_enum_fmt_vid_out_mplane)))
			set_bit(_IOC_NR(VIDIOC_ENUM_FMT), valid_ioctls);
		if ((is_rx && (ops->vidioc_g_fmt_vid_cap ||
			       ops->vidioc_g_fmt_vid_cap_mplane ||
			       ops->vidioc_g_fmt_vid_overlay)) ||
		    (is_tx && (ops->vidioc_g_fmt_vid_out ||
			       ops->vidioc_g_fmt_vid_out_mplane ||
			       ops->vidioc_g_fmt_vid_out_overlay)))
			 set_bit(_IOC_NR(VIDIOC_G_FMT), valid_ioctls);
		if ((is_rx && (ops->vidioc_s_fmt_vid_cap ||
			       ops->vidioc_s_fmt_vid_cap_mplane ||
			       ops->vidioc_s_fmt_vid_overlay)) ||
		    (is_tx && (ops->vidioc_s_fmt_vid_out ||
			       ops->vidioc_s_fmt_vid_out_mplane ||
			       ops->vidioc_s_fmt_vid_out_overlay)))
			 set_bit(_IOC_NR(VIDIOC_S_FMT), valid_ioctls);
		if ((is_rx && (ops->vidioc_try_fmt_vid_cap ||
			       ops->vidioc_try_fmt_vid_cap_mplane ||
			       ops->vidioc_try_fmt_vid_overlay)) ||
		    (is_tx && (ops->vidioc_try_fmt_vid_out ||
			       ops->vidioc_try_fmt_vid_out_mplane ||
			       ops->vidioc_try_fmt_vid_out_overlay)))
			 set_bit(_IOC_NR(VIDIOC_TRY_FMT), valid_ioctls);
		SET_VALID_IOCTL(ops, VIDIOC_OVERLAY, vidioc_overlay);
		SET_VALID_IOCTL(ops, VIDIOC_G_FBUF, vidioc_g_fbuf);
		SET_VALID_IOCTL(ops, VIDIOC_S_FBUF, vidioc_s_fbuf);
		SET_VALID_IOCTL(ops, VIDIOC_G_JPEGCOMP, vidioc_g_jpegcomp);
		SET_VALID_IOCTL(ops, VIDIOC_S_JPEGCOMP, vidioc_s_jpegcomp);
		SET_VALID_IOCTL(ops, VIDIOC_G_ENC_INDEX, vidioc_g_enc_index);
		SET_VALID_IOCTL(ops, VIDIOC_ENCODER_CMD, vidioc_encoder_cmd);
		SET_VALID_IOCTL(ops, VIDIOC_TRY_ENCODER_CMD, vidioc_try_encoder_cmd);
		SET_VALID_IOCTL(ops, VIDIOC_DECODER_CMD, vidioc_decoder_cmd);
		SET_VALID_IOCTL(ops, VIDIOC_TRY_DECODER_CMD, vidioc_try_decoder_cmd);
		SET_VALID_IOCTL(ops, VIDIOC_ENUM_FRAMESIZES, vidioc_enum_framesizes);
		SET_VALID_IOCTL(ops, VIDIOC_ENUM_FRAMEINTERVALS, vidioc_enum_frameintervals);
	} else if (is_vbi) {
		
		if ((is_rx && (ops->vidioc_g_fmt_vbi_cap ||
			       ops->vidioc_g_fmt_sliced_vbi_cap)) ||
		    (is_tx && (ops->vidioc_g_fmt_vbi_out ||
			       ops->vidioc_g_fmt_sliced_vbi_out)))
			set_bit(_IOC_NR(VIDIOC_G_FMT), valid_ioctls);
		if ((is_rx && (ops->vidioc_s_fmt_vbi_cap ||
			       ops->vidioc_s_fmt_sliced_vbi_cap)) ||
		    (is_tx && (ops->vidioc_s_fmt_vbi_out ||
			       ops->vidioc_s_fmt_sliced_vbi_out)))
			set_bit(_IOC_NR(VIDIOC_S_FMT), valid_ioctls);
		if ((is_rx && (ops->vidioc_try_fmt_vbi_cap ||
			       ops->vidioc_try_fmt_sliced_vbi_cap)) ||
		    (is_tx && (ops->vidioc_try_fmt_vbi_out ||
			       ops->vidioc_try_fmt_sliced_vbi_out)))
			set_bit(_IOC_NR(VIDIOC_TRY_FMT), valid_ioctls);
		SET_VALID_IOCTL(ops, VIDIOC_G_SLICED_VBI_CAP, vidioc_g_sliced_vbi_cap);
	}
	if (!is_radio) {
		
		SET_VALID_IOCTL(ops, VIDIOC_REQBUFS, vidioc_reqbufs);
		SET_VALID_IOCTL(ops, VIDIOC_QUERYBUF, vidioc_querybuf);
		SET_VALID_IOCTL(ops, VIDIOC_QBUF, vidioc_qbuf);
		SET_VALID_IOCTL(ops, VIDIOC_EXPBUF, vidioc_expbuf);
		SET_VALID_IOCTL(ops, VIDIOC_DQBUF, vidioc_dqbuf);
		SET_VALID_IOCTL(ops, VIDIOC_CREATE_BUFS, vidioc_create_bufs);
		SET_VALID_IOCTL(ops, VIDIOC_PREPARE_BUF, vidioc_prepare_buf);
		if (ops->vidioc_s_std)
			set_bit(_IOC_NR(VIDIOC_ENUMSTD), valid_ioctls);
		if (ops->vidioc_g_std || vdev->current_norm)
			set_bit(_IOC_NR(VIDIOC_G_STD), valid_ioctls);
		SET_VALID_IOCTL(ops, VIDIOC_S_STD, vidioc_s_std);
		if (is_rx) {
			SET_VALID_IOCTL(ops, VIDIOC_QUERYSTD, vidioc_querystd);
			SET_VALID_IOCTL(ops, VIDIOC_ENUMINPUT, vidioc_enum_input);
			SET_VALID_IOCTL(ops, VIDIOC_G_INPUT, vidioc_g_input);
			SET_VALID_IOCTL(ops, VIDIOC_S_INPUT, vidioc_s_input);
			SET_VALID_IOCTL(ops, VIDIOC_ENUMAUDIO, vidioc_enumaudio);
			SET_VALID_IOCTL(ops, VIDIOC_G_AUDIO, vidioc_g_audio);
			SET_VALID_IOCTL(ops, VIDIOC_S_AUDIO, vidioc_s_audio);
			SET_VALID_IOCTL(ops, VIDIOC_QUERY_DV_TIMINGS, vidioc_query_dv_timings);
		}
		if (is_tx) {
			SET_VALID_IOCTL(ops, VIDIOC_ENUMOUTPUT, vidioc_enum_output);
			SET_VALID_IOCTL(ops, VIDIOC_G_OUTPUT, vidioc_g_output);
			SET_VALID_IOCTL(ops, VIDIOC_S_OUTPUT, vidioc_s_output);
			SET_VALID_IOCTL(ops, VIDIOC_ENUMAUDOUT, vidioc_enumaudout);
			SET_VALID_IOCTL(ops, VIDIOC_G_AUDOUT, vidioc_g_audout);
			SET_VALID_IOCTL(ops, VIDIOC_S_AUDOUT, vidioc_s_audout);
		}
		if (ops->vidioc_g_crop || ops->vidioc_g_selection)
			set_bit(_IOC_NR(VIDIOC_G_CROP), valid_ioctls);
		if (ops->vidioc_s_crop || ops->vidioc_s_selection)
			set_bit(_IOC_NR(VIDIOC_S_CROP), valid_ioctls);
		SET_VALID_IOCTL(ops, VIDIOC_G_SELECTION, vidioc_g_selection);
		SET_VALID_IOCTL(ops, VIDIOC_S_SELECTION, vidioc_s_selection);
		if (ops->vidioc_cropcap || ops->vidioc_g_selection)
			set_bit(_IOC_NR(VIDIOC_CROPCAP), valid_ioctls);
		if (ops->vidioc_g_parm || (vdev->vfl_type == VFL_TYPE_GRABBER &&
					(ops->vidioc_g_std || vdev->current_norm)))
			set_bit(_IOC_NR(VIDIOC_G_PARM), valid_ioctls);
		SET_VALID_IOCTL(ops, VIDIOC_S_PARM, vidioc_s_parm);
		SET_VALID_IOCTL(ops, VIDIOC_S_DV_TIMINGS, vidioc_s_dv_timings);
		SET_VALID_IOCTL(ops, VIDIOC_G_DV_TIMINGS, vidioc_g_dv_timings);
		SET_VALID_IOCTL(ops, VIDIOC_ENUM_DV_TIMINGS, vidioc_enum_dv_timings);
		SET_VALID_IOCTL(ops, VIDIOC_DV_TIMINGS_CAP, vidioc_dv_timings_cap);
	} else {
		
		SET_VALID_IOCTL(ops, VIDIOC_DQBUF, vidioc_dqbuf);
	}
	if (is_tx) {
		
		SET_VALID_IOCTL(ops, VIDIOC_G_MODULATOR, vidioc_g_modulator);
		SET_VALID_IOCTL(ops, VIDIOC_S_MODULATOR, vidioc_s_modulator);
	}
	if (is_rx) {
		
		SET_VALID_IOCTL(ops, VIDIOC_G_TUNER, vidioc_g_tuner);
		SET_VALID_IOCTL(ops, VIDIOC_S_TUNER, vidioc_s_tuner);
		SET_VALID_IOCTL(ops, VIDIOC_S_HW_FREQ_SEEK, vidioc_s_hw_freq_seek);
	}

	bitmap_andnot(vdev->valid_ioctls, valid_ioctls, vdev->valid_ioctls,
			BASE_VIDIOC_PRIVATE);
}

int __video_register_device(struct video_device *vdev, int type, int nr,
		int warn_if_nr_in_use, struct module *owner)
{
	int i = 0;
	int ret;
	int minor_offset = 0;
	int minor_cnt = VIDEO_NUM_DEVICES;
	const char *name_base;

	vdev->minor = -1;

	
	if (WARN_ON(!vdev->release))
		return -EINVAL;

	
	spin_lock_init(&vdev->fh_lock);
	INIT_LIST_HEAD(&vdev->fh_list);

	
	switch (type) {
	case VFL_TYPE_GRABBER:
		name_base = "video";
		break;
	case VFL_TYPE_VBI:
		name_base = "vbi";
		break;
	case VFL_TYPE_RADIO:
		name_base = "radio";
		break;
	case VFL_TYPE_SUBDEV:
		name_base = "v4l-subdev";
		break;
	default:
		printk(KERN_ERR "%s called with unknown type: %d\n",
		       __func__, type);
		return -EINVAL;
	}

	vdev->vfl_type = type;
	vdev->cdev = NULL;
	if (vdev->v4l2_dev) {
		if (vdev->v4l2_dev->dev)
			vdev->parent = vdev->v4l2_dev->dev;
		if (vdev->ctrl_handler == NULL)
			vdev->ctrl_handler = vdev->v4l2_dev->ctrl_handler;
		if (vdev->prio == NULL)
			vdev->prio = &vdev->v4l2_dev->prio;
	}

	
#ifdef CONFIG_VIDEO_FIXED_MINOR_RANGES
	switch (type) {
	case VFL_TYPE_GRABBER:
		minor_offset = 0;
		minor_cnt = 64;
		break;
	case VFL_TYPE_RADIO:
		minor_offset = 64;
		minor_cnt = 64;
		break;
	case VFL_TYPE_VBI:
		minor_offset = 224;
		minor_cnt = 32;
		break;
	default:
		minor_offset = 128;
		minor_cnt = 64;
		break;
	}
#endif

	
	mutex_lock(&videodev_lock);
	nr = devnode_find(vdev, nr == -1 ? 0 : nr, minor_cnt);
	if (nr == minor_cnt)
		nr = devnode_find(vdev, 0, minor_cnt);
	if (nr == minor_cnt) {
		printk(KERN_ERR "could not get a free device node number\n");
		mutex_unlock(&videodev_lock);
		return -ENFILE;
	}
#ifdef CONFIG_VIDEO_FIXED_MINOR_RANGES
	
	i = nr;
#else
	for (i = 0; i < VIDEO_NUM_DEVICES; i++)
		if (video_device[i] == NULL)
			break;
	if (i == VIDEO_NUM_DEVICES) {
		mutex_unlock(&videodev_lock);
		printk(KERN_ERR "could not get a free minor\n");
		return -ENFILE;
	}
#endif
	vdev->minor = i + minor_offset;
	vdev->num = nr;
	devnode_set(vdev);

	
	WARN_ON(video_device[vdev->minor] != NULL);
	vdev->index = get_index(vdev);
	mutex_unlock(&videodev_lock);

	if (vdev->ioctl_ops)
		determine_valid_ioctls(vdev);

	
	vdev->cdev = cdev_alloc();
	if (vdev->cdev == NULL) {
		ret = -ENOMEM;
		goto cleanup;
	}
	vdev->cdev->ops = &v4l2_fops;
	vdev->cdev->owner = owner;
	ret = cdev_add(vdev->cdev, MKDEV(VIDEO_MAJOR, vdev->minor), 1);
	if (ret < 0) {
		printk(KERN_ERR "%s: cdev_add failed\n", __func__);
		kfree(vdev->cdev);
		vdev->cdev = NULL;
		goto cleanup;
	}

	
	vdev->dev.class = &video_class;
	vdev->dev.devt = MKDEV(VIDEO_MAJOR, vdev->minor);
	if (vdev->parent)
		vdev->dev.parent = vdev->parent;
	dev_set_name(&vdev->dev, "%s%d", name_base, vdev->num);
	ret = device_register(&vdev->dev);
	if (ret < 0) {
		printk(KERN_ERR "%s: device_register failed\n", __func__);
		goto cleanup;
	}
	vdev->dev.release = v4l2_device_release;

	if (nr != -1 && nr != vdev->num && warn_if_nr_in_use)
		printk(KERN_WARNING "%s: requested %s%d, got %s\n", __func__,
			name_base, nr, video_device_node_name(vdev));

	
	if (vdev->v4l2_dev)
		v4l2_device_get(vdev->v4l2_dev);

#if defined(CONFIG_MEDIA_CONTROLLER)
	
	if (vdev->v4l2_dev && vdev->v4l2_dev->mdev &&
	    vdev->vfl_type != VFL_TYPE_SUBDEV) {
		vdev->entity.type = MEDIA_ENT_T_DEVNODE_V4L;
		vdev->entity.name = vdev->name;
		vdev->entity.info.v4l.major = VIDEO_MAJOR;
		vdev->entity.info.v4l.minor = vdev->minor;
		ret = media_device_register_entity(vdev->v4l2_dev->mdev,
			&vdev->entity);
		if (ret < 0)
			printk(KERN_WARNING
			       "%s: media_device_register_entity failed\n",
			       __func__);
	}
#endif
	
	set_bit(V4L2_FL_REGISTERED, &vdev->flags);
	mutex_lock(&videodev_lock);
	video_device[vdev->minor] = vdev;
	mutex_unlock(&videodev_lock);

	return 0;

cleanup:
	mutex_lock(&videodev_lock);
	if (vdev->cdev)
		cdev_del(vdev->cdev);
	devnode_clear(vdev);
	mutex_unlock(&videodev_lock);
	
	vdev->minor = -1;
	return ret;
}
EXPORT_SYMBOL(__video_register_device);

void video_unregister_device(struct video_device *vdev)
{
	
	if (!vdev || !video_is_registered(vdev))
		return;

	mutex_lock(&videodev_lock);
	clear_bit(V4L2_FL_REGISTERED, &vdev->flags);
	mutex_unlock(&videodev_lock);
	device_unregister(&vdev->dev);
}
EXPORT_SYMBOL(video_unregister_device);

static int __init videodev_init(void)
{
	dev_t dev = MKDEV(VIDEO_MAJOR, 0);
	int ret;

	printk(KERN_INFO "Linux video capture interface: v2.00\n");
	ret = register_chrdev_region(dev, VIDEO_NUM_DEVICES, VIDEO_NAME);
	if (ret < 0) {
		printk(KERN_WARNING "videodev: unable to get major %d\n",
				VIDEO_MAJOR);
		return ret;
	}

	ret = class_register(&video_class);
	if (ret < 0) {
		unregister_chrdev_region(dev, VIDEO_NUM_DEVICES);
		printk(KERN_WARNING "video_dev: class_register failed\n");
		return -EIO;
	}

	return 0;
}

static void __exit videodev_exit(void)
{
	dev_t dev = MKDEV(VIDEO_MAJOR, 0);

	class_unregister(&video_class);
	unregister_chrdev_region(dev, VIDEO_NUM_DEVICES);
}

subsys_initcall(videodev_init);
module_exit(videodev_exit)

MODULE_AUTHOR("Alan Cox, Mauro Carvalho Chehab <mchehab@infradead.org>");
MODULE_DESCRIPTION("Device registrar for Video4Linux drivers v2");
MODULE_LICENSE("GPL");
MODULE_ALIAS_CHARDEV_MAJOR(VIDEO_MAJOR);


