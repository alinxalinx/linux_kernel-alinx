// SPDX-License-Identifier: GPL-2.0
/*
 * Digilent FPGA HDMI driver.
 *
 * Copyright (C) 2020 Digilent, Inc.
 *
 * Author : Cosmin Tanislav <demonsingur@gmail.com>
 */


#include <drm/drm_atomic_helper.h>
#include <drm/drm_crtc.h>
#include <drm/drm_edid.h>
#include <drm/drm_fourcc.h>
#include <drm/drm_probe_helper.h>
#include <linux/clk.h>
#include <linux/component.h>
#include <linux/device.h>
#include <linux/of_device.h>
#include <linux/i2c.h>
#include <linux/of_gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/delay.h>

struct digilent_hdmi {
	struct drm_encoder encoder;
	struct drm_connector connector;
	struct drm_device *drm_dev;

	struct device *dev;

	struct clk *clk;
	bool clk_enabled;

	struct i2c_adapter *i2c_bus;
	u32 fmax;
	u32 hmax;
	u32 vmax;
	u32 hpref;
	u32 vpref;
	u32 reset_gpio;
};
//72
#define TX_SLV0 					0x39
//7A
#define TX_SLV1 					0x3D
#define connector_to_hdmi(c) container_of(c, struct digilent_hdmi, connector)
#define encoder_to_hdmi(e) container_of(e, struct digilent_hdmi, encoder)

typedef unsigned char BYTE;
typedef unsigned int  WORD;


typedef struct {
 BYTE SlaveAddr;
 BYTE Offset;
 BYTE RegAddr;
 BYTE NBytesLSB;
 BYTE NBytesMSB;
 BYTE Dummy;
 BYTE Cmd;

} MDDCType;

static struct edid m_edid;

static u8 ax_readbyte_8b(struct i2c_adapter *i2c_bus, BYTE slaveAddr,  BYTE regAddr)
{
	u8 rdbuf = 0;
	int error;
	
	struct i2c_msg wrmsg[2] = {
		{
			.addr = slaveAddr,
			.flags = 0,
			.len = 1,
			.buf = &regAddr,
		}, {
			.addr = slaveAddr,
			.flags = I2C_M_RD,
			.len = 1,
			.buf = &rdbuf,
		}
	};


	if(!i2c_bus)
	{
		printk(KERN_ALERT "ax_readbyte_8b i2c_bus error\n");
		return 0;
	}
	
	error = i2c_transfer(i2c_bus , wrmsg, 2);
	if(error < 0)
	{
		printk(KERN_ALERT "ax_readbyte_8b transfer failed\n");		
		return 0;
	}

	return rdbuf;

}

static void ax_writebyte_8b(struct i2c_adapter *i2c_bus, BYTE slaveAddr,  BYTE regAddr, BYTE data)
{
	int error;

	BYTE buffer[2];

	struct i2c_msg wrmsg = {	
		.addr = slaveAddr,
		.flags = 0,
		.len = 2,
		.buf = buffer,
	};

	
	buffer[0] = regAddr;
	buffer[1] = data;



	if(!i2c_bus)
	{
		printk(KERN_ALERT "ax_writebyte_8b i2c_bus error\n");
		return ;
	}
	
	error = i2c_transfer(i2c_bus , &wrmsg, 1);
	if(error < 0)
	{
		printk(KERN_ALERT "ax_writebyte_8b transfer failed\n");		
		return;
	}

}


static void ax_writebyte_nb(struct i2c_adapter *i2c_bus, BYTE slaveAddr,  BYTE regAddr,BYTE offset)
{
	int error;

	BYTE buffer[8];

	struct i2c_msg wrmsg = {	
		.addr = slaveAddr,
		.flags = 0,
		.len = 8,
		.buf = buffer,		
	};

	
	buffer[0] = regAddr;
	buffer[1] = 0xA0;
	buffer[2] = 0x00;
	buffer[3] = offset;
	buffer[4] = 16;
	buffer[5] = 0x00;
	buffer[6] = 0x00;
	buffer[7] = 0x02;



	if(!i2c_bus)
	{
		printk(KERN_ALERT "ax_writebyte_nb i2c_bus error\n");
		return ;
	}
	
	error = i2c_transfer(i2c_bus , &wrmsg, 1);
	if(error < 0)
	{
		printk(KERN_ALERT "ax_writebyte_nb transfer failed\n");		
		return;
	}

}



void ax_9134_BlockRead_MDDC(struct i2c_adapter *i2c_bus, BYTE * pData)
{
	BYTE FIFO_Size,  Status;

	int datacount = 128;
	int i = 0;
	int j =0;

	ax_writebyte_8b(i2c_bus, TX_SLV0, 0xF3, 0x09);
	udelay(200);


	ax_writebyte_8b(i2c_bus, TX_SLV0, 0xF5, 0x10);
	udelay(1000);	


	FIFO_Size = 0;

	datacount = 16;

	for(j=0;j<8;j++)
	{
		ax_writebyte_nb(i2c_bus, TX_SLV0,0xED, j*16);
		udelay(500);
	
		do{
			BYTE is = ax_readbyte_8b(i2c_bus, TX_SLV0, 0xF2);
			if((is>>3) & 0x01)
				break;
			Status = (is>>4) & 0x01;
			udelay(100);				
		}while(Status);
		
		
		FIFO_Size = ax_readbyte_8b(i2c_bus, TX_SLV0, 0xF5);
		
		if(FIFO_Size) {
			for(i=0;i<FIFO_Size;i++)
			{
				
			
				BYTE data = ax_readbyte_8b(i2c_bus, TX_SLV0, 0xF4);	

				memcpy(pData, &data, 1);
				pData++;
				mdelay(10);				
			}
		}
		
	}



}

static void ax_9134_BlockReadEDID(struct i2c_adapter *i2c_bus, BYTE *pData)
{

	ax_9134_BlockRead_MDDC(i2c_bus, pData);

}


static int ax_9134_drm_edid_block_checksum(const u8 *raw_edid)
{
	int i;
	u8 csum = 0;
	for (i = 0; i < EDID_LENGTH; i++)
		csum += raw_edid[i];

	return csum;
}


static bool ax_9134_drm_edid_block_valid(u8 *raw_edid)
{
	u8 csum;
	int score;
	struct edid *edid = (struct edid *)raw_edid;

	if (WARN_ON(!raw_edid))
		return false;

	score = drm_edid_header_is_valid(raw_edid);
	if (score == 8) ;
	else {
		goto bad;
	}

	csum = ax_9134_drm_edid_block_checksum(raw_edid);
	if (csum) {
		if (raw_edid[0] != 0x02)
			goto bad;
	}

	switch (raw_edid[0]) {
	case 0: 
		if (edid->version != 1) {
			goto bad;
		}

		if (edid->revision > 4)
		break;

	default:
		break;
	}

	return true;

bad:

	return false;
}


static bool ax_9134_drm_edid_is_valid(struct edid *edid)
{
	int i;
	u8 *raw = (u8 *)edid; 

	if (!edid)
		return false;

	edid->checksum += edid->extensions;
	edid->extensions = 0;	

	if (!ax_9134_drm_edid_block_valid(raw + i * EDID_LENGTH))
		return false;

	return true;
}



static int digilent_hdmi_get_modes(struct drm_connector *connector)
{
	struct digilent_hdmi *hdmi = connector_to_hdmi(connector);
	struct edid *edid;
	int count = 0;

	if (hdmi->i2c_bus) {
		BYTE editbuf[128];

		struct edid *ped = &m_edid;
		
	    	memset(editbuf, 0, 128);

        	ax_9134_BlockReadEDID(hdmi->i2c_bus, editbuf);


		if (ax_9134_drm_edid_is_valid((struct edid*)&editbuf)) {
			memcpy(ped, editbuf, 128);
			ped->checksum += ped->extensions;
			ped->extensions = 0;
			
		}


		drm_connector_update_edid_property(connector, ped);
   		count = drm_add_edid_modes(connector, ped);

	  
	    	ax_writebyte_8b(hdmi->i2c_bus, TX_SLV0, 0x08, 0x35);
	    	udelay(100);			
	    	ax_writebyte_8b(hdmi->i2c_bus, TX_SLV1, 0x2f, 0x00);

	} else {
		count = drm_add_modes_noedid(connector, hdmi->hmax, hdmi->vmax);
		drm_set_preferred_mode(connector, hdmi->hpref, hdmi->vpref);
	}

	return 0;
}

static int digilent_hdmi_mode_valid(struct drm_connector *connector,
		struct drm_display_mode *mode)
{
	struct digilent_hdmi *hdmi = connector_to_hdmi(connector);

		if (!mode)
			goto mode_bad;

		if (mode->flags & (DRM_MODE_FLAG_INTERLACE | DRM_MODE_FLAG_DBLCLK
							| DRM_MODE_FLAG_3D_MASK))
			goto mode_bad;

		if (mode->clock > hdmi->fmax
						|| mode->hdisplay > hdmi->hmax
						|| mode->vdisplay > hdmi->vmax)
			goto mode_bad;

		return MODE_OK;

	mode_bad:
		return MODE_BAD;
}

static
struct drm_encoder *digilent_hdmi_best_encoder(struct drm_connector *connector)
	{
			struct digilent_hdmi *hdmi = connector_to_hdmi(connector);
			return &hdmi->encoder;
		}
		
		static
		struct drm_connector_helper_funcs digilent_hdmi_connector_helper_funcs = {
				.get_modes = digilent_hdmi_get_modes,
				.mode_valid	= digilent_hdmi_mode_valid,
				.best_encoder = digilent_hdmi_best_encoder,
			};


static
enum drm_connector_status digilent_hdmi_detect(struct drm_connector *connector,
				bool force)
	{
			BYTE ret;
			struct digilent_hdmi *hdmi = connector_to_hdmi(connector);
		
				if (!hdmi->i2c_bus)
					return connector_status_unknown;
					
				ret = ax_readbyte_8b(hdmi->i2c_bus, TX_SLV0, 0x09 );
				if(ret == 0x81)
				{
					memset(&m_edid, 0, 128);
					return connector_status_disconnected;
				}	
		   		if(ret == 0x87)
   				{
   					return connector_status_connected; 		
   				}
				memset(&m_edid, 0, 128);
				return connector_status_unknown;
				
				return drm_probe_ddc(hdmi->i2c_bus)
						? connector_status_connected
						: connector_status_disconnected;
		}
		
		static void digilent_hdmi_connector_destroy(struct drm_connector *connector)
	{
			drm_connector_unregister(connector);
			drm_connector_cleanup(connector);
		}
		
		static const struct drm_connector_funcs digilent_hdmi_connector_funcs = {
				.detect = digilent_hdmi_detect,
				.fill_modes = drm_helper_probe_single_connector_modes,
				.destroy = digilent_hdmi_connector_destroy,
				.atomic_duplicate_state	= drm_atomic_helper_connector_duplicate_state,
				.atomic_destroy_state	= drm_atomic_helper_connector_destroy_state,
				.reset			= drm_atomic_helper_connector_reset,
			};

static int digilent_hdmi_create_connector(struct digilent_hdmi *hdmi)
	{
			struct drm_connector *connector = &hdmi->connector;
			struct drm_encoder *encoder = &hdmi->encoder;
			int ret;
		
				connector->polled = DRM_CONNECTOR_POLL_CONNECT
						| DRM_CONNECTOR_POLL_DISCONNECT;
		
				ret = drm_connector_init(hdmi->drm_dev, connector,
								&digilent_hdmi_connector_funcs,
								DRM_MODE_CONNECTOR_HDMIA);
			if (ret) {
					dev_err(hdmi->dev, "failed to initialize connector\n");
					return ret;
				}
			drm_connector_helper_add(connector,
							&digilent_hdmi_connector_helper_funcs);
		
				drm_connector_register(connector);
			drm_connector_attach_encoder(connector, encoder);
		
				return 0;
		}
		
		static void digilent_hdmi_atomic_mode_set(struct drm_encoder *encoder,
						struct drm_crtc_state *crtc_state,
						struct drm_connector_state *connector_state)
{
	struct digilent_hdmi *hdmi = encoder_to_hdmi(encoder);
	struct drm_display_mode *m = &crtc_state->adjusted_mode;

	clk_set_rate(hdmi->clk, m->clock * 1000);
}

static void digilent_hdmi_enable(struct drm_encoder *encoder)
	{
			struct digilent_hdmi *hdmi = encoder_to_hdmi(encoder);
		
				if (hdmi->clk_enabled)
					return;
		
				clk_prepare_enable(hdmi->clk);
			hdmi->clk_enabled = true;
}

static void digilent_hdmi_disable(struct drm_encoder *encoder)
{
	struct digilent_hdmi *hdmi = encoder_to_hdmi(encoder);

	if (!hdmi->clk_enabled)
		return;

		clk_disable_unprepare(hdmi->clk);
	hdmi->clk_enabled = false;
}

static const struct drm_encoder_helper_funcs digilent_hdmi_encoder_helper_funcs = {
		.atomic_mode_set = digilent_hdmi_atomic_mode_set,
	.enable = digilent_hdmi_enable,
	.disable = digilent_hdmi_disable,
};

static const struct drm_encoder_funcs digilent_hdmi_encoder_funcs = {
	.destroy = drm_encoder_cleanup,
};

static int digilent_hdmi_create_encoder(struct digilent_hdmi *hdmi)
{
	struct drm_encoder *encoder = &hdmi->encoder;
	int ret;

	encoder->possible_crtcs = 1;
	ret = drm_encoder_init(hdmi->drm_dev, encoder,
					&digilent_hdmi_encoder_funcs,
					DRM_MODE_ENCODER_TMDS, NULL);
	if (ret) {
			dev_err(hdmi->dev, "failed to initialize encoder\n");
			return ret;
		}
	drm_encoder_helper_add(encoder, &digilent_hdmi_encoder_helper_funcs);

		return 0;
}

static int digilent_hdmi_bind(struct device *dev, struct device *master,
					 void *data)
	{
			struct digilent_hdmi *hdmi = dev_get_drvdata(dev);
			int ret;
		
				hdmi->drm_dev = data;
		
				ret = digilent_hdmi_create_encoder(hdmi);
			if (ret) {
					dev_err(dev, "failed to create encoder: %d\n", ret);
					goto encoder_create_fail;
				}
		
			
				ret = digilent_hdmi_create_connector(hdmi);
			if (ret) {
					dev_err(dev, "failed to create connector: %d\n", ret);
					goto hdmi_create_fail;
				}
		
	return 0;

hdmi_create_fail:
	drm_encoder_cleanup(&hdmi->encoder);
encoder_create_fail:
	return ret;
}

static void digilent_hdmi_unbind(struct device *dev, struct device *master,
		void *data)
{
	struct digilent_hdmi *hdmi = dev_get_drvdata(dev);

	digilent_hdmi_disable(&hdmi->encoder);
}

static const struct component_ops digilent_hdmi_component_ops = {
	.bind	= digilent_hdmi_bind,
	.unbind	= digilent_hdmi_unbind,
};

#define DIGILENT_ENC_MAX_FREQ 150000
#define DIGILENT_ENC_MAX_H 1920
#define DIGILENT_ENC_MAX_V 1080
#define DIGILENT_ENC_PREF_H 1280
#define DIGILENT_ENC_PREF_V 720

static int digilent_hdmi_parse_dt(struct digilent_hdmi *hdmi)
{
	struct device *dev = hdmi->dev;
	struct device_node *node = dev->of_node;
	struct device_node *i2c_node;
	int ret;
	int gpio;
	u32 reset_gpio_flags;
	
	hdmi->clk = devm_clk_get(dev, "clk");
	if (IS_ERR(hdmi->clk)) {
			ret = PTR_ERR(hdmi->clk);
			dev_err(dev, "failed to get hdmi clock: %d\n", ret);
			return ret;
		}

		i2c_node = of_parse_phandle(node, "digilent,edid-i2c", 0);
	if (i2c_node) {
			hdmi->i2c_bus = of_get_i2c_adapter_by_node(i2c_node);
			of_node_put(i2c_node);
	
				if (!hdmi->i2c_bus) {
						ret = -EPROBE_DEFER;
						dev_err(dev, "failed to get edid i2c adapter: %d\n", ret);
						return ret;
					}
		} else {
				dev_info(dev, "failed to find edid i2c property\n");
			}

	gpio = of_get_named_gpio_flags(node, "ax_9134,reset-gpios",0, &reset_gpio_flags);
	if (gpio < 0)
	{

		return gpio;
	}

	hdmi->reset_gpio = gpio;	

	if(gpio_is_valid(gpio))
	{
		int i;
		int err = gpio_request(hdmi->reset_gpio, "ax_reset_gpio");
		if (err) {
			goto free_reset_gpio;
		}		


		err = gpio_direction_output(hdmi->reset_gpio, 0);
		if (err) {
			goto free_reset_gpio;
		}
		for(i=0;i<100;i++)
		udelay(500);
		
		err = gpio_direction_output(hdmi->reset_gpio, 1);
		if (err) {
			goto free_reset_gpio;
		}	


	}






		ret = of_property_read_u32(node, "digilent,fmax", &hdmi->fmax);
	if (ret < 0)
			hdmi->fmax = DIGILENT_ENC_MAX_FREQ;

		ret = of_property_read_u32(node, "digilent,hmax", &hdmi->hmax);
	if (ret < 0) {
			hdmi->hmax = DIGILENT_ENC_MAX_H;
			dev_info(dev, "No max horizontal width in DT, using default %d\n", DIGILENT_ENC_MAX_H);
	}

		ret = of_property_read_u32(node, "digilent,vmax", &hdmi->vmax);
	if (ret < 0)
			hdmi->vmax = DIGILENT_ENC_MAX_V;

		ret = of_property_read_u32(node, "digilent,hpref", &hdmi->hpref);
	if (ret < 0)
			hdmi->hpref = DIGILENT_ENC_PREF_H;

		ret = of_property_read_u32(node, "digilent,vpref", &hdmi->vpref);
	if (ret < 0)
		hdmi->vpref = DIGILENT_ENC_PREF_V;

	return 0;
	
	
free_reset_gpio:
	if (gpio_is_valid(hdmi->reset_gpio))
		gpio_free(hdmi->reset_gpio);	

	return -1;	
}

static int digilent_hdmi_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct digilent_hdmi *hdmi;
	int ret;

	hdmi = devm_kzalloc(dev, sizeof(*hdmi), GFP_KERNEL);
	if (!hdmi) {
		ret = -ENOMEM;
		dev_err(dev, "failed to allocate: %d\n", ret);
		return ret;
	}

	hdmi->dev = dev;

	ret = digilent_hdmi_parse_dt(hdmi);
	if (ret) {
		dev_err(dev, "failed to parse device tree: %d\n", ret);
		return ret;
	}

	platform_set_drvdata(pdev, hdmi);

		ret = component_add(dev, &digilent_hdmi_component_ops);
	if (ret < 0) {
		dev_err(dev, "fail to add component: %d\n", ret);
		return ret;
	}

	return 0;
}

static int digilent_hdmi_remove(struct platform_device *pdev)
{
	struct digilent_hdmi *hdmi = platform_get_drvdata(pdev);

	component_del(&pdev->dev, &digilent_hdmi_component_ops);
	if (hdmi->i2c_bus)
		i2c_put_adapter(hdmi->i2c_bus);
	return 0;
}

static const struct of_device_id digilent_hdmi_of_match[] = {
		{ .compatible = "ax_9134,drm-encoder"},
		{ }
	};
MODULE_DEVICE_TABLE(of, digilent_hdmi_of_match);

static struct platform_driver hdmi_driver = {
		.probe = digilent_hdmi_probe,
		.remove = digilent_hdmi_remove,
		.driver = {
				.name = "digilent-hdmi",
				.of_match_table = digilent_hdmi_of_match,
			},
};


module_platform_driver(hdmi_driver);

MODULE_AUTHOR("Cosmin Tanislav <demonsingur@gmail.com>");
MODULE_DESCRIPTION("Digilent FPGA HDMI driver");
MODULE_LICENSE("GPL v2");

