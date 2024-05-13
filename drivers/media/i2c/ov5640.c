#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/slab.h>
#include <linux/v4l2-mediabus.h>
#include <linux/videodev2.h>

#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>

/* ov5640 registers */
#define REG_CHIP_ID_HIGH		0x300a
#define REG_CHIP_ID_LOW			0x300b


/* active pixel array size */
#define ov5640_SENSOR_SIZE_X	2592
#define ov5640_SENSOR_SIZE_Y	1944

/*
 * About ov5640 resolution, cropping and binning:
 * This sensor supports it all, at least in the feature description.
 * Unfortunately, no combination of appropriate registers settings could make
 * the chip work the intended way. As it works with predefined register lists,
 * some undocumented registers are presumably changed there to achieve their
 * goals.
 * This driver currently only works for resolutions up to 720 lines with a
 * 1:1 scale. Hopefully these restrictions will be removed in the future.
 */
#define ov5640_MAX_WIDTH	ov5640_SENSOR_SIZE_X
#define ov5640_MAX_HEIGHT	720

/* default sizes */
#define ov5640_DEFAULT_WIDTH	1280
#define ov5640_DEFAULT_HEIGHT	ov5640_MAX_HEIGHT

/* minimum extra blanking */
#define BLANKING_EXTRA_WIDTH		500
#define BLANKING_EXTRA_HEIGHT		20

/*
 * the sensor's autoexposure is buggy when setting total_height low.
 * It tries to expose longer than 1 frame period without taking care of it
 * and this leads to weird output. So we set 1000 lines as minimum.
 */
#define BLANKING_MIN_HEIGHT		1000

struct regval_list {
    u16 reg_num;
    u8 value;
};

static struct regval_list ov5640_default_regs_init[] = {
{ 0x3103, 0x03 },
{ 0x3017, 0xff },
{ 0x3018, 0xff },
{ 0x3034, 0x1A },
{ 0x3035, 0x11 },
{ 0x3036, 0x69 },
{ 0x3037, 0x13 },
{ 0x3108, 0x01 },
{ 0x3630, 0x36 },
{ 0x3631, 0x0e },
{ 0x3632, 0xe2 },
{ 0x3633, 0x12 },
{ 0x3621, 0xe0 },
{ 0x3704, 0xa0 },
{ 0x3703, 0x5a },
{ 0x3715, 0x78 },
{ 0x3717, 0x01 },
{ 0x370b, 0x60 },
{ 0x3705, 0x1a },
{ 0x3905, 0x02 },
{ 0x3906, 0x10 },
{ 0x3901, 0x0a },
{ 0x3731, 0x12 },
{ 0x3600, 0x08 },
{ 0x3601, 0x33 },
{ 0x302d, 0x60 },
{ 0x3620, 0x52 },
{ 0x371b, 0x20 },
{ 0x471c, 0x50 },
{ 0x3a13, 0x43 },
{ 0x3a18, 0x00 },
{ 0x3a19, 0xf8 },
{ 0x3635, 0x13 },
{ 0x3636, 0x03 },
{ 0x3634, 0x40 },
{ 0x3622, 0x01 },
{ 0x3c01, 0x34 },
{ 0x3c04, 0x28 },
{ 0x3c05, 0x98 },
{ 0x3c06, 0x00 },
{ 0x3c07, 0x07 },
{ 0x3c08, 0x00 },
{ 0x3c09, 0x1c },
{ 0x3c0a, 0x9c },
{ 0x3c0b, 0x40 },
{ 0x3820, 0x43 },
{ 0x3821, 0x01 },
{ 0x3814, 0x11 },
{ 0x3815, 0x11 },
{ 0x3800, 0x01 },
{ 0x3801, 0x50 },
{ 0x3802, 0x01 },
{ 0x3803, 0xb2 },
{ 0x3804, 0x08 },
{ 0x3805, 0xef },
{ 0x3806, 0x05 },
{ 0x3807, 0xf2 },
{ 0x3808, 0x07 },
{ 0x3809, 0x80 },
{ 0x380a, 0x04 },
{ 0x380b, 0x38 },
{ 0x380c, 0x09 },
{ 0x380d, 0xc4 },
{ 0x380e, 0x04 },
{ 0x380f, 0x60 },
{ 0x3810, 0x00 },
{ 0x3811, 0x10 },
{ 0x3812, 0x00 },
{ 0x3813, 0x04 },
{ 0x3618, 0x04 },
{ 0x3612, 0x2b },
{ 0x3708, 0x62 },
{ 0x3709, 0x12 },
{ 0x370c, 0x00 },
{ 0x3a02, 0x04 },
{ 0x3a03, 0x60 },
{ 0x3a08, 0x01 },
{ 0x3a09, 0x50 },
{ 0x3a0a, 0x01 },
{ 0x3a0b, 0x18 },
{ 0x3a0e, 0x03 },
{ 0x3a0d, 0x04 },
{ 0x3a14, 0x04 },
{ 0x3a15, 0x60 },
{ 0x4001, 0x02 },
{ 0x4004, 0x06 },
{ 0x3000, 0x00 },
{ 0x3002, 0x1c },
{ 0x3004, 0xff },
{ 0x3006, 0xc3 },
{ 0x300e, 0x58 },
{ 0x302e, 0x00 },
{ 0x4300, 0x3f },
{ 0x501f, 0x00 },
{ 0x4713, 0x02 },
{ 0x4407, 0x04 },
{ 0x440e, 0x00 },
{ 0x460b, 0x37 },
{ 0x460c, 0x20 },
{ 0x3824, 0x04 },
{ 0x5000, 0xa7 },
{ 0x5001, 0xa3 },
{ 0x5180, 0xff },
{ 0x5181, 0xf2 },
{ 0x5182, 0x00 },
{ 0x5183, 0x14 },
{ 0x5184, 0x25 },
{ 0x5185, 0x24 },
{ 0x5186, 0x09 },
{ 0x5187, 0x09 },
{ 0x5188, 0x09 },
{ 0x5189, 0x75 },
{ 0x518a, 0x54 },
{ 0x518b, 0xe0 },
{ 0x518c, 0xb2 },
{ 0x518d, 0x42 },
{ 0x518e, 0x3d },
{ 0x518f, 0x56 },
{ 0x5190, 0x46 },
{ 0x5191, 0xf8 },
{ 0x5192, 0x04 },
{ 0x5193, 0x70 },
{ 0x5194, 0xf0 },
{ 0x5195, 0xf0 },
{ 0x5196, 0x03 },
{ 0x5197, 0x01 },
{ 0x5198, 0x04 },
{ 0x5199, 0x12 },
{ 0x519a, 0x04 },
{ 0x519b, 0x00 },
{ 0x519c, 0x06 },
{ 0x519d, 0x82 },
{ 0x519e, 0x38 },
{ 0x5381, 0x1e },
{ 0x5382, 0x5b },
{ 0x5383, 0x08 },
{ 0x5384, 0x0a },
{ 0x5385, 0x7e },
{ 0x5386, 0x88 },
{ 0x5387, 0x7c },
{ 0x5388, 0x6c },
{ 0x5389, 0x10 },
{ 0x538a, 0x01 },
{ 0x538b, 0x98 },
{ 0x5300, 0x08 },
{ 0x5301, 0x30 },
{ 0x5302, 0x10 },
{ 0x5303, 0x00 },
{ 0x5304, 0x08 },
{ 0x5305, 0x30 },
{ 0x5306, 0x08 },
{ 0x5307, 0x16 },
{ 0x5309, 0x08 },
{ 0x530a, 0x30 },
{ 0x530b, 0x04 },
{ 0x530c, 0x06 },
{ 0x5480, 0x01 },
{ 0x5481, 0x08 },
{ 0x5482, 0x14 },
{ 0x5483, 0x28 },
{ 0x5484, 0x51 },
{ 0x5485, 0x65 },
{ 0x5486, 0x71 },
{ 0x5487, 0x7d },
{ 0x5488, 0x87 },
{ 0x5489, 0x91 },
{ 0x548a, 0x9a },
{ 0x548b, 0xaa },
{ 0x548c, 0xb8 },
{ 0x548d, 0xcd },
{ 0x548e, 0xdd },
{ 0x548f, 0xea },
{ 0x5490, 0x1d },
{ 0x5580, 0x02 },
{ 0x5583, 0x40 },
{ 0x5584, 0x10 },
{ 0x5589, 0x10 },
{ 0x558a, 0x00 },
{ 0x558b, 0xf8 },
{ 0x5800, 0x23 },
{ 0x5801, 0x14 },
{ 0x5802, 0x0f },
{ 0x5803, 0x0f },
{ 0x5804, 0x12 },
{ 0x5805, 0x26 },
{ 0x5806, 0x0c },
{ 0x5807, 0x08 },
{ 0x5808, 0x05 },
{ 0x5809, 0x05 },
{ 0x580a, 0x08 },
{ 0x580b, 0x0d },
{ 0x580c, 0x08 },
{ 0x580d, 0x03 },
{ 0x580e, 0x00 },
{ 0x580f, 0x00 },
{ 0x5810, 0x03 },
{ 0x5811, 0x09 },
{ 0x5812, 0x07 },
{ 0x5813, 0x03 },
{ 0x5814, 0x00 },
{ 0x5815, 0x01 },
{ 0x5816, 0x03 },
{ 0x5817, 0x08 },
{ 0x5818, 0x0d },
{ 0x5819, 0x08 },
{ 0x581a, 0x05 },
{ 0x581b, 0x06 },
{ 0x581c, 0x08 },
{ 0x581d, 0x0e },
{ 0x581e, 0x29 },
{ 0x581f, 0x17 },
{ 0x5820, 0x11 },
{ 0x5821, 0x11 },
{ 0x5822, 0x15 },
{ 0x5823, 0x28 },
{ 0x5824, 0x46 },
{ 0x5825, 0x26 },
{ 0x5826, 0x08 },
{ 0x5827, 0x26 },
{ 0x5828, 0x64 },
{ 0x5829, 0x26 },
{ 0x582a, 0x24 },
{ 0x582b, 0x22 },
{ 0x582c, 0x24 },
{ 0x582d, 0x24 },
{ 0x582e, 0x06 },
{ 0x582f, 0x22 },
{ 0x5830, 0x40 },
{ 0x5831, 0x42 },
{ 0x5832, 0x24 },
{ 0x5833, 0x26 },
{ 0x5834, 0x24 },
{ 0x5835, 0x22 },
{ 0x5836, 0x22 },
{ 0x5837, 0x26 },
{ 0x5838, 0x44 },
{ 0x5839, 0x24 },
{ 0x583a, 0x26 },
{ 0x583b, 0x28 },
{ 0x583c, 0x42 },
{ 0x583d, 0xce },
{ 0x5025, 0x00 },
{ 0x3a0f, 0x30 },
{ 0x3a10, 0x28 },
{ 0x3a1b, 0x30 },
{ 0x3a1e, 0x26 },
{ 0x3a11, 0x60 },
{ 0x3a1f, 0x14 },
{ 0x3008, 0x02 },
{ 0x3035, 0x21 },
{ 0xffff, 0xff },
};

struct ov5640_datafmt {
    u32	code;
    enum v4l2_colorspace		colorspace;
};

struct ov5640 {
    struct v4l2_subdev		subdev;
    struct media_pad pad;
    const struct ov5640_datafmt	*fmt;
    struct v4l2_rect crop_rect;
    struct i2c_client *client;

    /* blanking information */
    int total_width;
    int total_height;
    enum v4l2_colorspace colorspace;
	
	struct gpio_desc *reset_gpio;
	struct mutex lock; /* mutex lock for operations */
};

static const struct ov5640_datafmt ov5640_colour_fmts[] = {
{MEDIA_BUS_FMT_UYVY8_1X16, V4L2_COLORSPACE_JPEG},
};

static struct ov5640 *to_ov5640(const struct i2c_client *client)
{
    return container_of(i2c_get_clientdata(client), struct ov5640, subdev);
}

/*
 * ov5640_reset - Function called to reset the sensor
 * @priv: Pointer to device structure
 * @rst: Input value for determining the sensor's end state after reset
 *
 * Set the senor in reset and then
 * if rst = 0, keep it in reset;
 * if rst = 1, bring it out of reset.
 *
 */
static void ov5640_reset(struct ov5640 *priv, int rst)
{
	gpiod_set_value_cansleep(priv->reset_gpio, 0);
	usleep_range(2000, 2200);
	gpiod_set_value_cansleep(priv->reset_gpio, !!rst);
	usleep_range(2000, 2200);
}

/* Find a data format by a pixel code in an array */
static const struct ov5640_datafmt
        *ov5640_find_datafmt(u32 code)
{
    int i;

    for (i = 0; i < ARRAY_SIZE(ov5640_colour_fmts); i++)
        if (ov5640_colour_fmts[i].code == code)
            return ov5640_colour_fmts + i;

    return NULL;
}

static int reg_read(struct i2c_client *client, u16 reg, u8 *val)
{
    int ret;
    /* We have 16-bit i2c addresses - care for endianness */
    unsigned char data[2] = { reg >> 8, reg & 0xff };

    ret = i2c_master_send(client, data, 2);
    if (ret < 2) {
        dev_err(&client->dev, "%s: i2c read error, reg: %x\n",
                __func__, reg);
        return ret < 0 ? ret : -EIO;
    }

    ret = i2c_master_recv(client, val, 1);
    if (ret < 1) {
        dev_err(&client->dev, "%s: i2c read error, reg: %x\n",
                __func__, reg);
        return ret < 0 ? ret : -EIO;
    }
    return 0;
}

static int reg_write(struct i2c_client *client, u16 reg, u8 val)
{
    int ret;
    unsigned char data[3] = { reg >> 8, reg & 0xff, val };

    ret = i2c_master_send(client, data, 3);
    if (ret < 3) {
        dev_err(&client->dev, "%s: i2c write error, reg: %x\n",
                __func__, reg);
        return ret < 0 ? ret : -EIO;
    }

    return 0;
}

/*
 * convenience function to write 16 bit register values that are split up
 * into two consecutive high and low parts
 */
static int reg_write16(struct i2c_client *client, u16 reg, u16 val16)
{
    int ret;

    ret = reg_write(client, reg, val16 >> 8);
    if (ret)
        return ret;
    return reg_write(client, reg + 1, val16 & 0x00ff);
}

#ifdef CONFIG_VIDEO_ADV_DEBUG
static int ov5640_get_register(struct v4l2_subdev *sd, struct v4l2_dbg_register *reg)
{
    struct i2c_client *client = v4l2_get_subdevdata(sd);
    int ret;
    u8 val;

    if (reg->reg & ~0xffff)
        return -EINVAL;

    reg->size = 1;

    ret = reg_read(client, reg->reg, &val);
    if (!ret)
        reg->val = (__u64)val;

    return ret;
}

static int ov5640_set_register(struct v4l2_subdev *sd, const struct v4l2_dbg_register *reg)
{
    struct i2c_client *client = v4l2_get_subdevdata(sd);

    if (reg->reg & ~0xffff || reg->val & ~0xff)
        return -EINVAL;

    return reg_write(client, reg->reg, reg->val);
}
#endif

static int ov5640_write_array(struct i2c_client *client,
                              struct regval_list *vals)
{
    while (vals->reg_num != 0xffff || vals->value != 0xff) {
        int ret = reg_write(client, vals->reg_num, vals->value);
        if (ret < 0)
            return ret;
        vals++;
    }
    dev_dbg(&client->dev, "Register list loaded\n");
    return 0;
}



static int ov5640_video_probe(struct i2c_client *client)
{
    struct v4l2_subdev *subdev = i2c_get_clientdata(client);
    int ret;
    u8 id_high, id_low;
    u16 id;

    /* Read sensor Model ID */
    ret = reg_read(client, REG_CHIP_ID_HIGH, &id_high);
    if (ret < 0)
        goto done;

    id = id_high << 8;

    ret = reg_read(client, REG_CHIP_ID_LOW, &id_low);
    if (ret < 0)
        goto done;

    id |= id_low;

    dev_info(&client->dev, "Chip ID 0x%04x detected\n", id);

    if (id != 0x5640) {
        ret = -ENODEV;
        goto done;
    }

    ret = 0;

done:
    return ret;
}

static int ov5640_get_fmt(struct v4l2_subdev *sd,
		struct v4l2_subdev_state *state,
		struct v4l2_subdev_format *format)
{
	struct v4l2_mbus_framefmt *mf = &format->format;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct ov5640 *priv = to_ov5640(client);

	const struct ov5640_datafmt *fmt = priv->fmt;

	if (format->pad)
		return -EINVAL;

	mf->code	= fmt->code;
	mf->colorspace	= fmt->colorspace;
	mf->width	= priv->crop_rect.width;
	mf->height	= priv->crop_rect.height;
	mf->field	= V4L2_FIELD_NONE;

	return 0;
}

static int ov5640_set_fmt(struct v4l2_subdev *sd,
		struct v4l2_subdev_state *state,
		struct v4l2_subdev_format *format)
{
	struct v4l2_mbus_framefmt *mf = &format->format;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct ov5640 *priv = to_ov5640(client);
	const struct ov5640_datafmt *fmt = ov5640_find_datafmt(mf->code);

    priv->crop_rect.width   = format->format.width;
    priv->crop_rect.height  = format->format.height;
    priv->crop_rect.left    = (ov5640_MAX_WIDTH - format->format.width) / 2;
    priv->crop_rect.top     = (ov5640_MAX_HEIGHT - format->format.height) / 2;
    priv->total_width       = format->format.width + BLANKING_EXTRA_WIDTH;
    priv->total_height      = format->format.height + BLANKING_EXTRA_HEIGHT;
    priv->colorspace        = format->format.colorspace;

    printk(KERN_WARNING "++++++++++reay set format %dx%d++++++++++\n",
           priv->crop_rect.width,
           priv->crop_rect.height);

    reg_write(client, 0x3808, (format->format.width>>8)&0xff);
    reg_write(client, 0x3809, (format->format.width>>0)&0xff);
    reg_write(client, 0x380a, (format->format.height>>8)&0xff);
    reg_write(client, 0x380b, (format->format.height>>0)&0xff);

    return 0; /* FIXME */
}



static int ov5640_enum_mbus_code(struct v4l2_subdev *sd,
		struct v4l2_subdev_state *state,
		struct v4l2_subdev_mbus_code_enum *code)
{
	if (code->pad || code->index >= ARRAY_SIZE(ov5640_colour_fmts))
		return -EINVAL;

	code->code = ov5640_colour_fmts[code->index].code;
	return 0;
}

static int ov5640_s_power(struct v4l2_subdev *sd, int on)
{
	int ret;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	ret = ov5640_write_array(client, ov5640_default_regs_init);
	return ret;
}
static int ov5640_s_stream(struct v4l2_subdev *sd, int enable)
{
	int ret = 0;
	return ret;
}
static struct v4l2_subdev_video_ops ov5640_subdev_video_ops = {
	.s_stream = ov5640_s_stream,	
};

static const struct v4l2_subdev_pad_ops ov5640_subdev_pad_ops = {
	.enum_mbus_code = ov5640_enum_mbus_code,
	.get_fmt	= ov5640_get_fmt,
	.set_fmt	= ov5640_set_fmt,
};

static struct v4l2_subdev_core_ops ov5640_subdev_core_ops = {
	.s_power	= ov5640_s_power,
#ifdef CONFIG_VIDEO_ADV_DEBUG
	.g_register	= ov5640_get_register,
	.s_register	= ov5640_set_register,
#endif
};


static const struct v4l2_subdev_ops ov5640_ops = {
	.core	= &ov5640_subdev_core_ops,
	.video	= &ov5640_subdev_video_ops,
    .pad   = &ov5640_subdev_pad_ops,
};

static int ov5640_probe(struct i2c_client *client,
                        const struct i2c_device_id *did)
{
    struct ov5640 *priv;
    int ret;
    priv = devm_kzalloc(&client->dev, sizeof(struct ov5640), GFP_KERNEL);
    if (!priv)
        return -ENOMEM;

    priv->fmt               = &ov5640_colour_fmts[0];
    priv->crop_rect.width   = ov5640_DEFAULT_WIDTH;
    priv->crop_rect.height  = ov5640_DEFAULT_HEIGHT;
    priv->crop_rect.left    = (ov5640_MAX_WIDTH - ov5640_DEFAULT_WIDTH) / 2;
    priv->crop_rect.top     = (ov5640_MAX_HEIGHT - ov5640_DEFAULT_HEIGHT) / 2;
    priv->total_width       = ov5640_DEFAULT_WIDTH + BLANKING_EXTRA_WIDTH;
    priv->total_height      = BLANKING_MIN_HEIGHT;
    priv->colorspace        = 8;

    priv->client = client;
    priv->client->flags = I2C_CLIENT_SCCB;

    v4l2_i2c_subdev_init(&priv->subdev, client, &ov5640_ops);

    /* V4L init */
    strcpy(priv->subdev.name, "ov5640");

    priv->pad.flags = MEDIA_PAD_FL_SOURCE;
    priv->subdev.entity.function = MEDIA_ENT_F_CAM_SENSOR;
    ret = media_entity_pads_init(&priv->subdev.entity, 1, &priv->pad);
    if (ret) {
        v4l_err(client, "Failed to initialized pad\n");
        goto done;
    }
	
	/* initialize sensor reset gpio */
	priv->reset_gpio = devm_gpiod_get_optional(&client->dev, "reset",
						     GPIOD_OUT_HIGH);
	if (IS_ERR(priv->reset_gpio)) {
		if (PTR_ERR(priv->reset_gpio) != -EPROBE_DEFER)
			dev_err(&client->dev, "Reset GPIO not setup in DT");
		ret = PTR_ERR(priv->reset_gpio);
	}
	
	/* pull sensor out of reset */
	ov5640_reset(priv, 1);
	
	ret = ov5640_write_array(client, ov5640_default_regs_init);
	
    ret = v4l2_async_register_subdev(&priv->subdev);
    if (ret) {
        v4l_err(client, "Failed to register subdev for async detection\n");
        goto done;
    }

done:
    if (ret > 0) {
        media_entity_cleanup(&priv->subdev.entity);
    }

    ov5640_video_probe(client);

err_me:
	//media_entity_cleanup(&sd->entity);
	//mutex_destroy(&priv->lock);
	return ret;
}

static void ov5640_remove(struct i2c_client *client)
{
    
}

static const struct of_device_id ov5640_of_id_table[] = {
	{ .compatible = "alinx,ov5640" },
	{ }
};
MODULE_DEVICE_TABLE(of, ov5640_of_id_table);

static const struct i2c_device_id ov5640_id[] = {
	{ "ov5640", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ov5640_id);

static struct i2c_driver ov5640_i2c_driver = {
    .driver = {
        .name = "ov5640",
		.of_match_table	= ov5640_of_id_table,
    },
    .probe		= ov5640_probe,
    .remove		= ov5640_remove,
    .id_table	= ov5640_id,
};

module_i2c_driver(ov5640_i2c_driver);

MODULE_DESCRIPTION("Omnivision ov5640 Camera driver");
MODULE_AUTHOR("alinx");
MODULE_LICENSE("GPL v2");


