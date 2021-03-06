#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/mutex.h>
#include <linux/unistd.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/slab.h>
#include <linux/pm.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/kthread.h>
#include <linux/freezer.h>
#include <linux/timer.h>
#include <linux/version.h>
#include <linux/mtd/hisi_nve_interface.h>
#include <huawei_platform/log/hw_log.h>

#include "ams_tcs3430.h"

#ifdef HWLOG_TAG
#undef HWLOG_TAG
#endif
#define HWLOG_TAG color_sensor
#define TOTAL_GAIN_NUM 5
#define DATA_NUM_PER_GAIN 4
#define TIME_OUT_DEFAULT 1500
#define ONE_SHOW_LEN     10

HWLOG_REGIST();

#define NV_READ_TAG	1
#define NV_WRITE_TAG	0

static struct class *color_sensor_class;
struct hisi_nve_info_user nv_user_info;

int read_color_calibrate_data_from_nv(int nv_number, int nv_size, char *nv_name, char *temp)
{
	int ret = 0;
	if(NULL == nv_name || NULL == temp){
		hwlog_err("read_color_calibrate_data_from_nv INPUT PARA NULL\n");
		return -1;
	}
	hwlog_info("read_color_calibrate_data_from_nv %d in!!\n", nv_number);
	memset(&nv_user_info, 0, sizeof(nv_user_info));
	nv_user_info.nv_operation = NV_READ_TAG;
	nv_user_info.nv_number = nv_number;
	nv_user_info.valid_size = nv_size;
	strncpy(nv_user_info.nv_name, nv_name, sizeof(nv_user_info.nv_name));
	nv_user_info.nv_name[sizeof(nv_user_info.nv_name) - 1] = '\0';
	if ((ret = hisi_nve_direct_access(&nv_user_info))!=0)
	{
		hwlog_err("hisi_nve_direct_access read nv %d error(%d)\n", nv_number, ret);
		return -1;
	}
	memcpy(temp, nv_user_info.nv_data, sizeof(nv_user_info.nv_data) < nv_user_info.valid_size ?
		sizeof(nv_user_info.nv_data) : nv_user_info.valid_size);
	return 0;
}
EXPORT_SYMBOL_GPL(read_color_calibrate_data_from_nv);

int write_color_calibrate_data_to_nv(int nv_number, int nv_size, char *nv_name, char *temp)
{
	int ret = 0;
	if(NULL == nv_name || NULL == temp){
		hwlog_err("write_color_calibrate_data_to_nv INPUT PARA NULL\n");
		return -1;
	}
	hwlog_info("write_color_calibrate_data_to_nv %d in!!\n", nv_number);
	memset(&nv_user_info, 0, sizeof(nv_user_info));
	nv_user_info.nv_operation = NV_WRITE_TAG;
	nv_user_info.nv_number = nv_number;
	nv_user_info.valid_size = nv_size;
	strncpy(nv_user_info.nv_name, nv_name, sizeof(nv_user_info.nv_name));
	nv_user_info.nv_name[sizeof(nv_user_info.nv_name) - 1] = '\0';
	/*copy to nv by pass*/
	memcpy(nv_user_info.nv_data, temp, sizeof(nv_user_info.nv_data) < nv_user_info.valid_size ?
		sizeof(nv_user_info.nv_data) : nv_user_info.valid_size);
	if ((ret = hisi_nve_direct_access(&nv_user_info))!=0)
	{
		hwlog_err("hisi_nve_direct_access read nv %d error(%d)\n", nv_number, ret);
		return -1;
	}
	return 0;
}
EXPORT_SYMBOL_GPL(write_color_calibrate_data_to_nv);

static ssize_t color_calibrate_show(struct device *dev, struct device_attribute *attr,
			     char *buf)
{
	color_sensor_output_para out_data ={0};
	struct amsDriver_chip *chip;
	int ret = 0;
	int i;
	int size = 1;
	if(NULL == dev || NULL == attr || NULL == buf)
	{
		hwlog_err("[%s] input NULL!! \n", __func__);
		return -1;
	}
	chip = dev_get_drvdata(dev);

	hwlog_info("[%s] in \n", __func__);
	size = sizeof(color_sensor_output_para);

	chip->color_show_calibrate_state(chip, &out_data);
	memcpy(buf, &out_data, size);
	return size;
}

static ssize_t color_calibrate_store(struct device *dev, struct device_attribute *attr,
			      const char *buf, size_t size)
{
	color_sensor_input_para in_data;
	bool state;
	struct amsDriver_chip *chip;

	if(NULL == dev || NULL == attr || NULL == buf)
	{
		hwlog_err("[%s] input NULL!! \n", __func__);
		return -1;
	}
	chip = dev_get_drvdata(dev);

	hwlog_info("[%s] color_sensor store in!! \n", __func__);

	if (size >= sizeof(color_sensor_input_para)){
		memcpy(&in_data, buf, sizeof(color_sensor_input_para));
	}

	hwlog_info("[%s] color_sensor store input enable = %d, data[%d, %d, %d, %d].\n", __func__,
		in_data.enable, in_data.tar_x, in_data.tar_y, in_data.tar_z, in_data.tar_ir);
	chip->color_store_calibrate_state(chip, &in_data);

	return size;
}
static ssize_t color_enable_show(struct device *dev, struct device_attribute *attr,
			      const char *buf)
{
	struct amsDriver_chip *chip;
	int state;

	if(NULL == dev || NULL == attr || NULL == buf)
	{
		hwlog_err("[%s] input NULL!! \n", __func__);
		return -1;
	}

	chip = dev_get_drvdata(dev);
	chip->color_enable_show_state(chip, &state);

	return snprintf(buf, ONE_SHOW_LEN, "%d\n", state);
}
static ssize_t color_enable_store(struct device *dev, struct device_attribute *attr,
			      const char *buf, size_t size)
{
	color_sensor_input_para in_data;
	struct amsDriver_chip *chip;
	bool state;

	if(NULL == dev || NULL == attr || NULL == buf)
	{
		hwlog_err("[%s] input NULL!! \n", __func__);
		return -1;
	}

	chip = dev_get_drvdata(dev);
	if (strtobool(buf, &state)) {
		hwlog_err("[%s] Failed to strtobool enable state.\n", __func__);
		return -EINVAL;
	}
	chip->color_enable_store_state(chip, (int)state);

	return size;
}

static ssize_t calibrate_timeout_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	if(NULL == dev || NULL == attr || NULL == buf)
	{
		hwlog_err("[%s] input NULL!! \n", __func__);
		return -1;
	}

	return snprintf(buf, PAGE_SIZE, "%d\n", TIME_OUT_DEFAULT);
}

static ssize_t color_gain_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct amsDriver_chip *chip;
	int gain;

	if(NULL == dev || NULL == attr || NULL == buf)
	{
		hwlog_err("[%s] input NULL!! \n", __func__);
		return -1;
	}

	chip = dev_get_drvdata(dev);
	//gain = ams_tcs3430_getGain(chip->deviceCtx);
	gain = chip->color_sensor_getGain(chip->deviceCtx);

	return snprintf(buf, PAGE_SIZE, "%d\n", gain);
}
static ssize_t color_gain_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	struct amsDriver_chip *chip;
	unsigned long value = 0L;
	int gain_value = 0;

	if(NULL == dev || NULL == attr || NULL == buf)
	{
		hwlog_err("[%s] input NULL!! \n", __func__);
		return -1;
	}

	chip = dev_get_drvdata(dev);

	if (strict_strtol(buf, 10, &value))
		return -EINVAL;
	gain_value = (int)value;
	//ams_tcs3430_setGain(chip->deviceCtx, gain_value);
	chip->color_sensor_setGain(chip->deviceCtx, gain_value);
	return size;
}
DEVICE_ATTR(calibrate, 0660, color_calibrate_show, color_calibrate_store);
DEVICE_ATTR(color_enable, 0660, color_enable_show, color_enable_store);
DEVICE_ATTR(gain, 0660, color_gain_show, color_gain_store);
DEVICE_ATTR(calibrate_timeout, 0440, calibrate_timeout_show, NULL);

static struct attribute *color_sensor_attributes[] = {
	&dev_attr_calibrate.attr,
	&dev_attr_color_enable.attr,
	&dev_attr_gain.attr,
	&dev_attr_calibrate_timeout.attr,
	NULL,
};
static const struct attribute_group color_sensor_attr_group = {
	.attrs = color_sensor_attributes,
};

static const struct attribute_group color_sensor_attr_groups[] = {
	&color_sensor_attr_group,
	NULL,
};

int color_register(struct amsDriver_chip *chip)
{
	if(NULL == chip)
	{
		hwlog_err("[%s] input NULL!! \n", __func__);
		return -1;
	}
		
	chip->dev = device_create(color_sensor_class, NULL, 0, chip, "color_sensor");
	if (NULL == chip->dev) {
		hwlog_err("[%s] Failed to create color_sensor dev.", __func__);
		return -1;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(color_register);

void color_unregister(struct amsDriver_chip *chip)
{
	device_destroy(color_sensor_class, 0);
}
EXPORT_SYMBOL_GPL(color_unregister);

static int color_sensor_init(void)
{
	color_sensor_class = class_create(THIS_MODULE, "ap_sensor");
	if (IS_ERR(color_sensor_class))
		return PTR_ERR(color_sensor_class);
	color_sensor_class->dev_groups = &color_sensor_attr_groups;

	hwlog_info("[%s]color_sensor init\n", __func__);
	return 0;
}

static void color_sensor_exit(void)
{
	class_destroy(color_sensor_class);
}

subsys_initcall(color_sensor_init);
module_exit(color_sensor_exit);

MODULE_AUTHOR("Huawei");
MODULE_DESCRIPTION("Color class init");
MODULE_LICENSE("GPL");
