#include <asm/uaccess.h>
#include <linux/dma/xilinx_dma.h>
#include <linux/cdev.h>
#include <linux/dma-mapping.h>
#include <linux/dmaengine.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include "axi_adc_dma.h"


/*ADC channel name */
static const char adc_channels[][20] =
{
	{"adc0"},
	{"adc1"},
	{"adc2"},
	{"adc3"},
	{"adc4"},
	{"adc5"},
	{"adc6"},
	{"adc7"},
	{"adc8"},
	{"adc9"},
	{"adc10"},
	{"adc11"},
	{"adc12"},
	{"adc13"},
	{"adc14"},
	{"adc15"},
};

static struct axi_adc_dev *axi_adc_dev[16];
static int dev_index = 0;
static dev_t devno;
static struct cdev adc_cdev;
static struct class *axi_adc_class;
static void dma_slave_rx_callback(void *completion)
{
	complete(completion);
}


/* File operations */
int axi_adc_dma_open(struct inode *inode, struct file *filp)
{

	unsigned int mn;
	mn = iminor(inode);
	/*Assign minor number for later reference */
	filp->private_data = (void *) mn;
	return SUCCESS;
}

int axi_adc_dma_release(struct inode *inode, struct file *filp)
{
	return SUCCESS;
}

ssize_t axi_adc_dma_read(struct file * filep, char __user * buf,
                         size_t count, loff_t * f_pos)
{
	int minor = 0, rx_tmo = 0, status = 0;

	struct dma_device *rx_dev = axi_adc_dev[minor]->rx_chan->device;
	/* Query minor number.
	 * @To be extended for multiple channel support
	 */
	minor = (int) filep->private_data;

	/* Validation for read size */
	if (count > axi_adc_dev[minor]->dma_len_bytes)
	{
		dev_err(&axi_adc_dev[minor]->pdev->dev, "improper buffer size \n");
		return EINVAL;
	}

	
	rx_tmo = wait_for_completion_timeout(&axi_adc_dev[minor]->rx_cmp,
	                                     axi_adc_dev[minor]->rx_tmo);
	/* Check the status of DMA channel */
	status =dma_async_is_tx_complete(axi_adc_dev[minor]->rx_chan,
	                                 axi_adc_dev[minor]->rx_cookie, NULL, NULL);
	if (rx_tmo == 0)
	{
		dev_err(&axi_adc_dev[minor]->pdev->dev, "RX test timed out\n");
		return -EAGAIN;
	}
	dma_unmap_single(rx_dev->dev, axi_adc_dev[minor]->dma_dsts[0],axi_adc_dev[minor]->dma_len_bytes, DMA_DEV_TO_MEM);
	copy_to_user(buf, axi_adc_dev[minor]->dsts[0], count);
	return count;
}

/* IOCTL calls provide interface to configure ,start and stop
   DMA engine */
static long axi_adc_dma_ioctl(struct file *file,unsigned int cmd, unsigned long arg)
{
	enum dma_status status;
	enum dma_ctrl_flags flags;
	int ret;
	int i;
	int minor = (int) file->private_data;
	struct adc_dma_cfg *cfg;
	struct dma_device *rx_dev = axi_adc_dev[minor]->rx_chan->device;
	struct dma_async_tx_descriptor *rxd = NULL;
	struct scatterlist rx_sg[MAX_BUF_COUNT];
	//dma_addr_t dma_dsts[bd_cnt];
	switch (cmd)
	{
	case AXI_ADC_SET_SAMPLE_NUM:
		axi_adc_dev[minor]->adc_sample_num = arg;
		break;
	case AXI_ADC_SET_DMA_LEN_BYTES:
		axi_adc_dev[minor]->dma_len_bytes = arg;
		break;
	case AXI_ADC_DMA_INIT:
		axi_adc_dev[minor]->bd_cnt = 1;
		axi_adc_dev[minor]->dsts = kcalloc(axi_adc_dev[minor]->bd_cnt+1, sizeof(u8 *), GFP_KERNEL);
		if (!axi_adc_dev[minor]->dsts) return ret;
		for (i = 0; i < axi_adc_dev[minor]->bd_cnt; i++)
		{
			axi_adc_dev[minor]->dsts[i] = kmalloc(axi_adc_dev[minor]->dma_len_bytes, GFP_KERNEL);
		}
		axi_adc_dev[minor]->dsts[i] = NULL;
		axi_adc_dev[minor]->dma_dsts = kcalloc(axi_adc_dev[minor]->bd_cnt+1, sizeof(dma_addr_t), GFP_KERNEL);
		flags = DMA_CTRL_ACK | DMA_PREP_INTERRUPT;
		for (i = 0; i < axi_adc_dev[minor]->bd_cnt; i++)
		{
			axi_adc_dev[minor]->dma_dsts[i] =
			    dma_map_single(rx_dev->dev,axi_adc_dev[minor]->dsts[i],
			                   axi_adc_dev[minor]->dma_len_bytes,DMA_MEM_TO_DEV);
		}

		break;
	case AXI_ADC_DMA_START:
		/* Start the DMA transaction */
		sg_init_table(rx_sg, axi_adc_dev[minor]->bd_cnt);
		for (i = 0; i < axi_adc_dev[minor]->bd_cnt; i++)
		{
			sg_dma_address(&rx_sg[i]) = axi_adc_dev[minor]->dma_dsts[i];
			sg_dma_len(&rx_sg[i]) = axi_adc_dev[minor]->dma_len_bytes;
		}
		rxd = rx_dev->device_prep_slave_sg(axi_adc_dev[minor]->rx_chan, rx_sg, axi_adc_dev[minor]->bd_cnt,
		                                   DMA_DEV_TO_MEM, flags, NULL);
		if (!rxd)
		{
			dev_err(&axi_adc_dev[minor]->pdev->dev, "rxd is NULL\n");
			for (i = 0; i < axi_adc_dev[minor]->bd_cnt; i++)
				dma_unmap_single(rx_dev->dev, axi_adc_dev[minor]->dma_dsts[i],
				                 axi_adc_dev[minor]->dma_len_bytes,DMA_DEV_TO_MEM);
		}
		init_completion(&axi_adc_dev[minor]->rx_cmp);
		rxd->callback = dma_slave_rx_callback;
		rxd->callback_param = &axi_adc_dev[minor]->rx_cmp;
		axi_adc_dev[minor]->rx_cookie = rxd->tx_submit(rxd);

		if (dma_submit_error(axi_adc_dev[minor]->rx_cookie))
		{
			dev_err(&axi_adc_dev[minor]->pdev->dev, "dma submit error\n");
		}
		axi_adc_dev[minor]->rx_tmo =msecs_to_jiffies(AXI_ADC_CALLBACK_TIMEOUTMSEC); /* RX takes longer */
		dma_async_issue_pending(axi_adc_dev[minor]->rx_chan);
		writel(axi_adc_dev[minor]->adc_sample_num,axi_adc_dev[minor]->adc_virtaddr+4);
		writel(1,axi_adc_dev[minor]->adc_virtaddr);
		break;
	case AXI_ADC_DMA_DEINIT:
		break;
	default:
		return -EOPNOTSUPP;

	}
	return SUCCESS;
}

struct file_operations axi_adc_fops =
{
	.owner = THIS_MODULE,
	.read = axi_adc_dma_read,
	.open = axi_adc_dma_open,
	.unlocked_ioctl = axi_adc_dma_ioctl,
	.release = axi_adc_dma_release
};

static int axi_adc_remove(struct platform_device *pdev)
{
	int i;
	for(i = 0;i < dev_index;i++)
	{
		
		device_destroy(axi_adc_class,MKDEV(MAJOR(devno),i));

		/* Free up the DMA channel */
		dma_release_channel(axi_adc_dev[i]->rx_chan);

		/* Unmap the adc I/O memory */
		if (axi_adc_dev[i]->adc_virtaddr)
			iounmap(axi_adc_dev[i]->adc_virtaddr);


		if (axi_adc_dev[i])
		{
			kfree(axi_adc_dev[i]);
		}
		dev_info(&pdev->dev, "adc DMA Unload :: Success \n");
	}
	class_destroy(axi_adc_class);
	cdev_del(&adc_cdev);
	unregister_chrdev_region(devno, AXI_ADC_MINOR_COUNT);
	return SUCCESS;
}

static int axi_adc_probe(struct platform_device *pdev)
{
	int status = 0;

	struct device_node *node=NULL;

	/*Allocate device node */
	node = pdev->dev.of_node;

	/* Allocate a private structure to manage this device */
	axi_adc_dev[dev_index] = kmalloc(sizeof(struct axi_adc_dev), GFP_KERNEL);
	if (axi_adc_dev[dev_index] == NULL)
	{
		dev_err(&pdev->dev, "unable to allocate device structure\n");
		return -ENOMEM;
	}
	memset(axi_adc_dev[dev_index], 0, sizeof(struct axi_adc_dev));

	axi_adc_dev[dev_index]->rx_chan = dma_request_slave_channel(&pdev->dev, "axidma1");
	if (IS_ERR(axi_adc_dev[dev_index]->rx_chan))
	{
		dev_err(&pdev->dev, "No DMA Rx channel\n");
		goto free_rx;
	}

	if (axi_adc_dev[dev_index]->rx_chan == NULL)
	{
		dev_err(&pdev->dev, "No DMA Rx channel\n");
		goto fail1;
	}


	/* IOMAP adc registers */
	axi_adc_dev[dev_index]->adc_virtaddr = of_iomap(node, 0);
	if (!axi_adc_dev[dev_index]->adc_virtaddr)
	{
		dev_err(&pdev->dev, "unable to IOMAP adc registers\n");
		status = -ENOMEM;
		goto fail1;
	}


	axi_adc_dev[dev_index]->pdev = pdev;
	/* Initialize our device mutex */
	mutex_init(&axi_adc_dev[dev_index]->mutex);
	
	if(dev_index == 0)
	{

		status =alloc_chrdev_region(&devno,0, AXI_ADC_MINOR_COUNT,MODULE_NAME);
		if (status < 0)
		{
			dev_err(&pdev->dev, "unable to alloc chrdev \n");
			goto fail2;
		}

		/* Register with the kernel as a character device */
		cdev_init(&adc_cdev, &axi_adc_fops);
		adc_cdev.owner = THIS_MODULE;
		adc_cdev.ops = &axi_adc_fops;
		status = cdev_add(&adc_cdev,devno,AXI_ADC_MINOR_COUNT);
		axi_adc_class = class_create(THIS_MODULE, MODULE_NAME);
	
	}


	//Creating device node for each ADC channel
	device_create(axi_adc_class, NULL,
	              MKDEV(MAJOR(devno), dev_index),
	              NULL, adc_channels[dev_index]);


	dev_info(&pdev->dev, "ALINX PL adc added successfully\n");
	dev_index++;
	return SUCCESS;

fail2:
	iounmap(axi_adc_dev[dev_index]->adc_virtaddr);
free_rx:
	dma_release_channel(axi_adc_dev[dev_index]->rx_chan);
fail1:
	kfree(axi_adc_dev[dev_index]);
	return status;
}

static const struct of_device_id axi_adc_dma_of_ids[] =
{
	{.compatible = "alinx,axi-adc-dma",},
};

static struct platform_driver axi_adc_dma_of_driver =
{
	.driver = {
		.name = MODULE_NAME,
		.owner = THIS_MODULE,
		.of_match_table = axi_adc_dma_of_ids,
	},
	.probe = axi_adc_probe,
	 .remove = axi_adc_remove,
  };

module_platform_driver(axi_adc_dma_of_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("ALINX AXI adc DMA driver");
MODULE_AUTHOR("ALINX, Inc.");
MODULE_VERSION("1.00a");
