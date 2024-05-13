/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Definitions for Xilinx Axi Ethernet device driver.
 *
 * Copyright (c) 2009 Secret Lab Technologies, Ltd.
 * Copyright (c) 2010 - 2022 Xilinx, Inc. All rights reserved.
 * Copyright (c) 2022 Advanced Micro Devices, Inc.
 */

#ifndef XILINX_AXIENET_H
#define XILINX_AXIENET_H

#include <linux/clk.h>
#include <linux/netdevice.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/if_vlan.h>
#include <linux/phylink.h>
#include <linux/net_tstamp.h>
#include <linux/of_platform.h>

/* Packet size info */
#define XAE_HDR_SIZE			14 /* Size of Ethernet header */
#define XAE_TRL_SIZE			 4 /* Size of Ethernet trailer (FCS) */
#define XAE_MTU			      1500 /* Max MTU of an Ethernet frame */
#define XAE_JUMBO_MTU		      9000 /* Max MTU of a jumbo Eth. frame */

#define XAE_MAX_FRAME_SIZE	 (XAE_MTU + XAE_HDR_SIZE + XAE_TRL_SIZE)
#define XAE_MAX_VLAN_FRAME_SIZE  (XAE_MTU + VLAN_ETH_HLEN + XAE_TRL_SIZE)
#define XAE_MAX_JUMBO_FRAME_SIZE (XAE_JUMBO_MTU + XAE_HDR_SIZE + XAE_TRL_SIZE)

/* DMA address width min and max range */
#define XAE_DMA_MASK_MIN	32
#define XAE_DMA_MASK_MAX	64

/* In AXI DMA Tx and Rx queue count is same */
#define for_each_tx_dma_queue(lp, var) \
	for ((var) = 0; (var) < (lp)->num_tx_queues; (var)++)

#define for_each_rx_dma_queue(lp, var) \
	for ((var) = 0; (var) < (lp)->num_rx_queues; (var)++)
/* Configuration options */

/* Accept all incoming packets. Default: disabled (cleared) */
#define XAE_OPTION_PROMISC			BIT(0)

/* Jumbo frame support for Tx & Rx. Default: disabled (cleared) */
#define XAE_OPTION_JUMBO			BIT(1)

/* VLAN Rx & Tx frame support. Default: disabled (cleared) */
#define XAE_OPTION_VLAN				BIT(2)

/* Enable recognition of flow control frames on Rx. Default: enabled (set) */
#define XAE_OPTION_FLOW_CONTROL			BIT(4)

/* Strip FCS and PAD from incoming frames. Note: PAD from VLAN frames is not
 * stripped. Default: disabled (set)
 */
#define XAE_OPTION_FCS_STRIP			BIT(5)

/* Generate FCS field and add PAD automatically for outgoing frames.
 * Default: enabled (set)
 */
#define XAE_OPTION_FCS_INSERT			BIT(6)

/* Enable Length/Type error checking for incoming frames. When this option is
 * set, the MAC will filter frames that have a mismatched type/length field
 * and if XAE_OPTION_REPORT_RXERR is set, the user is notified when these
 * types of frames are encountered. When this option is cleared, the MAC will
 * allow these types of frames to be received. Default: enabled (set)
 */
#define XAE_OPTION_LENTYPE_ERR			BIT(7)

/* Enable the transmitter. Default: enabled (set) */
#define XAE_OPTION_TXEN				BIT(11)

/*  Enable the receiver. Default: enabled (set) */
#define XAE_OPTION_RXEN				BIT(12)

/*  Default options set when device is initialized or reset */
#define XAE_OPTION_DEFAULTS				   \
				(XAE_OPTION_TXEN |	   \
				 XAE_OPTION_FLOW_CONTROL | \
				 XAE_OPTION_RXEN)

/* Axi DMA Register definitions */

#define XAXIDMA_TX_CR_OFFSET	0x00000000 /* Channel control */
#define XAXIDMA_TX_SR_OFFSET	0x00000004 /* Status */
#define XAXIDMA_TX_CDESC_OFFSET	0x00000008 /* Current descriptor pointer */
#define XAXIDMA_TX_TDESC_OFFSET	0x00000010 /* Tail descriptor pointer */

#define XAXIDMA_RX_CR_OFFSET	0x00000030 /* Channel control */
#define XAXIDMA_RX_SR_OFFSET	0x00000034 /* Status */
#define XAXIDMA_RX_CDESC_OFFSET	0x00000038 /* Current descriptor pointer */
#define XAXIDMA_RX_TDESC_OFFSET	0x00000040 /* Tail descriptor pointer */

#define XAXIDMA_CR_RUNSTOP_MASK	0x00000001 /* Start/stop DMA channel */
#define XAXIDMA_CR_RESET_MASK	0x00000004 /* Reset DMA engine */

#define XAXIDMA_SR_HALT_MASK	0x00000001 /* Indicates DMA channel halted */

#define XAXIDMA_BD_NDESC_OFFSET		0x00 /* Next descriptor pointer */
#define XAXIDMA_BD_BUFA_OFFSET		0x08 /* Buffer address */
#define XAXIDMA_BD_CTRL_LEN_OFFSET	0x18 /* Control/buffer length */
#define XAXIDMA_BD_STS_OFFSET		0x1C /* Status */
#define XAXIDMA_BD_USR0_OFFSET		0x20 /* User IP specific word0 */
#define XAXIDMA_BD_USR1_OFFSET		0x24 /* User IP specific word1 */
#define XAXIDMA_BD_USR2_OFFSET		0x28 /* User IP specific word2 */
#define XAXIDMA_BD_USR3_OFFSET		0x2C /* User IP specific word3 */
#define XAXIDMA_BD_USR4_OFFSET		0x30 /* User IP specific word4 */
#define XAXIDMA_BD_ID_OFFSET		0x34 /* Sw ID */
#define XAXIDMA_BD_HAS_STSCNTRL_OFFSET	0x38 /* Whether has stscntrl strm */
#define XAXIDMA_BD_HAS_DRE_OFFSET	0x3C /* Whether has DRE */

#define XAXIDMA_BD_HAS_DRE_SHIFT	8 /* Whether has DRE shift */
#define XAXIDMA_BD_HAS_DRE_MASK		0xF00 /* Whether has DRE mask */
#define XAXIDMA_BD_WORDLEN_MASK		0xFF /* Whether has DRE mask */

#define XAXIDMA_BD_CTRL_LENGTH_MASK	0x007FFFFF /* Requested len */
#define XAXIDMA_BD_CTRL_TXSOF_MASK	0x08000000 /* First tx packet */
#define XAXIDMA_BD_CTRL_TXEOF_MASK	0x04000000 /* Last tx packet */
#define XAXIDMA_BD_CTRL_ALL_MASK	0x0C000000 /* All control bits */

#define XAXIDMA_DELAY_MASK		0xFF000000 /* Delay timeout counter */
#define XAXIDMA_COALESCE_MASK		0x00FF0000 /* Coalesce counter */

#define XAXIDMA_DELAY_SHIFT		24
#define XAXIDMA_COALESCE_SHIFT		16

#define XAXIDMA_IRQ_IOC_MASK		0x00001000 /* Completion intr */
#define XAXIDMA_IRQ_DELAY_MASK		0x00002000 /* Delay interrupt */
#define XAXIDMA_IRQ_ERROR_MASK		0x00004000 /* Error interrupt */
#define XAXIDMA_IRQ_ALL_MASK		0x00007000 /* All interrupts */

/* Default TX/RX Threshold and delay timer values for SGDMA mode */
#define XAXIDMA_DFT_TX_THRESHOLD	24
#define XAXIDMA_DFT_TX_USEC		50
#define XAXIDMA_DFT_RX_THRESHOLD	1
#define XAXIDMA_DFT_RX_USEC		50

#define XAXIDMA_BD_CTRL_TXSOF_MASK	0x08000000 /* First tx packet */
#define XAXIDMA_BD_CTRL_TXEOF_MASK	0x04000000 /* Last tx packet */
#define XAXIDMA_BD_CTRL_ALL_MASK	0x0C000000 /* All control bits */

#define XAXIDMA_BD_STS_ACTUAL_LEN_MASK	0x007FFFFF /* Actual len */
#define XAXIDMA_BD_STS_COMPLETE_MASK	0x80000000 /* Completed */
#define XAXIDMA_BD_STS_DEC_ERR_MASK	0x40000000 /* Decode error */
#define XAXIDMA_BD_STS_SLV_ERR_MASK	0x20000000 /* Slave error */
#define XAXIDMA_BD_STS_INT_ERR_MASK	0x10000000 /* Internal err */
#define XAXIDMA_BD_STS_ALL_ERR_MASK	0x70000000 /* All errors */
#define XAXIDMA_BD_STS_RXSOF_MASK	0x08000000 /* First rx pkt */
#define XAXIDMA_BD_STS_RXEOF_MASK	0x04000000 /* Last rx pkt */
#define XAXIDMA_BD_STS_ALL_MASK		0xFC000000 /* All status bits */

#define XAXIDMA_BD_MINIMUM_ALIGNMENT	0x40

/* AXI Tx Timestamp Stream FIFO Register Definitions */
#define XAXIFIFO_TXTS_ISR	0x00000000 /* Interrupt Status Register */
#define XAXIFIFO_TXTS_TDFV	0x0000000C /* Transmit Data FIFO Vacancy */
#define XAXIFIFO_TXTS_TXFD	0x00000010 /* Tx Data Write Port */
#define XAXIFIFO_TXTS_TLR	0x00000014 /* Transmit Length Register */
#define XAXIFIFO_TXTS_RFO	0x0000001C /* Rx Fifo Occupancy */
#define XAXIFIFO_TXTS_RDFR	0x00000018 /* Rx Fifo reset */
#define XAXIFIFO_TXTS_RXFD	0x00000020 /* Rx Data Read Port */
#define XAXIFIFO_TXTS_RLR	0x00000024 /* Receive Length Register */
#define XAXIFIFO_TXTS_SRR	0x00000028 /* AXI4-Stream Reset */

#define XAXIFIFO_TXTS_INT_RC_MASK	0x04000000
#define XAXIFIFO_TXTS_RXFD_MASK		0x7FFFFFFF
#define XAXIFIFO_TXTS_RESET_MASK	0x000000A5
#define XAXIFIFO_TXTS_TAG_MASK		0xFFFF0000
#define XAXIFIFO_TXTS_TAG_SHIFT		16
#define XAXIFIFO_TXTS_TAG_MAX		0xFFFE

/* Axi Ethernet registers definition */
#define XAE_RAF_OFFSET		0x00000000 /* Reset and Address filter */
#define XAE_TPF_OFFSET		0x00000004 /* Tx Pause Frame */
#define XAE_IFGP_OFFSET		0x00000008 /* Tx Inter-frame gap adjustment*/
#define XAE_IS_OFFSET		0x0000000C /* Interrupt status */
#define XAE_IP_OFFSET		0x00000010 /* Interrupt pending */
#define XAE_IE_OFFSET		0x00000014 /* Interrupt enable */
#define XAE_TTAG_OFFSET		0x00000018 /* Tx VLAN TAG */
#define XAE_RTAG_OFFSET		0x0000001C /* Rx VLAN TAG */
#define XAE_UAWL_OFFSET		0x00000020 /* Unicast address word lower */
#define XAE_UAWU_OFFSET		0x00000024 /* Unicast address word upper */
#define XAE_TPID0_OFFSET	0x00000028 /* VLAN TPID0 register */
#define XAE_TPID1_OFFSET	0x0000002C /* VLAN TPID1 register */
#define XAE_PPST_OFFSET		0x00000030 /* PCS PMA Soft Temac Status Reg */
#define XAE_RCW0_OFFSET		0x00000400 /* Rx Configuration Word 0 */
#define XAE_RCW1_OFFSET		0x00000404 /* Rx Configuration Word 1 */
#define XAE_TC_OFFSET		0x00000408 /* Tx Configuration */
#define XAE_FCC_OFFSET		0x0000040C /* Flow Control Configuration */
#define XAE_ID_OFFSET		0x000004F8 /* Identification register */
#define XAE_EMMC_OFFSET		0x00000410 /* MAC speed configuration */
#define XAE_RMFC_OFFSET		0x00000414 /* RX Max Frame Configuration */
#define XAE_MDIO_MC_OFFSET	0x00000500 /* MDIO Setup */
#define XAE_MDIO_MCR_OFFSET	0x00000504 /* MDIO Control */
#define XAE_MDIO_MWD_OFFSET	0x00000508 /* MDIO Write Data */
#define XAE_MDIO_MRD_OFFSET	0x0000050C /* MDIO Read Data */
#define XAE_TEMAC_IS_OFFSET	0x00000600 /* TEMAC Interrupt Status */
#define XAE_TEMAC_IP_OFFSET	0x00000610 /* TEMAC Interrupt Pending Status */
#define XAE_TEMAC_IE_OFFSET	0x00000620 /* TEMAC Interrupt Enable Status */
#define XAE_TEMAC_IC_OFFSET	0x00000630 /* TEMAC Interrupt Clear Status */
#define XAE_UAW0_OFFSET		0x00000700 /* Unicast address word 0 */
#define XAE_UAW1_OFFSET		0x00000704 /* Unicast address word 1 */
#define XAE_FMC_OFFSET		0x00000708 /* Frame Filter Control */
#define XAE_AF0_OFFSET		0x00000710 /* Address Filter 0 */
#define XAE_AF1_OFFSET		0x00000714 /* Address Filter 1 */

#define XAE_TX_VLAN_DATA_OFFSET 0x00004000 /* TX VLAN data table address */
#define XAE_RX_VLAN_DATA_OFFSET 0x00008000 /* RX VLAN data table address */
#define XAE_MCAST_TABLE_OFFSET	0x00020000 /* Multicast table address */

/* Bit Masks for Axi Ethernet RAF register */
/* Reject receive multicast destination address */
#define XAE_RAF_MCSTREJ_MASK		0x00000002
/* Reject receive broadcast destination address */
#define XAE_RAF_BCSTREJ_MASK		0x00000004
#define XAE_RAF_TXVTAGMODE_MASK		0x00000018 /* Tx VLAN TAG mode */
#define XAE_RAF_RXVTAGMODE_MASK		0x00000060 /* Rx VLAN TAG mode */
#define XAE_RAF_TXVSTRPMODE_MASK	0x00000180 /* Tx VLAN STRIP mode */
#define XAE_RAF_RXVSTRPMODE_MASK	0x00000600 /* Rx VLAN STRIP mode */
#define XAE_RAF_NEWFNCENBL_MASK		0x00000800 /* New function mode */
/* Extended Multicast Filtering mode */
#define XAE_RAF_EMULTIFLTRENBL_MASK	0x00001000
#define XAE_RAF_STATSRST_MASK		0x00002000 /* Stats. Counter Reset */
#define XAE_RAF_RXBADFRMEN_MASK		0x00004000 /* Recv Bad Frame Enable */
#define XAE_RAF_TXVTAGMODE_SHIFT	3 /* Tx Tag mode shift bits */
#define XAE_RAF_RXVTAGMODE_SHIFT	5 /* Rx Tag mode shift bits */
#define XAE_RAF_TXVSTRPMODE_SHIFT	7 /* Tx strip mode shift bits*/
#define XAE_RAF_RXVSTRPMODE_SHIFT	9 /* Rx Strip mode shift bits*/

/* Bit Masks for Axi Ethernet TPF and IFGP registers */
#define XAE_TPF_TPFV_MASK		0x0000FFFF /* Tx pause frame value */
/* Transmit inter-frame gap adjustment value */
#define XAE_IFGP0_IFGP_MASK		0x0000007F

/* Bit Masks for Axi Ethernet IS, IE and IP registers, Same masks apply
 * for all 3 registers.
 */
/* Hard register access complete */
#define XAE_INT_HARDACSCMPLT_MASK	0x00000001
/* Auto negotiation complete */
#define XAE_INT_AUTONEG_MASK		0x00000002
#define XAE_INT_RXCMPIT_MASK		0x00000004 /* Rx complete */
#define XAE_INT_RXRJECT_MASK		0x00000008 /* Rx frame rejected */
#define XAE_INT_RXFIFOOVR_MASK		0x00000010 /* Rx fifo overrun */
#define XAE_INT_TXCMPIT_MASK		0x00000020 /* Tx complete */
#define XAE_INT_RXDCMLOCK_MASK		0x00000040 /* Rx Dcm Lock */
#define XAE_INT_MGTRDY_MASK		0x00000080 /* MGT clock Lock */
#define XAE_INT_PHYRSTCMPLT_MASK	0x00000100 /* Phy Reset complete */
#define XAE_INT_ALL_MASK		0x0000003F /* All the ints */

/* INT bits that indicate receive errors */
#define XAE_INT_RECV_ERROR_MASK				\
	(XAE_INT_RXRJECT_MASK | XAE_INT_RXFIFOOVR_MASK)

/* Bit masks for Axi Ethernet VLAN TPID Word 0 register */
#define XAE_TPID_0_MASK		0x0000FFFF /* TPID 0 */
#define XAE_TPID_1_MASK		0xFFFF0000 /* TPID 1 */

/* Bit masks for Axi Ethernet VLAN TPID Word 1 register */
#define XAE_TPID_2_MASK		0x0000FFFF /* TPID 0 */
#define XAE_TPID_3_MASK		0xFFFF0000 /* TPID 1 */

/* Bit masks for Axi Ethernet RCW1 register */
#define XAE_RCW1_INBAND1588_MASK 0x00400000 /* Inband 1588 Enable */
#define XAE_RCW1_RST_MASK	0x80000000 /* Reset */
#define XAE_RCW1_JUM_MASK	0x40000000 /* Jumbo frame enable */
/* In-Band FCS enable (FCS not stripped) */
#define XAE_RCW1_FCS_MASK	0x20000000
#define XAE_RCW1_RX_MASK	0x10000000 /* Receiver enable */
#define XAE_RCW1_VLAN_MASK	0x08000000 /* VLAN frame enable */
/* Length/type field valid check disable */
#define XAE_RCW1_LT_DIS_MASK	0x02000000
/* Control frame Length check disable */
#define XAE_RCW1_CL_DIS_MASK	0x01000000
/* Pause frame source address bits [47:32]. Bits [31:0] are
 * stored in register RCW0
 */
#define XAE_RCW1_PAUSEADDR_MASK 0x0000FFFF

/* Bit masks for Axi Ethernet TC register */
#define XAE_TC_INBAND1588_MASK 0x00400000 /* Inband 1588 Enable */
#define XAE_TC_RST_MASK		0x80000000 /* Reset */
#define XAE_TC_JUM_MASK		0x40000000 /* Jumbo frame enable */
/* In-Band FCS enable (FCS not generated) */
#define XAE_TC_FCS_MASK		0x20000000
#define XAE_TC_TX_MASK		0x10000000 /* Transmitter enable */
#define XAE_TC_VLAN_MASK	0x08000000 /* VLAN frame enable */
/* Inter-frame gap adjustment enable */
#define XAE_TC_IFG_MASK		0x02000000

/* Bit masks for Axi Ethernet FCC register */
#define XAE_FCC_FCRX_MASK	0x20000000 /* Rx flow control enable */
#define XAE_FCC_FCTX_MASK	0x40000000 /* Tx flow control enable */

/* Bit masks for Axi Ethernet EMMC register */
#define XAE_EMMC_LINKSPEED_MASK	0xC0000000 /* Link speed */
#define XAE_EMMC_RGMII_MASK	0x20000000 /* RGMII mode enable */
#define XAE_EMMC_SGMII_MASK	0x10000000 /* SGMII mode enable */
#define XAE_EMMC_GPCS_MASK	0x08000000 /* 1000BaseX mode enable */
#define XAE_EMMC_HOST_MASK	0x04000000 /* Host interface enable */
#define XAE_EMMC_TX16BIT	0x02000000 /* 16 bit Tx client enable */
#define XAE_EMMC_RX16BIT	0x01000000 /* 16 bit Rx client enable */
#define XAE_EMMC_LINKSPD_10	0x00000000 /* Link Speed mask for 10 Mbit */
#define XAE_EMMC_LINKSPD_100	0x40000000 /* Link Speed mask for 100 Mbit */
#define XAE_EMMC_LINKSPD_1000	0x80000000 /* Link Speed mask for 1000 Mbit */
#define XAE_EMMC_LINKSPD_2500	0x80000000 /* Link Speed mask for 2500 Mbit */

/* Bit masks for Axi Ethernet MDIO interface MC register */
#define XAE_MDIO_MC_MDIOEN_MASK		0x00000040 /* MII management enable */
#define XAE_MDIO_MC_CLOCK_DIVIDE_MAX	0x3F	   /* Maximum MDIO divisor */

/* Bit masks for Axi Ethernet MDIO interface MCR register */
#define XAE_MDIO_MCR_PHYAD_MASK		0x1F000000 /* Phy Address Mask */
#define XAE_MDIO_MCR_PHYAD_SHIFT	24	   /* Phy Address Shift */
#define XAE_MDIO_MCR_REGAD_MASK		0x001F0000 /* Reg Address Mask */
#define XAE_MDIO_MCR_REGAD_SHIFT	16	   /* Reg Address Shift */
#define XAE_MDIO_MCR_OP_MASK		0x0000C000 /* Operation Code Mask */
#define XAE_MDIO_MCR_OP_SHIFT		13	   /* Operation Code Shift */
#define XAE_MDIO_MCR_OP_READ_MASK	0x00008000 /* Op Code Read Mask */
#define XAE_MDIO_MCR_OP_WRITE_MASK	0x00004000 /* Op Code Write Mask */
#define XAE_MDIO_MCR_INITIATE_MASK	0x00000800 /* Ready Mask */
#define XAE_MDIO_MCR_READY_MASK		0x00000080 /* Ready Mask */

/* Bit masks for Axi Ethernet UAW1 register */
/* Station address bits [47:32]; Station address
 * bits [31:0] are stored in register UAW0
 */
#define XAE_UAW1_UNICASTADDR_MASK	0x0000FFFF

/* Bit masks for Axi Ethernet FMC register */
#define XAE_FMC_PM_MASK			0x80000000 /* Promis. mode enable */
#define XAE_FMC_IND_MASK		0x00000003 /* Index Mask */

#define XAE_MDIO_DIV_DFT		29 /* Default MDIO clock divisor */

/* Total number of entries in the hardware multicast table. */
#define XAE_MULTICAST_CAM_TABLE_NUM	4

/* Axi Ethernet Synthesis features */
#define XAE_FEATURE_PARTIAL_RX_CSUM	BIT(0)
#define XAE_FEATURE_PARTIAL_TX_CSUM	BIT(1)
#define XAE_FEATURE_FULL_RX_CSUM	BIT(2)
#define XAE_FEATURE_FULL_TX_CSUM	BIT(3)
#define XAE_FEATURE_DMA_64BIT		BIT(4)

#define XAE_NO_CSUM_OFFLOAD		0

#define XAE_FULL_CSUM_STATUS_MASK	0x00000038
#define XAE_IP_UDP_CSUM_VALIDATED	0x00000003
#define XAE_IP_TCP_CSUM_VALIDATED	0x00000002

#define DELAY_OF_ONE_MILLISEC		1000

/* Xilinx PCS/PMA PHY register for switching 1000BaseX or SGMII */
#define XLNX_MII_STD_SELECT_REG		0x11
#define XLNX_MII_STD_SELECT_SGMII	BIT(0)
#define XAXIENET_NAPI_WEIGHT		64

/* Definition of 1588 PTP in Axi Ethernet IP */
#define TX_TS_OP_NOOP           0x0
#define TX_TS_OP_ONESTEP        0x1
#define TX_TS_OP_TWOSTEP        0x2
#define TX_TS_CSUM_UPDATE       0x1
#define TX_TS_CSUM_UPDATE_MRMAC		0x4
#define TX_TS_PDELAY_UPDATE_MRMAC	0x8
#define TX_PTP_CSUM_OFFSET      0x28
#define TX_PTP_TS_OFFSET        0x4C
#define TX_PTP_CF_OFFSET        0x32

/* XXV MAC Register Definitions */
#define XXV_GT_RESET_OFFSET		0x00000000
#define XXV_TC_OFFSET			0x0000000C
#define XXV_RCW1_OFFSET			0x00000014
#define XXV_JUM_OFFSET			0x00000018
#define XXV_TICKREG_OFFSET		0x00000020
#define XXV_STATRX_BLKLCK_OFFSET	0x0000040C
#define XXV_USXGMII_AN_OFFSET		0x000000C8
#define XXV_USXGMII_AN_STS_OFFSET	0x00000458
#define XXV_STAT_GTWIZ_OFFSET		0x000004A0
#define XXV_CONFIG_REVISION		0x00000024

/* XXV MAC Register Mask Definitions */
#define XXV_GT_RESET_MASK	BIT(0)
#define XXV_TC_TX_MASK		BIT(0)
#define XXV_RCW1_RX_MASK	BIT(0)
#define XXV_RCW1_FCS_MASK	BIT(1)
#define XXV_TC_FCS_MASK		BIT(1)
#define XXV_MIN_JUM_MASK	GENMASK(7, 0)
#define XXV_MAX_JUM_MASK	GENMASK(10, 8)
#define XXV_RX_BLKLCK_MASK	BIT(0)
#define XXV_TICKREG_STATEN_MASK BIT(0)
#define XXV_MAC_MIN_PKT_LEN	64
#define XXV_GTWIZ_RESET_DONE	(BIT(0) | BIT(1))
#define XXV_MAJ_MASK		GENMASK(7, 0)
#define XXV_MIN_MASK		GENMASK(15, 8)

/* USXGMII Register Mask Definitions  */
#define USXGMII_AN_EN		BIT(5)
#define USXGMII_AN_RESET	BIT(6)
#define USXGMII_AN_RESTART	BIT(7)
#define USXGMII_EN		BIT(16)
#define USXGMII_RATE_MASK	0x0E000700
#define USXGMII_RATE_1G		0x04000200
#define USXGMII_RATE_2G5	0x08000400
#define USXGMII_RATE_10M	0x0
#define USXGMII_RATE_100M	0x02000100
#define USXGMII_RATE_5G		0x0A000500
#define USXGMII_RATE_10G	0x06000300
#define USXGMII_FD		BIT(28)
#define USXGMII_LINK_STS	BIT(31)

/* USXGMII AN STS register mask definitions */
#define USXGMII_AN_STS_COMP_MASK	BIT(16)

/* MCDMA Register Definitions */
#define XMCDMA_CR_OFFSET	0x00
#define XMCDMA_SR_OFFSET	0x04
#define XMCDMA_CHEN_OFFSET	0x08
#define XMCDMA_CHSER_OFFSET	0x0C
#define XMCDMA_ERR_OFFSET	0x10
#define XMCDMA_PKTDROP_OFFSET	0x14
#define XMCDMA_TXWEIGHT0_OFFSET 0x18
#define XMCDMA_TXWEIGHT1_OFFSET 0x1C
#define XMCDMA_RXINT_SER_OFFSET 0x20
#define XMCDMA_TXINT_SER_OFFSET 0x28

#define XMCDMA_CHOBS1_OFFSET	0x440
#define XMCDMA_CHOBS2_OFFSET	0x444
#define XMCDMA_CHOBS3_OFFSET	0x448
#define XMCDMA_CHOBS4_OFFSET	0x44C
#define XMCDMA_CHOBS5_OFFSET	0x450
#define XMCDMA_CHOBS6_OFFSET	0x454

#define XMCDMA_CHAN_RX_OFFSET  0x500

/* Per Channel Registers */
#define XMCDMA_CHAN_CR_OFFSET(chan_id)		(0x40 + ((chan_id) - 1) * 0x40)
#define XMCDMA_CHAN_SR_OFFSET(chan_id)		(0x44 + ((chan_id) - 1) * 0x40)
#define XMCDMA_CHAN_CURDESC_OFFSET(chan_id)	(0x48 + ((chan_id) - 1) * 0x40)
#define XMCDMA_CHAN_TAILDESC_OFFSET(chan_id)	(0x50 + ((chan_id) - 1) * 0x40)
#define XMCDMA_CHAN_PKTDROP_OFFSET(chan_id)	(0x58 + ((chan_id) - 1) * 0x40)

#define XMCDMA_RX_OFFSET	0x500

/* MCDMA Mask registers */
#define XMCDMA_CR_RUNSTOP_MASK		BIT(0) /* Start/stop DMA channel */
#define XMCDMA_CR_RESET_MASK		BIT(2) /* Reset DMA engine */

#define XMCDMA_SR_HALTED_MASK		BIT(0)
#define XMCDMA_SR_IDLE_MASK		BIT(1)

#define XMCDMA_IRQ_ERRON_OTHERQ_MASK	BIT(3)
#define XMCDMA_IRQ_PKTDROP_MASK		BIT(4)
#define XMCDMA_IRQ_IOC_MASK		BIT(5)
#define XMCDMA_IRQ_DELAY_MASK		BIT(6)
#define XMCDMA_IRQ_ERR_MASK		BIT(7)
#define XMCDMA_IRQ_ALL_MASK		GENMASK(7, 5)
#define XMCDMA_PKTDROP_COALESCE_MASK	GENMASK(15, 8)
#define XMCDMA_COALESCE_MASK		GENMASK(23, 16)
#define XMCDMA_DELAY_MASK		GENMASK(31, 24)

#define XMCDMA_CHEN_MASK		GENMASK(7, 0)
#define XMCDMA_CHID_MASK		GENMASK(7, 0)

#define XMCDMA_ERR_INTERNAL_MASK	BIT(0)
#define XMCDMA_ERR_SLAVE_MASK		BIT(1)
#define XMCDMA_ERR_DECODE_MASK		BIT(2)
#define XMCDMA_ERR_SG_INT_MASK		BIT(4)
#define XMCDMA_ERR_SG_SLV_MASK		BIT(5)
#define XMCDMA_ERR_SG_DEC_MASK		BIT(6)

#define XMCDMA_PKTDROP_CNT_MASK		GENMASK(31, 0)

#define XMCDMA_BD_CTRL_TXSOF_MASK	0x80000000 /* First tx packet */
#define XMCDMA_BD_CTRL_TXEOF_MASK	0x40000000 /* Last tx packet */
#define XMCDMA_BD_CTRL_ALL_MASK		0xC0000000 /* All control bits */
#define XMCDMA_BD_STS_ALL_MASK		0xF0000000 /* All status bits */

#define XMCDMA_COALESCE_SHIFT		16
#define XMCDMA_DELAY_SHIFT		24
#define XMCDMA_DFT_TX_THRESHOLD		1

#define XMCDMA_TXWEIGHT_CH_MASK(chan_id)	GENMASK(((chan_id) * 4 + 3), \
							(chan_id) * 4)
#define XMCDMA_TXWEIGHT_CH_SHIFT(chan_id)	((chan_id) * 4)

/* PTP Packet length */
#define XAE_TX_PTP_LEN		16
#define XXV_TX_PTP_LEN		12

/* Macros used when AXI DMA h/w is configured without DRE */
#define XAE_TX_BUFFERS		64
#define XAE_MAX_PKT_LEN		8192

/* MRMAC Register Definitions */
/* Configuration Registers */
#define MRMAC_REV_OFFSET		0x00000000
#define MRMAC_RESET_OFFSET		0x00000004
#define MRMAC_MODE_OFFSET		0x00000008
#define MRMAC_CONFIG_TX_OFFSET		0x0000000C
#define MRMAC_CONFIG_RX_OFFSET		0x00000010
#define MRMAC_TICK_OFFSET		0x0000002C
#define MRMAC_CFG1588_OFFSET	0x00000040

/* Status Registers */
#define MRMAC_TX_STS_OFFSET		0x00000740
#define MRMAC_RX_STS_OFFSET		0x00000744
#define MRMAC_TX_RT_STS_OFFSET		0x00000748
#define MRMAC_RX_RT_STS_OFFSET		0x0000074C
#define MRMAC_STATRX_BLKLCK_OFFSET	0x00000754
#define MRMAC_STATRX_VALID_CTRL_OFFSET	0x000007B8

/* Register bit masks */
#define MRMAC_RX_SERDES_RST_MASK	(BIT(3) | BIT(2) | BIT(1) | BIT(0))
#define MRMAC_TX_SERDES_RST_MASK	BIT(4)
#define MRMAC_RX_RST_MASK		BIT(5)
#define MRMAC_TX_RST_MASK		BIT(6)
#define MRMAC_RX_AXI_RST_MASK		BIT(8)
#define MRMAC_TX_AXI_RST_MASK		BIT(9)
#define MRMAC_STS_ALL_MASK		0xFFFFFFFF

#define MRMAC_RX_EN_MASK		BIT(0)
#define MRMAC_RX_DEL_FCS_MASK		BIT(1)

#define MRMAC_TX_EN_MASK		BIT(0)
#define MRMAC_TX_INS_FCS_MASK		BIT(1)

#define MRMAC_RX_BLKLCK_MASK		BIT(0)
#define MRMAC_RX_STATUS_MASK		BIT(0)
#define MRMAC_RX_VALID_MASK		BIT(0)

#define MRMAC_CTL_DATA_RATE_MASK	GENMASK(2, 0)
#define MRMAC_CTL_DATA_RATE_10G		0
#define MRMAC_CTL_DATA_RATE_25G		1
#define MRMAC_CTL_DATA_RATE_40G		2
#define MRMAC_CTL_DATA_RATE_50G		3
#define MRMAC_CTL_DATA_RATE_100G	4

#define MRMAC_CTL_AXIS_CFG_MASK		GENMASK(11, 9)
#define MRMAC_CTL_AXIS_CFG_SHIFT	9
#define MRMAC_CTL_AXIS_CFG_10G_IND	1
#define MRMAC_CTL_AXIS_CFG_25G_IND	1

#define MRMAC_CTL_SERDES_WIDTH_MASK	GENMASK(6, 4)
#define MRMAC_CTL_SERDES_WIDTH_SHIFT	4
#define MRMAC_CTL_SERDES_WIDTH_10G	4
#define MRMAC_CTL_SERDES_WIDTH_25G	6

#define MRMAC_CTL_RATE_CFG_MASK		(MRMAC_CTL_DATA_RATE_MASK |	\
					 MRMAC_CTL_AXIS_CFG_MASK |	\
					 MRMAC_CTL_SERDES_WIDTH_MASK)

#define MRMAC_CTL_PM_TICK_MASK		BIT(30)
#define MRMAC_TICK_TRIGGER		BIT(0)
#define MRMAC_ONE_STEP_EN		BIT(0)

/* MRMAC GT wrapper registers */
#define MRMAC_GT_PLL_OFFSET		0x0
#define MRMAC_GT_PLL_STS_OFFSET		0x8
#define MRMAC_GT_RATE_OFFSET		0x0
#define MRMAC_GT_CTRL_OFFSET		0x8

#define MRMAC_GT_PLL_RST_MASK		0x00030003
#define MRMAC_GT_PLL_DONE_MASK		0xFF
#define MRMAC_GT_RST_ALL_MASK		BIT(0)
#define MRMAC_GT_RST_RX_MASK		BIT(1)
#define MRMAC_GT_RST_TX_MASK		BIT(2)
#define MRMAC_GT_10G_MASK		0x00000001
#define MRMAC_GT_25G_MASK		0x00000002

#define MRMAC_GT_LANE_OFFSET		BIT(16)
#define MRMAC_MAX_GT_LANES		4
/**
 * struct axidma_bd - Axi Dma buffer descriptor layout
 * @next:         MM2S/S2MM Next Descriptor Pointer
 * @reserved1:    Reserved and not used for 32-bit
 * @phys:         MM2S/S2MM Buffer Address
 * @reserved2:    Reserved and not used for 32-bit
 * @reserved3:    Reserved and not used
 * @reserved4:    Reserved and not used
 * @cntrl:        MM2S/S2MM Control value
 * @status:       MM2S/S2MM Status value
 * @app0:         MM2S/S2MM User Application Field 0.
 * @app1:         MM2S/S2MM User Application Field 1.
 * @app2:         MM2S/S2MM User Application Field 2.
 * @app3:         MM2S/S2MM User Application Field 3.
 * @app4:         MM2S/S2MM User Application Field 4.
 * @sw_id_offset: MM2S/S2MM Sw ID
 * @ptp_tx_skb:   If timestamping is enabled used for timestamping skb
 *		  Otherwise reserved.
 * @ptp_tx_ts_tag: Tag value of 2 step timestamping if timestamping is enabled
 *		   Otherwise reserved.
 * @tx_skb:	  Transmit skb address
 * @tx_desc_mapping: Tx Descriptor DMA mapping type.
 */
struct axidma_bd {
	phys_addr_t next;	/* Physical address of next buffer descriptor */
#ifndef CONFIG_PHYS_ADDR_T_64BIT
	u32 reserved1;
#endif
	phys_addr_t phys;
#ifndef CONFIG_PHYS_ADDR_T_64BIT
	u32 reserved2;
#endif
	u32 reserved3;
	u32 reserved4;
	u32 cntrl;
	u32 status;
	u32 app0;
	u32 app1;	/* TX start << 16 | insert */
	u32 app2;	/* TX csum seed */
	u32 app3;
	u32 app4;
	phys_addr_t sw_id_offset; /* first unused field by h/w */
	phys_addr_t ptp_tx_skb;
	u32 ptp_tx_ts_tag;
	phys_addr_t tx_skb;
	u32 tx_desc_mapping;
} __aligned(XAXIDMA_BD_MINIMUM_ALIGNMENT);
/**
 * struct aximcdma_bd - Axi MCDMA buffer descriptor layout
 * @next:         MM2S/S2MM Next Descriptor Pointer
 * @reserved1:    Reserved and not used for 32-bit
 * @phys:         MM2S/S2MM Buffer Address
 * @reserved2:    Reserved and not used for 32-bit
 * @reserved3:    Reserved and not used
 * @cntrl:        MM2S/S2MM Control value
 * @status:       S2MM Status value
 * @sband_stats:  S2MM Sideband Status value
 *		  MM2S Status value
 * @app0:         MM2S/S2MM User Application Field 0.
 * @app1:         MM2S/S2MM User Application Field 1.
 * @app2:         MM2S/S2MM User Application Field 2.
 * @app3:         MM2S/S2MM User Application Field 3.
 * @app4:         MM2S/S2MM User Application Field 4.
 * @sw_id_offset: MM2S/S2MM Sw ID
 * @ptp_tx_skb:   If timestamping is enabled used for timestamping skb
 *		  Otherwise reserved.
 * @ptp_tx_ts_tag: Tag value of 2 step timestamping if timestamping is enabled
 *		   Otherwise reserved.
 * @tx_skb:	  Transmit skb address
 * @tx_desc_mapping: Tx Descriptor DMA mapping type.
 */
struct aximcdma_bd {
	phys_addr_t next;	/* Physical address of next buffer descriptor */
#ifndef CONFIG_PHYS_ADDR_T_64BIT
	u32 reserved1;
#endif
	phys_addr_t phys;
#ifndef CONFIG_PHYS_ADDR_T_64BIT
	u32 reserved2;
#endif
	u32 reserved3;
	u32 cntrl;
	u32 status;
	u32 sband_stats;
	u32 app0;
	u32 app1;	/* TX start << 16 | insert */
	u32 app2;	/* TX csum seed */
	u32 app3;
	u32 app4;
	phys_addr_t sw_id_offset; /* first unused field by h/w */
	phys_addr_t ptp_tx_skb;
	u32 ptp_tx_ts_tag;
	phys_addr_t tx_skb;
	u32 tx_desc_mapping;
} __aligned(XAXIDMA_BD_MINIMUM_ALIGNMENT);

#define XAE_NUM_MISC_CLOCKS 3
#define DESC_DMA_MAP_SINGLE 0
#define DESC_DMA_MAP_PAGE 1

#if defined(CONFIG_AXIENET_HAS_MCDMA)
#define XAE_MAX_QUEUES		16
#else
#define XAE_MAX_QUEUES		1
#endif
/**
 * struct axienet_local - axienet private per device data
 * @ndev:	Pointer for net_device to which it will be attached.
 * @dev:	Pointer to device structure
 * @phy_node:	Pointer to device node structure
 * @phylink:	Pointer to phylink instance
 * @phylink_config: phylink configuration settings
 * @pcs_phy:	Reference to PCS/PMA PHY if used
 * @pcs:	phylink pcs structure for PCS PHY
 * @switch_x_sgmii: Whether switchable 1000BaseX/SGMII mode is enabled in the core
 * @axi_clk:	AXI4-Lite bus clock
 * @misc_clks:	Misc ethernet clocks (AXI4-Stream, Ref, MGT clocks)
 * @mii_bus:	Pointer to MII bus structure
 * @mii_clk_div: MII bus clock divider value
 * @regs_start: Resource start for axienet device addresses
 * @regs:	Base address for the axienet_local device address space
 * @mcdma_regs:	Base address for the aximcdma device address space
 * @napi:	Napi Structure array for all dma queues
 * @num_tx_queues: Total number of Tx DMA queues
 * @num_rx_queues: Total number of Rx DMA queues
 * @dq:		DMA queues data
 * @phy_mode:	Phy type to identify between MII/GMII/RGMII/SGMII/1000 Base-X
 * @dma_err_tasklet: Tasklet structure to process Axi DMA errors
 * @eth_irq:	Axi Ethernet IRQ number
 * @options:	AxiEthernet option word
 * @features:	Stores the extended features supported by the axienet hw
 * @tx_bd_num: Number of TX buffer descriptors.
 * @rx_bd_num: Number of RX buffer descriptors.
 * @max_frm_size: Stores the maximum size of the frame that can be that
 *		  Txed/Rxed in the existing hardware. If jumbo option is
 *		  supported, the maximum frame size would be 9k. Else it is
 *		  1522 bytes (assuming support for basic VLAN)
 * @rxmem:	Stores rx memory size for jumbo frame handling.
 * @csum_offload_on_tx_path:	Stores the checksum selection on TX side.
 * @csum_offload_on_rx_path:	Stores the checksum selection on RX side.
 * @coalesce_count_rx:	Store the irq coalesce on RX side.
 * @coalesce_usec_rx:	IRQ coalesce delay for RX
 * @coalesce_count_tx:	Store the irq coalesce on TX side.
 * @coalesce_usec_tx:	IRQ coalesce delay for TX
 * @eth_hasnobuf: Ethernet is configured in Non buf mode.
 * @eth_hasptp: Ethernet is configured for ptp.
 * @axienet_config: Ethernet config structure
 * @tx_ts_regs:	  Base address for the axififo device address space.
 * @rx_ts_regs:	  Base address for the rx axififo device address space.
 * @tstamp_config: Hardware timestamp config structure.
 * @tx_ptpheader: Stores the tx ptp header.
 * @aclk: AXI4-Lite clock for ethernet and dma.
 * @eth_sclk: AXI4-Stream interface clock.
 * @eth_refclk: Stable clock used by signal delay primitives and transceivers.
 * @eth_dclk: Dynamic Reconfiguration Port(DRP) clock.
 * @dma_sg_clk: DMA Scatter Gather Clock.
 * @dma_rx_clk: DMA S2MM Primary Clock.
 * @dma_tx_clk: DMA MM2S Primary Clock.
 * @qnum:     Axi Ethernet queue number to be operate on.
 * @chan_num: MCDMA Channel number to be operate on.
 * @chan_id:  MCMDA Channel id used in conjunction with weight parameter.
 * @weight:   MCDMA Channel weight value to be configured for.
 * @dma_mask: Specify the width of the DMA address space.
 * @usxgmii_rate: USXGMII PHY speed.
 * @mrmac_rate: MRMAC speed.
 * @gt_pll: Common GT PLL mask control register space.
 * @gt_ctrl: GT speed and reset control register space.
 * @phc_index: Index to corresponding PTP clock used.
 * @gt_lane: MRMAC GT lane index used.
 * @ptp_os_cf: CF TS of PTP PDelay req for one step usage.
 * @xxv_ip_version: XXV IP version
 */
struct axienet_local {
	struct net_device *ndev;
	struct device *dev;

	struct phylink *phylink;
	struct phylink_config phylink_config;

	struct mdio_device *pcs_phy;
	struct phylink_pcs pcs;

	bool switch_x_sgmii;

	struct clk *axi_clk;
	struct clk_bulk_data misc_clks[XAE_NUM_MISC_CLOCKS];

	struct mii_bus *mii_bus;
	u8 mii_clk_div;

	resource_size_t regs_start;
	void __iomem *regs;
	void __iomem *mcdma_regs;

	struct tasklet_struct dma_err_tasklet[XAE_MAX_QUEUES];
	struct napi_struct napi[XAE_MAX_QUEUES];	/* NAPI Structure */

	u16    num_tx_queues;	/* Number of TX DMA queues */
	u16    num_rx_queues;	/* Number of RX DMA queues */
	struct axienet_dma_q *dq[XAE_MAX_QUEUES];	/* DMA queue data*/

	phy_interface_t phy_mode;
	spinlock_t ptp_tx_lock;		/* PTP tx lock*/
	int eth_irq;

	u32 options;
	u32 features;

	u16 tx_bd_num;
	u32 rx_bd_num;

	u32 max_frm_size;
	u32 rxmem;

	int csum_offload_on_tx_path;
	int csum_offload_on_rx_path;

	u32 coalesce_count_rx;
	u32 coalesce_usec_rx;
	u32 coalesce_count_tx;
	u32 coalesce_usec_tx;
	bool eth_hasnobuf;
	bool eth_hasptp;
	const struct axienet_config *axienet_config;

#ifdef CONFIG_XILINX_AXI_EMAC_HWTSTAMP
	void __iomem *tx_ts_regs;
	void __iomem *rx_ts_regs;
	struct hwtstamp_config tstamp_config;
	u8 *tx_ptpheader;
#endif
	struct clk *aclk;
	struct clk *eth_sclk;
	struct clk *eth_refclk;
	struct clk *eth_dclk;
	struct clk *dma_sg_clk;
	struct clk *dma_rx_clk;
	struct clk *dma_tx_clk;

	/* MCDMA Fields */
	int qnum[XAE_MAX_QUEUES];
	int chan_num[XAE_MAX_QUEUES];
	/* WRR Fields */
	u16 chan_id;
	u16 weight;

	u8 dma_mask;
	u32 usxgmii_rate;

	u32 mrmac_rate;		/* MRMAC speed */
	void __iomem *gt_pll;	/* Common GT PLL mask control register space */
	void __iomem *gt_ctrl;	/* GT speed and reset control register space */
	u32 phc_index;		/* Index to corresponding PTP clock used  */
	u32 gt_lane;		/* MRMAC GT lane index used */
	u64 ptp_os_cf;		/* CF TS of PTP PDelay req for one step usage */
	u32 xxv_ip_version;
};

/**
 * struct axienet_dma_q - axienet private per dma queue data
 * @lp:		Parent pointer
 * @dma_regs:	Base address for the axidma device address space
 * @tx_irq:	Axidma TX IRQ number
 * @rx_irq:	Axidma RX IRQ number
 * @tx_lock:	Spin lock for tx path
 * @rx_lock:	Spin lock for tx path
 * @tx_bd_v:	Virtual address of the TX buffer descriptor ring
 * @tx_bd_p:	Physical address(start address) of the TX buffer descr. ring
 * @rx_bd_v:	Virtual address of the RX buffer descriptor ring
 * @rx_bd_p:	Physical address(start address) of the RX buffer descr. ring
 * @tx_buf:	Virtual address of the Tx buffer pool used by the driver when
 *		DMA h/w is configured without DRE.
 * @tx_bufs:	Virutal address of the Tx buffer address.
 * @tx_bufs_dma: Physical address of the Tx buffer address used by the driver
 *		 when DMA h/w is configured without DRE.
 * @eth_hasdre: Tells whether DMA h/w is configured with dre or not.
 * @tx_bd_ci:	Stores the index of the Tx buffer descriptor in the ring being
 *		accessed currently. Used while alloc. BDs before a TX starts
 * @tx_bd_tail:	Stores the index of the Tx buffer descriptor in the ring being
 *		accessed currently. Used while processing BDs after the TX
 *		completed.
 * @rx_bd_ci:	Stores the index of the Rx buffer descriptor in the ring being
 *		accessed currently.
 * @chan_id:    MCDMA channel to operate on.
 * @rx_offset:	MCDMA S2MM channel starting offset.
 * @txq_bd_v:	Virtual address of the MCDMA TX buffer descriptor ring
 * @rxq_bd_v:	Virtual address of the MCDMA RX buffer descriptor ring
 * @tx_packets: Number of transmit packets processed by the dma queue.
 * @tx_bytes:   Number of transmit bytes processed by the dma queue.
 * @rx_packets: Number of receive packets processed by the dma queue.
 * @rx_bytes:	Number of receive bytes processed by the dma queue.
 */
struct axienet_dma_q {
	struct axienet_local	*lp; /* parent */
	void __iomem *dma_regs;

	int tx_irq;
	int rx_irq;

	spinlock_t tx_lock;		/* tx lock */
	spinlock_t rx_lock;		/* rx lock */

	/* Buffer descriptors */
	struct axidma_bd *tx_bd_v;
	struct axidma_bd *rx_bd_v;
	dma_addr_t rx_bd_p;
	dma_addr_t tx_bd_p;

	unsigned char *tx_buf[XAE_TX_BUFFERS];
	unsigned char *tx_bufs;
	dma_addr_t tx_bufs_dma;
	bool eth_hasdre;

	u32 tx_bd_ci;
	u32 rx_bd_ci;
	u32 tx_bd_tail;

	/* MCDMA fields */
	u16 chan_id;
	u32 rx_offset;
	struct aximcdma_bd *txq_bd_v;
	struct aximcdma_bd *rxq_bd_v;

	unsigned long tx_packets;
	unsigned long tx_bytes;
	unsigned long rx_packets;
	unsigned long rx_bytes;
};

#define AXIENET_ETHTOOLS_SSTATS_LEN 6
#define AXIENET_TX_SSTATS_LEN(lp) ((lp)->num_tx_queues * 2)
#define AXIENET_RX_SSTATS_LEN(lp) ((lp)->num_rx_queues * 2)

/**
 * enum axienet_ip_type - AXIENET IP/MAC type.
 *
 * @XAXIENET_1G:	 IP is 1G MAC
 * @XAXIENET_2_5G:	 IP type is 2.5G MAC.
 * @XAXIENET_LEGACY_10G: IP type is legacy 10G MAC.
 * @XAXIENET_10G_25G:	 IP type is 10G/25G MAC(XXV MAC).
 * @XAXIENET_MRMAC:	 IP type is hardened Multi Rate MAC (MRMAC).
 *
 */
enum axienet_ip_type {
	XAXIENET_1G = 0,
	XAXIENET_2_5G,
	XAXIENET_LEGACY_10G,
	XAXIENET_10G_25G,
	XAXIENET_MRMAC,
};

struct axienet_config {
	enum axienet_ip_type mactype;
	void (*setoptions)(struct net_device *ndev, u32 options);
	int (*clk_init)(struct platform_device *pdev, struct clk **axi_aclk,
			struct clk **axis_clk, struct clk **ref_clk,
			struct clk **dclk);
	u32 tx_ptplen;
	u8 ts_header_len;
};

/**
 * struct axiethernet_option - Used to set axi ethernet hardware options
 * @opt:	Option to be set.
 * @reg:	Register offset to be written for setting the option
 * @m_or:	Mask to be ORed for setting the option in the register
 */
struct axienet_option {
	u32 opt;
	u32 reg;
	u32 m_or;
};

struct xxvenet_option {
	u32 opt;
	u32 reg;
	u32 m_or;
};

extern void __iomem *mrmac_gt_pll;
extern void __iomem *mrmac_gt_ctrl;
extern int mrmac_pll_reg;
extern int mrmac_pll_rst;

/**
 * axienet_ior - Memory mapped Axi Ethernet register read
 * @lp:         Pointer to axienet local structure
 * @offset:     Address offset from the base address of Axi Ethernet core
 *
 * Return: The contents of the Axi Ethernet register
 *
 * This function returns the contents of the corresponding register.
 */
static inline u32 axienet_ior(struct axienet_local *lp, off_t offset)
{
	return ioread32(lp->regs + offset);
}

static inline u32 axinet_ior_read_mcr(struct axienet_local *lp)
{
	return axienet_ior(lp, XAE_MDIO_MCR_OFFSET);
}

static inline void axienet_lock_mii(struct axienet_local *lp)
{
	if (lp->mii_bus)
		mutex_lock(&lp->mii_bus->mdio_lock);
}

static inline void axienet_unlock_mii(struct axienet_local *lp)
{
	if (lp->mii_bus)
		mutex_unlock(&lp->mii_bus->mdio_lock);
}

/**
 * axienet_iow - Memory mapped Axi Ethernet register write
 * @lp:         Pointer to axienet local structure
 * @offset:     Address offset from the base address of Axi Ethernet core
 * @value:      Value to be written into the Axi Ethernet register
 *
 * This function writes the desired value into the corresponding Axi Ethernet
 * register.
 */
static inline void axienet_iow(struct axienet_local *lp, off_t offset,
			       u32 value)
{
	iowrite32(value, lp->regs + offset);
}

/**
 * axienet_get_mrmac_blocklock - Write to Clear MRMAC RX block lock status register
 * and read the latest status
 * @lp:         Pointer to axienet local structure
 *
 * Return: The contents of the Contents of MRMAC RX block lock status register
 */

static inline u32 axienet_get_mrmac_blocklock(struct axienet_local *lp)
{
	axienet_iow(lp, MRMAC_STATRX_BLKLCK_OFFSET, MRMAC_STS_ALL_MASK);
	return axienet_ior(lp, MRMAC_STATRX_BLKLCK_OFFSET);
}

/**
 * axienet_get_mrmac_rx_status - Write to Clear MRMAC RX status register
 * and read the latest status
 * @lp:		Pointer to axienet local structure
 *
 * Return: The contents of the Contents of MRMAC RX status register
 */

static inline u32 axienet_get_mrmac_rx_status(struct axienet_local *lp)
{
	axienet_iow(lp, MRMAC_RX_STS_OFFSET, MRMAC_STS_ALL_MASK);
	return axienet_ior(lp, MRMAC_RX_STS_OFFSET);
}

#ifdef CONFIG_XILINX_AXI_EMAC_HWTSTAMP
/**
 * axienet_txts_ior - Memory mapped AXI FIFO MM S register read
 * @lp:         Pointer to axienet_local structure
 * @reg:     Address offset from the base address of AXI FIFO MM S
 *              core
 *
 * Return: the contents of the AXI FIFO MM S register
 */

static inline u32 axienet_txts_ior(struct axienet_local *lp, off_t reg)
{
	return ioread32(lp->tx_ts_regs + reg);
}

/**
 * axienet_txts_iow - Memory mapper AXI FIFO MM S register write
 * @lp:         Pointer to axienet_local structure
 * @reg:     Address offset from the base address of AXI FIFO MM S
 *              core.
 * @value:      Value to be written into the AXI FIFO MM S register
 */
static inline void axienet_txts_iow(struct  axienet_local *lp, off_t reg,
				    u32 value)
{
	iowrite32(value, (lp->tx_ts_regs + reg));
}

/**
 * axienet_rxts_ior - Memory mapped AXI FIFO MM S register read
 * @lp:         Pointer to axienet_local structure
 * @reg:     Address offset from the base address of AXI FIFO MM S
 *              core
 *
 * Return: the contents of the AXI FIFO MM S register
 */

static inline u32 axienet_rxts_ior(struct axienet_local *lp, off_t reg)
{
	return ioread32(lp->rx_ts_regs + reg);
}

/**
 * axienet_rxts_iow - Memory mapper AXI FIFO MM S register write
 * @lp:         Pointer to axienet_local structure
 * @reg:     Address offset from the base address of AXI FIFO MM S
 *              core.
 * @value:      Value to be written into the AXI FIFO MM S register
 */
static inline void axienet_rxts_iow(struct  axienet_local *lp, off_t reg,
				    u32 value)
{
	iowrite32(value, (lp->rx_ts_regs + reg));
}
#endif

/**
 * axienet_dma_in32 - Memory mapped Axi DMA register read
 * @q:		Pointer to DMA queue structure
 * @reg:	Address offset from the base address of the Axi DMA core
 *
 * Return: The contents of the Axi DMA register
 *
 * This function returns the contents of the corresponding Axi DMA register.
 */
static inline u32 axienet_dma_in32(struct axienet_dma_q *q, off_t reg)
{
	return ioread32(q->dma_regs + reg);
}

/**
 * axienet_dma_out32 - Memory mapped Axi DMA register write.
 * @q:		Pointer to DMA queue structure
 * @reg:	Address offset from the base address of the Axi DMA core
 * @value:	Value to be written into the Axi DMA register
 *
 * This function writes the desired value into the corresponding Axi DMA
 * register.
 */
static inline void axienet_dma_out32(struct axienet_dma_q *q,
				     off_t reg, u32 value)
{
	iowrite32(value, q->dma_regs + reg);
}

/**
 * axienet_dma_bdout - Memory mapped Axi DMA register Buffer Descriptor write.
 * @q:		Pointer to DMA queue structure
 * @reg:	Address offset from the base address of the Axi DMA core
 * @value:	Value to be written into the Axi DMA register
 *
 * This function writes the desired value into the corresponding Axi DMA
 * register.
 */
static inline void axienet_dma_bdout(struct axienet_dma_q *q,
				     off_t reg, dma_addr_t value)
{
#if defined(CONFIG_PHYS_ADDR_T_64BIT)
	writeq(value, (q->dma_regs + reg));
#else
	writel(value, (q->dma_regs + reg));
#endif
}

/* Function prototypes visible in xilinx_axienet_mdio.c for other files */
int axienet_mdio_enable(struct axienet_local *lp);
void axienet_mdio_disable(struct axienet_local *lp);
int axienet_mdio_setup(struct axienet_local *lp);
void axienet_mdio_teardown(struct axienet_local *lp);
void __maybe_unused axienet_bd_free(struct net_device *ndev,
				    struct axienet_dma_q *q);
int __maybe_unused axienet_dma_q_init(struct net_device *ndev,
				      struct axienet_dma_q *q);
void axienet_dma_err_handler(unsigned long data);
irqreturn_t __maybe_unused axienet_tx_irq(int irq, void *_ndev);
irqreturn_t __maybe_unused axienet_rx_irq(int irq, void *_ndev);
void axienet_start_xmit_done(struct net_device *ndev, struct axienet_dma_q *q);
void axienet_dma_bd_release(struct net_device *ndev);
void __axienet_device_reset(struct axienet_dma_q *q);
void axienet_set_mac_address(struct net_device *ndev, const void *address);
void axienet_set_multicast_list(struct net_device *ndev);
int xaxienet_rx_poll(struct napi_struct *napi, int quota);

#if defined(CONFIG_AXIENET_HAS_MCDMA)
int __maybe_unused axienet_mcdma_rx_q_init(struct net_device *ndev,
					   struct axienet_dma_q *q);
int __maybe_unused axienet_mcdma_tx_q_init(struct net_device *ndev,
					   struct axienet_dma_q *q);
void __maybe_unused axienet_mcdma_tx_bd_free(struct net_device *ndev,
					     struct axienet_dma_q *q);
void __maybe_unused axienet_mcdma_rx_bd_free(struct net_device *ndev,
					     struct axienet_dma_q *q);
irqreturn_t __maybe_unused axienet_mcdma_tx_irq(int irq, void *_ndev);
irqreturn_t __maybe_unused axienet_mcdma_rx_irq(int irq, void *_ndev);
void __maybe_unused axienet_mcdma_err_handler(unsigned long data);
void axienet_strings(struct net_device *ndev, u32 sset, u8 *data);
int axienet_sset_count(struct net_device *ndev, int sset);
void axienet_get_stats(struct net_device *ndev,
		       struct ethtool_stats *stats,
		       u64 *data);
int axeinet_mcdma_create_sysfs(struct kobject *kobj);
void axeinet_mcdma_remove_sysfs(struct kobject *kobj);
int __maybe_unused axienet_mcdma_tx_probe(struct platform_device *pdev,
					  struct device_node *np,
					  struct axienet_local *lp);
int __maybe_unused axienet_mcdma_rx_probe(struct platform_device *pdev,
					  struct axienet_local *lp,
					  struct net_device *ndev);
#endif

#ifdef CONFIG_AXIENET_HAS_MCDMA
void axienet_tx_hwtstamp(struct axienet_local *lp,
			 struct aximcdma_bd *cur_p);
#else
void axienet_tx_hwtstamp(struct axienet_local *lp,
			 struct axidma_bd *cur_p);
#endif
u32 axienet_usec_to_timer(struct axienet_local *lp, u32 coalesce_usec);

#endif /* XILINX_AXI_ENET_H */
