/**
 * Copyright (c) 2015-2019 Shanghai Fullhan Microelectronics Co., Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 *
 * Change Logs:
 * Date           Author       Notes
 *
 */

/*****************************************************************************
 *  Include Section
 *  add all #include here
 *****************************************************************************/
#include <rtthread.h>
#include "fh_pmu.h"
#include <mmu.h>
#include "fh_def.h"
#include "board_info.h"
#include "fh_chip.h"
#include <calibrate.h>
#include <uart.h>
#include <timer.h>
#include <libc.h>
#include "pinctrl.h"
#ifdef RT_USING_SDIO
#include <mmc.h>
#endif

#include "platform_def.h"

#ifdef FH_USING_DMA_MEM
#include "dma_mem.h"
#endif
#include "fh_dma.h"

#ifdef RT_USING_SPISLAVE
#include "spi_slave.h"
#endif

#ifdef FH_USING_CLOCK
#include "fh_clock.h"
#endif

#ifdef RT_USING_FINSH
#include <finsh.h>
#endif

#ifdef RT_USING_USB
#include "fh_usbotg.h"
#endif

#ifdef FH_USING_SADC
#include "fh_sadc.h"
#endif

#ifdef RT_USING_SPI
#include "ssi.h"
#endif

#ifdef RT_USING_FAL_SFUD_ADAPT
#include "fh_fal_sfud_adapt.h"
#endif

#ifdef RT_USING_RTC
#include "fh_rtc.h"
#endif

#ifdef RT_USING_GPIO
#include "gpio.h"
#include "fh_gpio.h"
#endif

#ifdef RT_USING_PWM
#include "fh_pwm.h"
#endif

#ifdef RT_USING_WDT
#include "mol_wdt.h"
#endif

#ifdef RT_USING_DFS
#include "dfs.h"
#endif /* RT_USING_DFS */

#if defined(RT_USING_DFS_ELMFAT)
#include "dfs_elm.h"
#endif

#if defined(RT_USING_DFS_DEVFS)
#include "devfs.h"
#endif

#ifdef RT_USING_DFS_RAMFS
#include "dfs_ramfs.h"
#endif /* RT_USING_DFS_RAMFS */

#ifdef RT_USING_DFS_ROMFS
#include "dfs_romfs.h"
#endif

#ifdef RT_USING_LWIP
#include "lwip/sys.h"
#include "netif/ethernetif.h"
extern void lwip_sys_init(void);

#endif

#ifdef FH_USING_QOS_GMAC
#include "fh_qos_gmac.h"
#include "fh_qos_gmac_phyt.h"
#endif

#ifdef RT_USING_I2C
#include "i2c.h"
#include "fh_i2c.h"
#endif

#ifdef RT_USING_DFS_JFFS2
#include "dfs_jffs2.h"
#endif

#ifdef FH_USING_AES
#include "fh_aes.h"
#endif

#ifdef FH_USING_CESA
#include "fh_cesa.h"
#endif

#ifdef FH_USING_EFUSE
#include "fh_efuse.h"
#endif

#ifdef FH_USING_FH_PERF
#include "fh_perf_mon.h"
#endif

#ifdef FH_USING_NAND_FLASH
#include "fh_fal_sfud_adapt.h"
#endif

#ifdef FH_USING_FH_STEPMOTOR
#include "fh_stepmotor.h"
#endif

#ifdef FH_USING_MOL_I2S
#include "fh_i2s.h"
#endif

/* #ifdef RT_USING_DFS_JFFS2 */
/* #include "filesystems/jffs2/dfs_jffs2.h" */
/* #endif */


#ifndef HW_CIS_RST_GPIO
#define HW_CIS_RST_GPIO 13  /* cis(sensor) */
#endif
#ifndef HW_CIS_RST_GPIO_LEVEL
#define HW_CIS_RST_GPIO_LEVEL 1
#endif

#ifndef SPI_CRTOLLER0_SLAVE0_CS
#define SPI_CRTOLLER0_SLAVE0_CS (5)
#endif

#ifndef SPI_CRTOLLER0_SLAVE1_CS
#define SPI_CRTOLLER0_SLAVE1_CS (10)
#endif

#ifndef SPI_CRTOLLER1_SLAVE0_CS
#define SPI_CRTOLLER1_SLAVE0_CS (12)
#endif

#ifndef SPI0_TRANSFER_MODE
#define SPI0_TRANSFER_MODE USE_POLL_TRANSFER
#endif
#ifndef SPI1_TRANSFER_MODE
#define SPI1_TRANSFER_MODE USE_DMA_TRANSFER
#endif

/****************************************************************************
 * ADT section
 *  add definition of user defined Data Type that only be used in this file here
 ***************************************************************************/
struct st_platform_info
{
    char *name;
    void *private_data;
};

/******************************************************************************
 * Function prototype section
 * add prototypes for all functions called by this file,execepting those
 * declared in header file
 *****************************************************************************/

/*****************************************************************************
 * Global variables section - Exported
 * add declaration of global variables that will be exported here
 * e.g.
 *  int8_t foo;
 ****************************************************************************/

/*****************************************************************************
 * Global variables section - Local
 * define global variables(will be referred only in this file) here,
 * static keyword should be used to limit scope of local variable to this file
 * e.g.
 *  static uint8_t ufoo;
 *****************************************************************************/
#ifdef RT_USING_SDIO
void fh_mmc_reset(struct fh_mmc_obj *mmc_obj)
{
    char *clk_drv = "sdc0_clk_drv";
    char *clk_sample = "sdc0_clk_sample";
    struct clk *get;

    /*isp_sys_apb_reg*/
    if (mmc_obj->id == 0)
    {
        SET_REG_M(0x2c100058, 1 << 27, 1 << 27);
        udelay(10);
        SET_REG_M(0x2c100058, 0, 1 << 27);
    }
    else
    {
        SET_REG_M(0x2c100090, 1 << 27, 1 << 27);
        udelay(10);
        SET_REG_M(0x2c100090, 0, 1 << 27);
    }

    if (mmc_obj->id == 0)
    {
        get = clk_get(NULL, clk_drv);
        fh_clk_set_phase(get, 0x4);/* now drv fixed to 180 */
        get = clk_get(NULL, clk_sample);
        fh_clk_set_phase(get, 0x0);
    }
    else
    {
        clk_drv = "sdc1_clk_drv";
        clk_sample = "sdc1_clk_sample";
        get = clk_get(NULL, clk_drv);
        fh_clk_set_phase(get, 0x4);/* now drv fixed to 180 */
        get = clk_get(NULL, clk_sample);
        fh_clk_set_phase(get, 0x0);
    }

}

static struct fh_mmc_obj mmc0_obj = {
    .id             = 0,
    .irq            = SDC0_IRQn,
    .base           = SDC0_REG_BASE,
    /*
     * LCD 与 SD0 共用引脚，只能 1-bit。50MHz 写多块有 CRC 错误，
     * 25MHz 下长时间录像仍会偶发命令无响应（CMD24/CMD17 超时，
     * 卡停在数据态后所有命令都超时）。降到 12.5MHz 给信号/供电留
     * 余量：1-bit 12.5MHz 约 1.5MB/s，录像 5fps×~35KB≈160KB/s，
     * 抓拍/回放都够用；驱动里还有连续出错自动再减半到 6.25MHz 的兜底。
     */
    .sd_hz          = 12500000,
    .power_pin_gpio = -1,
    .mmc_reset      = fh_mmc_reset,
    .sd_bus_width   = MMCSD_BUSWIDTH_1,
};

static struct fh_mmc_obj mmc1_obj = {
    .id             = 1,
    .irq            = SDC1_IRQn,
    .base           = SDC1_REG_BASE,
    .sd_hz          = 50000000,
    .power_pin_gpio = -1,
    .mmc_reset      = fh_mmc_reset,
    .sd_bus_width   = MMCSD_BUSWIDTH_4,
};
#endif

#ifdef RT_USING_SPI
#define SPI0_CLK_IN (100000000)
#define SPI0_MAX_BAUD (SPI0_CLK_IN / 2)
#define SPI1_CLK_IN (100000000)
#define SPI1_MAX_BAUD (SPI1_CLK_IN / 2)

static struct spi_control_platform_data spi0_platform_data = {
    .id                         = 0,
    .irq                        = SPI0_IRQn,
    .base                       = SPI0_REG_BASE,
    .max_hz                     = SPI0_MAX_BAUD,
    .slave_no                   = 2,
    .clk_in                     = SPI0_CLK_IN,
    .rx_hs_no                   = SPI0_RX,
    .tx_hs_no                   = SPI0_TX,
    .dma_name                   = "fh_dma0",
    .clk_name                   = "spi0_clk",
    .transfer_mode              = SPI0_TRANSFER_MODE,
    .plat_slave[0].cs_pin       = SPI_CRTOLLER0_SLAVE0_CS,
    .plat_slave[0].actice_level = ACTIVE_LOW,
    .plat_slave[1].cs_pin       = SPI_CRTOLLER0_SLAVE1_CS,
    .plat_slave[1].actice_level = ACTIVE_LOW,
};

static struct spi_control_platform_data spi1_platform_data = {
    .id                         = 1,
    .irq                        = SPI1_IRQn,
    .base                       = SPI1_REG_BASE,
    .max_hz                     = SPI1_MAX_BAUD,
    .slave_no                   = 1,
    .clk_in                     = SPI1_CLK_IN,
    .rx_hs_no                   = SPI1_RX,
    .tx_hs_no                   = SPI1_TX,
    .dma_name                   = "fh_dma0",
    .clk_name                   = "spi1_clk",
    .transfer_mode              = SPI1_TRANSFER_MODE,
    .plat_slave[0].cs_pin       = SPI_CRTOLLER1_SLAVE0_CS,
    .plat_slave[0].actice_level = ACTIVE_LOW,
};

#endif

void reset_timer(void)
{
    struct clk *tmrclk = clk_get(NULL, "tmr0_clk");

    if (tmrclk != RT_NULL)
        clk_enable(tmrclk);

    fh_pmu_timer_reset();
}

#ifdef RT_USING_USB

/*usb vbus  power on*/
static void fh_usb_pwr_on(void)
{
    rt_uint32_t pwron_gpio = USB_VBUS_PWR_GPIO;

    fh_pmu_usb_pwr_on();
    fh_pmu_usb_tune();

    gpio_request(pwron_gpio);
    gpio_direction_output(pwron_gpio, 1);
    rt_thread_delay(1);
    gpio_release(pwron_gpio);

}

static struct fh_usbotg_obj usbotg_obj = {
    .id = 0, .irq = USB_IRQn, .base = USB_REG_BASE, .utmi_rst = fh_pmu_usb_utmi_rst,
    .phy_rst = fh_pmu_usb_phy_rst, .hcd_resume = fh_pmu_usb_resume, .power_on = fh_usb_pwr_on,
    .vbus_vldext = fh_pmu_usb_vbus_vldext,
};
#endif

#ifdef RT_USING_I2C
static struct fh_i2c_obj i2c0_obj = {
    .id = 0, .irq = I2C0_IRQn, .base = I2C0_REG_BASE,
};

static struct fh_i2c_obj i2c1_obj = {
    .id = 1, .irq = I2C1_IRQn, .base = I2C1_REG_BASE,
};
#endif

#ifdef RT_USING_GPIO
static struct fh_gpio_chip gpio0_obj = {
    .id = 0,
    .irq = GPIO0_IRQn,
    .base = GPIO0_REG_BASE,
    .trigger_type = HARDWARE,
    .chip = {
        .base = 0,
        .ngpio = 8,
    },
};

static struct fh_gpio_chip gpio1_obj = {
    .id = 1,
    .irq = GPIO1_IRQn,
    .base = GPIO1_REG_BASE,
    .trigger_type = HARDWARE,
    .chip = {
        .base = 8,
        .ngpio = 8,
    },
};

static struct fh_gpio_chip gpio2_obj = {
    .id = 2,
    .irq = GPIO2_IRQn,
    .base = GPIO2_REG_BASE,
    .trigger_type = HARDWARE,
    .chip = {
        .base = 16,
        .ngpio = 8,
    },
};

static struct fh_gpio_chip gpio3_obj = {
    .id = 3,
    .irq = GPIO3_IRQn,
    .base = GPIO3_REG_BASE,
    .trigger_type = HARDWARE,
    .chip = {
        .base = 24,
        .ngpio = 8,
    },
};

static struct fh_gpio_chip gpio4_obj = {
    .id = 4,
    .irq = GPIO4_IRQn,
    .base = GPIO4_REG_BASE,
    .trigger_type = HARDWARE,
    .chip = {
        .base = 32,
        .ngpio = 8,
    },
};

static struct fh_gpio_chip gpio5_obj = {
    .id = 5,
    .irq = GPIO5_IRQn,
    .base = GPIO5_REG_BASE,
    .trigger_type = HARDWARE,
    .chip = {
        .base = 40,
        .ngpio = 8,
    },
};

static struct fh_gpio_chip gpio6_obj = {
    .id = 6,
    .irq = GPIO6_IRQn,
    .base = GPIO6_REG_BASE,
    .trigger_type = HARDWARE,
    .chip = {
        .base = 48,
        .ngpio = 8,
    },
};
#endif

#ifdef RT_USING_PWM
static struct fh_pwm_obj pwm_obj = {
    .id = 0, .base = PWM_REG_BASE, .irq = PWM_IRQn, .npwm = 4,
};
#endif

#ifdef FH_USING_FH_STEPMOTOR
static struct fh_smt_obj smt_obj0 = {
    .id = 0, .base = SMT0_REG_BASE, .irq = SMT0_IRQn,
};
static struct fh_smt_obj smt_obj1 = {
    .id = 1, .base = SMT1_REG_BASE, .irq = SMT1_IRQn,
};
#endif


#ifdef RT_USING_WDT
static struct mol_wdt_obj wdt_obj = {
    .id = 0, .base = WDT0_REG_BASE, .irq = WDT_IRQn, .timeout = 10,
    .rst = 0,
};
#endif

#ifdef FH_USING_SADC
static struct wrap_sadc_obj sadc_obj = {
    .id          = 0,
    .regs        = (void *)SADC_REG_BASE,
    .irq_no      = SADC_IRQn,
    .frequency   = 5000000,
    .ref_vol     = 1800,
    .max_value   = 0xfff,
    .max_chan_no = 8,
};
#endif

#ifdef RT_USING_SDIO
struct st_platform_info plat_mmc0 = {
    .name = "mmc", .private_data = &mmc0_obj,
};

struct st_platform_info plat_mmc1 = {
    .name = "mmc", .private_data = &mmc1_obj,
};
#endif

#ifdef RT_USING_SPI
struct st_platform_info plat_spi0 = {
    .name = "spi", .private_data = &spi0_platform_data,
};

struct st_platform_info plat_spi1 = {
    .name = "spi", .private_data = &spi1_platform_data,
};
#endif

#ifdef FH_USING_FH_PERF
struct st_platform_info plat_fh_perf = {
    .name = "fh_perf", .private_data = &fh_perf_platform_data,
};
#endif


#ifdef RT_USING_FAL_SFUD_ADAPT
#if 0
static struct fh_fal_partition_adapt_info fh_sf_parts[] = {
    {
        /* bootstrap, this area is protected */
        .name       = "mtd0",
        .offset     = 0,
        .size       = 0x40000,
        .cache_enable   = 0,
    },
    {
        /* U-Boot environment */
        .name       = "mtd1",
        .offset     = 0x40000,
        .size       = 0x10000,
        .cache_enable   = 0,
    },
    {
        /* U-Boot */
        .name       = "mtd2",
        .offset     = 0x50000,
        .size       = 0x40000,
        .cache_enable   = 0,
    },
    {
       .name           = "mtd3",
       .offset         = 0x90000,
       .size           = 0x70000,
       .cache_enable   = 0,
    },
    {
        .name           = "mtd4",
        .offset         = APP_FLASH_PART_OFFSET,
        .size           = 0x400000,
        .cache_enable   = 0,
    },
    {
        .name           = "mtd5",
        .offset         = 0x500000,
        .size           = 0x200000,
        .cache_enable   = 0,
    },
};

struct fh_fal_sfud_adapt_plat_info fh_fal_sfud_platform_data = {
    .flash_name = "fh_flash",
    .spi_name   = "ssi0_0",
    .parts      = fh_sf_parts,
    .nr_parts   = ARRAY_SIZE(fh_sf_parts),
};
#else
struct fh_fal_sfud_adapt_plat_info fh_fal_sfud_platform_data = {
    .flash_name = "fh_flash",
    .spi_name   = "ssi0_0",
    .parts      = RT_NULL,
    .nr_parts   = 0,
};
#endif
struct st_platform_info plat_flash = {
    .name = "fh_fal_sfud_adapt", .private_data = (void *)&fh_fal_sfud_platform_data,
};
#endif

#ifdef FH_USING_NAND_FLASH
struct fh_fal_sfud_adapt_plat_info fh_fal_sfud_platform_nand_data = {
    .flash_name = "fh_nand_flash",
    .spi_name   = "ssi0_0",
    .parts      = RT_NULL,
    .nr_parts   = 0,
};

struct st_platform_info plat_nand_flash = {
    .name = "fh_nand_flash", .private_data = (void *)& fh_fal_sfud_platform_nand_data,
};
#endif

#ifdef RT_USING_USB
struct st_platform_info plat_usbotg = {
    .name = "fh_otg", .private_data = &usbotg_obj,
};
#endif

#ifdef RT_USING_I2C
struct st_platform_info plat_i2c0 = {
    .name = "i2c", .private_data = &i2c0_obj,
};

struct st_platform_info plat_i2c1 = {
    .name = "i2c", .private_data = &i2c1_obj,
};
#endif

#ifdef RT_USING_GPIO
struct st_platform_info plat_gpio0 = {
    .name = "gpio", .private_data = &gpio0_obj,
};

struct st_platform_info plat_gpio1 = {
    .name = "gpio", .private_data = &gpio1_obj,
};

struct st_platform_info plat_gpio2 = {
    .name = "gpio", .private_data = &gpio2_obj,
};

struct st_platform_info plat_gpio3 = {
    .name = "gpio", .private_data = &gpio3_obj,
};

struct st_platform_info plat_gpio4 = {
    .name = "gpio", .private_data = &gpio4_obj,
};

struct st_platform_info plat_gpio5 = {
    .name = "gpio", .private_data = &gpio5_obj,
};

struct st_platform_info plat_gpio6 = {
    .name = "gpio", .private_data = &gpio6_obj,
};
#endif

#ifdef RT_USING_PWM
struct st_platform_info plat_pwm = {
    .name = "pwm", .private_data = &pwm_obj,
};
#endif

#ifdef FH_USING_FH_STEPMOTOR
struct st_platform_info plat_smt0 = {
    .name = FH_SM_PLAT_DEVICE_NAME, .private_data = &smt_obj0,
};
struct st_platform_info plat_smt1 = {
    .name = FH_SM_PLAT_DEVICE_NAME, .private_data = &smt_obj1,
};
#endif


#ifdef RT_USING_WDT
struct st_platform_info plat_wdt = {
    .name = "wdt", .private_data = &wdt_obj,
};
#endif

#ifdef FH_USING_SADC
struct st_platform_info plat_sadc = {
    .name = "sadc", .private_data = &sadc_obj,
};
#endif


#ifdef FH_USING_AHB_DMA
static struct dma_platform_data dma0_platform_data = {
    .id                         = 0,
    .name                       = "fh_dma",
    .irq                        = DMAC0_IRQn,
    .base                       = DMAC0_REG_BASE,
    .channel_max_number         = 6,
    .dma_init                   = RT_NULL,
};

struct st_platform_info plat_ahb_dma0 = {
    .name = "fh_dma", .private_data = &dma0_platform_data,
};
#endif



#ifdef FH_USING_AXI_DMA
static struct dma_platform_data dma0_platform_data = {
    .id                         = 0,
    .name                       = "fh_dma",
    .irq                        = DMAC0_IRQn,
    .base                       = DMAC0_REG_BASE,
    .channel_max_number         = 6,
    .dma_init                   = RT_NULL,
};

struct st_platform_info plat_axi_dma0 = {
    .name = "fh_dma", .private_data = &dma0_platform_data,
};
#endif

#ifdef FH_USING_MOL_DMA
static struct dma_platform_data dma0_platform_data = {
    .id                         = 0,
    .name                       = "fh_dma",
    .irq                        = DMA_IRQn,
    .base                       = DMAC0_REG_BASE,
    .channel_max_number         = 24,
    .dma_init                   = RT_NULL,
};

struct st_platform_info plat_mol_dma0 = {
    .name = "fh_dma", .private_data = &dma0_platform_data,
};
#endif


#ifdef FH_USING_AES
struct fh_aes_platform_data fh_aes_plat_obj = {
    .id = 0,
    .irq = AES_IRQn,
    .base = AES_REG_BASE,
    .efuse_base = EFUSE_REG_BASE,
    .aes_support_flag = CRYPTO_CPU_SET_KEY | CRYPTO_EX_MEM_SET_KEY
    | CRYPTO_EX_MEM_SWITCH_KEY | CRYPTO_EX_MEM_4_ENTRY_1_KEY | CRYPTO_EX_MEM_INDEP_POWER,
};

struct st_platform_info plat_aes = {

    .name = "fh_aes",
    .private_data = &fh_aes_plat_obj,
};
#endif

#ifdef FH_USING_CESA
struct fh_cesa_platform_data fh_cesa_plat_obj = {
    .id = 0,
    .irq = AES_IRQn,
    .base = AES_REG_BASE,
    .ext_dma_enable = CESA_USE_EXT_DMA,
    .ext_dma_rx_handshake = AES_RX,
    .ext_dma_tx_handshake = AES_TX,
    .reset_base = CPU_SYS_AHB_REG_BASE,
    .reset_offset = 0x1b4,
    .reset_bit = 0x14,
    .ext_dma_name = "fh_dma0",
};

struct st_platform_info plat_cesa = {
    .name = "fh_cesa",
    .private_data = &fh_cesa_plat_obj,
};
#endif

#ifdef FH_USING_EFUSE
struct fh_efuse_platform_data fh_efuse_plat_obj = {
    .id = 0,
    .base = EFUSE_REG_BASE,
};

struct st_platform_info plat_efuse = {
    .name = "fh_efuse",
    .private_data = &fh_efuse_plat_obj,
};
#endif

#ifdef RT_USING_RTC


unsigned int TSENSOR_LUT[12] = {0x2024272B,
                                0x14161A1D,
                                0x0A0C0F11,
                                0x04050708,
                                0x00010203,
                                0x00000000,
                                0x02010100,
                                0x08060503,
                                0x100E0C09,
                                0x1C181613,
                                0x2A26221F,
                                0x3C37322E};
struct fh_rtc_adjust_parm rtc_adjust_parm = {

        .lut_coef = 62,
        .lut_offset = 0xfc,
        .tsensor_cp_default_out = 0xa44,
        .tsensor_lut = (int *)&TSENSOR_LUT,

};

struct fh_rtc_obj rtc_obj = {

        .id = 0,
        .irq = RTC_IRQn,
        .base = RTC_REG_BASE,
        .adjust_parm = &rtc_adjust_parm,
};

struct st_platform_info plat_rtc = {

    .name = "rtc",
    .private_data = &rtc_obj,
};
#endif

#ifdef FH_USING_QOS_GMAC
struct phy_reg_cfg phy_config_array[] = {
    {
        .id = FH_GMAC_PHY_RTL8201,
        .list = {
            {0},
        },
        .inf_sup = PHY_INTERFACE_MODE_MII,
    },
    {
        .id = FH_GMAC_PHY_IP101G,
        .list = {
            {W_PHY, 0x14, 0x10},
            {W_PHY, 0x10, 0x1006},
            {W_PHY, 0x14, 0x10},
            {0},
        },
        .inf_sup = PHY_INTERFACE_MODE_RMII,
    },
    {
        .id = FH_GMAC_PHY_TI83848,
        .list = {
            {M_PHY, 0x11, 0x20, 0x20},
            {0},
        },
        .inf_sup = PHY_INTERFACE_MODE_RMII,
    },
    {
        .id = FH_GMAC_PHY_DUMMY,
        .list = {
            {0},
        },
        .inf_sup = PHY_INTERFACE_MODE_RMII,
    },
    {
        .id = FH_GMAC_PHY_INTERNAL_V2,
        .list = {
            {0},
        },
        .inf_sup = PHY_INTERFACE_MODE_MII,
    },
    {
        .id = FH_GMAC_PHY_RTL8211F,
        .list = {
            {0},
        },
        .inf_sup = PHY_INTERFACE_MODE_RGMII,
    },
    {
        .id = FH_GMAC_PHY_JL2101,
        .list = {
            {0},
        },
        .inf_sup = PHY_INTERFACE_MODE_RGMII,
    },
    {
        .id = FH_GMAC_PHY_MAE0621,
        .list = {
            {W_PHY, 0x1f, 0x0d92},
            {W_PHY, 0x1f, 0x0d92},
            {W_PHY, 0x02, 0x200a},
            {W_PHY, 0x1f, 0x0},

            {W_PHY, 0x1f, 0x0},
            {W_PHY, 0x0d, 0x7},
            {W_PHY, 0x0e, 0x3c},
            {W_PHY, 0x0d, 0x4007},
            {W_PHY, 0x0e, 0x0},
            {W_PHY, 0x0, 0x9140},
            {W_PHY, 0x1f, 0x0},

            {0},
        },
        .inf_sup = PHY_INTERFACE_MODE_RGMII,
    },
    {0},
};

void get_init_mac_addr(unsigned char *macbuf)
{
    unsigned int rtc_seed = 0x20190611;

    macbuf[0] = 0x86;
    macbuf[1] = 0x30;
    macbuf[2] =  (rtc_seed >> 24) & 0xff;
    macbuf[3] =  (rtc_seed >> 16) & 0xff;
    macbuf[4] =  (rtc_seed >> 8) & 0xff;
    macbuf[5] =  0xff & rtc_seed;
}

/* add gmac plat */
void fh_pmu_ephy_set_reg(unsigned int offset, unsigned int data)
{
    SET_REG(EPHY_REG_BASE + offset, data);
}

void fh_pmu_ephy_set_m_reg(unsigned int offset,
unsigned int data, unsigned int mask)
{
    unsigned int ret;

    ret = GET_REG(EPHY_REG_BASE + offset);
    ret &= ~(mask);
    ret |= (data & mask);
    SET_REG(EPHY_REG_BASE + offset, ret);
}

enum
{
    CP_VAL_0,
    CP_VAL_1,
    VAL_100M_0,
    VAL_100M_1,
    VAL_10M_0,
    VAL_10M_1,
    TRAIN_MAX_SIZE,
};

#define BITS_PER_LONG 32
#define BITMAP_LAST_WORD_MASK(nbits) (~0UL >> (-(nbits) & (BITS_PER_LONG - 1)))
/**
 * __ffs - find first bit in word.
 * @word: The word to search
 *
 * Undefined if no bit exists, so code should check against 0 first.
 */
static  unsigned long __ffs(unsigned long word)
{
    int num = 0;

#if BITS_PER_LONG == 64
    if ((word & 0xffffffff) == 0)
    {
        num += 32;
        word >>= 32;
    }
#endif
    if ((word & 0xffff) == 0)
    {
        num += 16;
        word >>= 16;
    }
    if ((word & 0xff) == 0)
    {
        num += 8;
        word >>= 8;
    }
    if ((word & 0xf) == 0)
    {
        num += 4;
        word >>= 4;
    }
    if ((word & 0x3) == 0)
    {
        num += 2;
        word >>= 2;
    }
    if ((word & 0x1) == 0)
        num += 1;
    return num;
}

/**
 * __fls - find last (most-significant) set bit in a long word
 * @word: the word to search
 *
 * Undefined if no set bit exists, so code should check against 0 first.
 */
static  unsigned long __fls(unsigned long word)
{
    int num = BITS_PER_LONG - 1;

#if BITS_PER_LONG == 64
    if (!(word & (~0ul << 32)))
    {
        num -= 32;
        word <<= 32;
    }
#endif
    if (!(word & (~0ul << (BITS_PER_LONG-16))))
    {
        num -= 16;
        word <<= 16;
    }
    if (!(word & (~0ul << (BITS_PER_LONG-8))))
    {
        num -= 8;
        word <<= 8;
    }
    if (!(word & (~0ul << (BITS_PER_LONG-4))))
    {
        num -= 4;
        word <<= 4;
    }
    if (!(word & (~0ul << (BITS_PER_LONG-2))))
    {
        num -= 2;
        word <<= 2;
    }
    if (!(word & (~0ul << (BITS_PER_LONG-1))))
        num -= 1;
    return num;
}
/*
 * Find the first set bit in a memory region.
 */
static inline unsigned long find_first_bit(const unsigned long *addr, unsigned long size)
{
    unsigned long idx;

    for (idx = 0; idx * BITS_PER_LONG < size; idx++)
    {
        if (addr[idx])
            return MIN(idx * BITS_PER_LONG + __ffs(addr[idx]), size);
    }

    return size;
}


unsigned long find_last_bit(const unsigned long *addr, unsigned long size)
{
    if (size)
    {
        unsigned long val = BITMAP_LAST_WORD_MASK(size);
        unsigned long idx = (size-1) / BITS_PER_LONG;

        do
        {
            val &= addr[idx];
            if (val)
                return idx * BITS_PER_LONG + __fls(val);

            val = ~0ul;
        } while (idx--);
    }
    return size;
}

static unsigned int train_100_array[] = {
    31, 30, 29, 28,
    27, 26, 25, 24,
    23, 22, 21, 20,
    19, 18, 17, 16,
    0,  1,  2,  3,
    4,  5,  6,  7,
    8,  9,  10, 11,
    12, 13, 14, 15,
};

static unsigned int train_10_array[] = {
    31, 30, 29, 28,
    27, 26, 25, 24,
    23, 22, 21, 20,
    19, 18, 17, 16,
    0,  1,  2,  3,
    4,  5,  6,  7,
    8,  9,  10, 11,
    12, 13, 14, 15,
};

static unsigned int train_cp_array[] = {
    0, 1, 2, 3,
};
#define REFIX_CP_TRAIN_DATA_OFFSET    (0)
#define REFIX_100M_TRAIN_DATA_OFFSET    (0)
#define REFIX_10M_TRAIN_DATA_OFFSET    (0)

struct s_train_val train_array[TRAIN_MAX_SIZE] = {

    [CP_VAL_0].name = "cp_val_0",
    [CP_VAL_0].src_add = REG_PMU_EPHY_PARAM,
    [CP_VAL_0].src_mask = 0xc0,
    [CP_VAL_0].src_valid_index = 15,
    [CP_VAL_0].dst_add = 0x18,
    [CP_VAL_0].dst_valid_index = 22,
    [CP_VAL_0].bind_train_array = train_cp_array,
    [CP_VAL_0].bind_train_size = ARRAY_SIZE(train_cp_array),
    [CP_VAL_0].usr_train_offset =
    REFIX_CP_TRAIN_DATA_OFFSET,

    [CP_VAL_1].name = "cp_val_1",
    [CP_VAL_1].src_add = REG_PMU_EPHY_PARAM,
    [CP_VAL_1].src_mask = 0xc0,
    [CP_VAL_1].src_valid_index = 15,
    [CP_VAL_1].dst_add = 0xb7c,
    [CP_VAL_1].dst_valid_index = 6,
    [CP_VAL_1].bind_train_array = train_cp_array,
    [CP_VAL_1].bind_train_size = ARRAY_SIZE(train_cp_array),
    [CP_VAL_1].usr_train_offset =
    REFIX_CP_TRAIN_DATA_OFFSET,

    [VAL_100M_0].name = "val_100M_0",
    [VAL_100M_0].src_add = REG_PMU_EPHY_PARAM,
    [VAL_100M_0].src_mask = 0x1f,
    [VAL_100M_0].src_valid_index = 5,
    [VAL_100M_0].dst_add = 0x24,
    [VAL_100M_0].dst_valid_index = 8,
    [VAL_100M_0].bind_train_array = train_100_array,
    [VAL_100M_0].bind_train_size = ARRAY_SIZE(train_100_array),
    [VAL_100M_0].usr_train_offset =
    REFIX_100M_TRAIN_DATA_OFFSET,

    [VAL_100M_1].name = "val_100M_1",
    [VAL_100M_1].src_add = REG_PMU_EPHY_PARAM,
    [VAL_100M_1].src_mask = 0x1f,
    [VAL_100M_1].src_valid_index = 5,
    [VAL_100M_1].dst_add = 0x1308,
    [VAL_100M_1].dst_valid_index = 8,
    [VAL_100M_1].bind_train_array = train_100_array,
    [VAL_100M_1].bind_train_size = ARRAY_SIZE(train_100_array),
    [VAL_100M_1].usr_train_offset =
    REFIX_100M_TRAIN_DATA_OFFSET,

    [VAL_10M_0].name = "val_10M_0",
    [VAL_10M_0].src_add = REG_PMU_EPHY_PARAM,
    [VAL_10M_0].src_mask = 0x1f00,
    [VAL_10M_0].src_valid_index = 13,
    [VAL_10M_0].dst_add = 0x24,
    [VAL_10M_0].dst_valid_index = 16,
    [VAL_10M_0].bind_train_array = train_10_array,
    [VAL_10M_0].bind_train_size = ARRAY_SIZE(train_10_array),
    [VAL_10M_0].usr_train_offset =
    REFIX_10M_TRAIN_DATA_OFFSET,

    [VAL_10M_1].name = "val_10M_1",
    [VAL_10M_1].src_add = REG_PMU_EPHY_PARAM,
    [VAL_10M_1].src_mask = 0x1f00,
    [VAL_10M_1].src_valid_index = 13,
    [VAL_10M_1].dst_add = 0x1308,
    [VAL_10M_1].dst_valid_index = 0,
    [VAL_10M_1].bind_train_array = train_10_array,
    [VAL_10M_1].bind_train_size = ARRAY_SIZE(train_10_array),
    [VAL_10M_1].usr_train_offset =
    REFIX_10M_TRAIN_DATA_OFFSET,
};

void dump_phy_train_val(struct s_train_val *p_train)
{
    rt_kprintf("------------GAP_LINE---------------\n");
    rt_kprintf("name            : %s\n", p_train->name);
    rt_kprintf("src_add         : %08x\n", p_train->src_add);
    rt_kprintf("src_mask        : %08x\n", p_train->src_mask);
    rt_kprintf("src_valid_index : %08x\n", p_train->src_valid_index);
    rt_kprintf("dst_add         : %08x\n", p_train->dst_add);
    rt_kprintf("dst_valid_index : %08x\n", p_train->dst_valid_index);
    rt_kprintf("usr_train_offset: %d\n", p_train->usr_train_offset);
}

void parse_train_data(struct s_train_val *p_train)
{
    unsigned long test_data;
    int i;
    unsigned int temp_data;
    unsigned int valid_bit_pos;
    int max_offset = 0;
    int low_offset = 0;
    unsigned int *array;
    unsigned int size;

    array = p_train->bind_train_array;
    size = p_train->bind_train_size;
    test_data = p_train->src_mask;
    valid_bit_pos = find_first_bit(&test_data, 32);
    if ((!array) ||
        ((p_train->src_mask >> valid_bit_pos) != (size - 1)))
    {
        rt_kprintf("[ERROR]: para error..\n");
        return;
    }

    temp_data = fh_pmu_get_reg(p_train->src_add);
    if (temp_data & (1 << p_train->src_valid_index))
    {
        /*got valid val*/
        temp_data &= p_train->src_mask;
        temp_data >>= valid_bit_pos;
        /*find index*/
        for (i = 0; i < size; i++)
        {
            if (temp_data == array[i])
            {
                max_offset = (size - 1) - i;
                low_offset = 0 - i;
                break;
            }
        }

        if ((p_train->usr_train_offset < low_offset) ||
        (p_train->usr_train_offset > max_offset)) {
            rt_kprintf("[WARNING]: offset [%d] should limit in [%d : %d]\n",
            p_train->usr_train_offset, low_offset, max_offset);
            return;
        }
        i += p_train->usr_train_offset;
        temp_data = array[i];
        /*rt_kprintf("fix idx :: [%d] = %d\n", i, temp_data);*/
    }
    else
    {
        temp_data = 0;
        /*rt_kprintf("[%s] :: use default train data %d\n",*/
        /*p_train->name, temp_data);*/
    }
    temp_data &= (p_train->src_mask >> valid_bit_pos);
    fh_pmu_ephy_set_m_reg(p_train->dst_add,
    temp_data << p_train->dst_valid_index,
    (p_train->src_mask >> valid_bit_pos) << p_train->dst_valid_index);
}

static void fh_internal_phy_reset(void)
{
    int idx;

    fh_pmu_set_reg(REG_EPHY_CTRL0, 0xc);

    fh_pmu_ephy_set_m_reg(0, 0, 0x3<<7);
    fh_pmu_ephy_set_reg(0x28, 0x00247875);
    fh_pmu_ephy_set_reg(0x1274, 0x3c3);

    fh_pmu_set_reg_m(REG_GMAC_CTRL0, 1<<4, 1<<4); /* ephy soft reset */
    fh_pmu_set_reg_m(REG_EPHY_CTRL0, 1<<4, 1<<4); /* apb ephy soft reset */
    udelay(200);
    fh_pmu_set_reg_m(REG_EPHY_CTRL0, 0, 1<<4);
    fh_pmu_set_reg_m(REG_GMAC_CTRL0, 0, 1<<4);

    fh_pmu_ephy_set_m_reg(0x1370, 0x0, 1<<3);
    fh_pmu_ephy_set_reg(0x1274, 0x3c3);
    fh_pmu_ephy_set_reg(0x0b6C, 0xf04);
    fh_pmu_ephy_set_reg(0x1340, 0x0000);
    fh_pmu_ephy_set_reg(0x0bA8, 0x20);
    fh_pmu_ephy_set_reg(0x0018, 0x00c02800);
    fh_pmu_ephy_set_reg(0x001c, 0x00810000);
    fh_pmu_ephy_set_reg(0x0020, 0x0000340c);
    fh_pmu_ephy_set_reg(0x0024, 0x30131500);
    fh_pmu_ephy_set_reg(0x0028, 0x01402800);
    fh_pmu_ephy_set_reg(0x001c, 0xa1800000);
    fh_pmu_ephy_set_reg(0x0028, 0x00402800);
    fh_pmu_ephy_set_reg(0x0014, 0x06000000);
    fh_pmu_ephy_set_reg(0x0020, 0x2340c);
    fh_pmu_ephy_set_reg(0x000c, 0xba0edfff);
    fh_pmu_ephy_set_reg(0x0010, 0x7fcbf9de);
    fh_pmu_ephy_set_reg(0x0b98, 0xd150);
    fh_pmu_ephy_set_reg(0x1224, 0);
    fh_pmu_ephy_set_reg(0x12f0, 0);
    fh_pmu_ephy_set_reg(0x0bc0, 0x7de);
    fh_pmu_ephy_set_reg(0x0bc8, 0x09de);
    fh_pmu_ephy_set_reg(0x070c, 0x0401);
    fh_pmu_ephy_set_reg(0x0a84, 0x1d20);
    fh_pmu_ephy_set_reg(0x06d4, 0x3130);
    fh_pmu_ephy_set_reg(0x0a6c, 0x0818);
    fh_pmu_ephy_set_reg(0x0aec, 0x1000);
    fh_pmu_ephy_set_reg(0x0658, 0x1c00);
    fh_pmu_ephy_set_reg(0x07d4, 0x6900);
    fh_pmu_ephy_set_reg(0x06b8, 0x4);
    fh_pmu_ephy_set_reg(0x06E4, 0x04a0);
    fh_pmu_ephy_set_reg(0x0BBC, 0x1759);
    fh_pmu_ephy_set_reg(0x0bf8, 0x0100);
    fh_pmu_ephy_set_reg(0x0794, 0x9000);
    fh_pmu_ephy_set_reg(0x0788, 0xf);
    fh_pmu_ephy_set_reg(0x1378, 0x2338);
    fh_pmu_ephy_set_m_reg(0x0, 0x20<<24, 0xff<<24);
    fh_pmu_ephy_set_m_reg(0x4, 0x18, 0xff);
    fh_pmu_ephy_set_reg(0x0680, 0x0552);
    fh_pmu_ephy_set_reg(0x1274, 0);
    fh_pmu_ephy_set_reg(0x0b6C, 0x704);
    fh_pmu_ephy_set_reg(0x1340, 0);
    fh_pmu_ephy_set_reg(0x0ba8, 0);
    fh_pmu_ephy_set_reg(0x06c8, 0xc000);
    fh_pmu_ephy_set_reg(0x06e0, 0x8508);
    fh_pmu_ephy_set_reg(0x1328, 0x0065);
    fh_pmu_ephy_set_reg(0x076c, 0x0430);
    fh_pmu_ephy_set_reg(0x06b4, 0xfd00);
    fh_pmu_ephy_set_reg(0x1360, 0x3f37);
    fh_pmu_ephy_set_m_reg(0x0778, 0x24 << 8, 0xff << 8);
    fh_pmu_ephy_set_m_reg(0x077c, 0x24, 0xff);
    fh_pmu_ephy_set_reg(0xb10, 0x5000);
    fh_pmu_ephy_set_reg(0xb14, 0x8000);
    fh_pmu_ephy_set_reg(0xb1c, 0x000e);
    fh_pmu_ephy_set_reg(0xb20, 0x000e);
    fh_pmu_ephy_set_reg(0xb1c, 0x000f);
    fh_pmu_ephy_set_reg(0xb20, 0x0035);
    fh_pmu_ephy_set_reg(0xb1c, 0x0000);
    fh_pmu_ephy_set_reg(0xb10, 0x0000);
    fh_pmu_ephy_set_reg(0xb14, 0x0000);
    /*led*/
    fh_pmu_ephy_set_reg(0x7ec, 0x8a6);
    fh_pmu_ephy_set_reg(0x2c, 0x10);
    fh_pmu_ephy_set_reg(0x07f8, 0x325);

    for (idx = 0; idx < ARRAY_SIZE(train_array); idx++)
    {
        /*dump_phy_train_val(&train_array[idx]);*/
        parse_train_data(&train_array[idx]);
    }
}

static void fh_internal_phy_link_up_cb(void *pri)
{
    fh_pmu_ephy_set_reg(0x0b10, 0x5000);
    fh_pmu_ephy_set_reg(0x0b14, 0x8000);
    fh_pmu_ephy_set_reg(0x0b1c, 0x000e);
    fh_pmu_ephy_set_reg(0x0b20, 0x0004);
    fh_pmu_ephy_set_reg(0x0aec, 0x1800);
    fh_pmu_ephy_set_reg(0x0b10, 0x0000);
    fh_pmu_ephy_set_reg(0x0b14, 0x0000);
}

static void fh_internal_phy_link_down_cb(void *pri)
{
    fh_pmu_ephy_set_reg(0xb10, 0x5000);
    fh_pmu_ephy_set_reg(0xb14, 0x8000);
    fh_pmu_ephy_set_reg(0xb1c, 0x000e);
    fh_pmu_ephy_set_reg(0xb20, 0x000e);
    fh_pmu_ephy_set_reg(0xb1c, 0x000f);
    fh_pmu_ephy_set_reg(0xb20, 0x0035);
    fh_pmu_ephy_set_reg(0xb1c, 0x0000);
    fh_pmu_ephy_set_reg(0xb10, 0x0000);
    fh_pmu_ephy_set_reg(0xb14, 0x0000);
}

static int phy_sel_func(unsigned int phy_sel)
{
    return 0;
}

static int phy_inf_set(unsigned int inf)
{
    return 0;
}

static struct fh_gmac_platform_data fh_gmac_plat_info = {
    .id                         = 0,
    .irq                        = GMAC0_SBD_INTR_IRQn,
    .base_add                   = GMAC_REG_BASE,
    .phy_sel = phy_sel_func,
    .inf_set = phy_inf_set,
    .gmac_reset = 0,
    .p_cfg_array = phy_config_array,
    .phy_info[0] = {
        .phy_sel = INTERNAL_PHY,
        .phy_inf = PHY_INTERFACE_MODE_MII,
        .phy_reset = fh_internal_phy_reset,
        .ex_sync_mac_spd = 0,
        .brd_link_up_cb = fh_internal_phy_link_up_cb,
        .brd_link_down_cb = fh_internal_phy_link_down_cb,
    },
};

struct st_platform_info plat_gmac = {
    .name = "fh_qos_gmac", .private_data = &fh_gmac_plat_info,
};
#endif

#ifdef FH_USING_MOL_I2S
static struct fh_i2s_platform_data fh_i2s_data = {
    .dma_capture_channel = AUTO_FIND_CHANNEL,
    .dma_playback_channel = AUTO_FIND_CHANNEL,
    .dma_master = 0,
    .i2s_clk = "i2s_apb_clk",
};

struct st_platform_info plat_i2s = {
    .name = "fh_i2s", .private_data = &fh_i2s_data,
};
#endif

const static struct st_platform_info *platform_info[] = {
#ifdef RT_USING_SDIO
#ifdef WIFI_USING_SDIOWIFI
#if (WIFI_SDIO == 1)
        &plat_mmc0,
#elif(WIFI_SDIO == 0)
        &plat_mmc1,
#endif
#else
    /* &plat_mmc1, */
    &plat_mmc0,
#endif
#endif

#ifdef RT_USING_SPI
    &plat_spi0,
    /* &plat_spi1, */
#endif

#ifdef RT_USING_FAL_SFUD_ADAPT
    &plat_flash,
#endif
#ifdef FH_USING_NAND_FLASH
    &plat_nand_flash,
#endif
#ifdef RT_USING_I2C
    &plat_i2c0,
    &plat_i2c1,
#endif
#ifdef RT_USING_GPIO
    &plat_gpio0,
    &plat_gpio1,
    &plat_gpio2,
    &plat_gpio3,
    &plat_gpio4,
    &plat_gpio5,
    &plat_gpio6,
#endif
#ifdef RT_USING_PWM
    &plat_pwm,
#endif
#ifdef RT_USING_WDT
    &plat_wdt,
#endif
#ifdef FH_USING_SADC
    &plat_sadc,
#endif
    /* &plat_spi1, */
#ifdef FH_USING_AHB_DMA
    &plat_ahb_dma0,
#endif

#ifdef FH_USING_AXI_DMA
    &plat_axi_dma0,
#endif

#ifdef FH_USING_MOL_DMA
    &plat_mol_dma0,
#endif

#ifdef FH_USING_AES
    &plat_aes,
#endif

#ifdef FH_USING_CESA
    &plat_cesa,
#endif

#ifdef FH_USING_EFUSE
    &plat_efuse,
#endif

#ifdef RT_USING_RTC
    &plat_rtc,
#endif


#ifdef RT_USING_USB
    &plat_usbotg,
#endif

#ifdef RT_USING_SPISLAVE
    &plat_spi2,
#endif

#ifdef FH_USING_QOS_GMAC
    &plat_gmac,
#endif

#ifdef FH_USING_FH_PERF
    &plat_fh_perf,
#endif

#ifdef FH_USING_FH_STEPMOTOR
    &plat_smt0,
    &plat_smt1,
#endif

#ifdef FH_USING_MOL_I2S
    &plat_i2s,
#endif

};

/* function body */

/*****************************************************************************
 * Description:
 *      add function description here
 * Parameters:
 *      description for each argument, new argument starts at new line
 * Return:
 *      what does this function returned?
 *****************************************************************************/

void fh_platform_info_register(void)
{
    struct fh_board_info *test_info;
    int i;

    for (i = 0; i < sizeof(platform_info) / sizeof(struct st_platform_info *);
         i++)
    {
        test_info = fh_board_info_register(platform_info[i]->name,
                                           platform_info[i]->private_data);
        if (!test_info)
        {
            rt_kprintf("info_name(%s) failed registered\n",
                       platform_info[i]->name);
        }
    }
}

static struct mem_desc fh_mem_desc[] = {
    {FH_DDR_START, FH_RTT_OS_MEM_END - 1, FH_DDR_START, SECT_RWX_CB, 0,    SECT_MAPPED},
    {FH_RTT_OS_MEM_END, FH_DDR_END - 1, FH_RTT_OS_MEM_END, SECT_RWNX_NCNB_NORMAL, 0,    SECT_MAPPED},
    {0x00200000, 0x00300000 - 1, 0x00200000, SECT_RWNX_NCNB, 0, SECT_MAPPED}, /* GIC */
    {0x08900000, 0x2d2fffff, 0x08900000, SECT_RWNX_NCNB, 0, SECT_MAPPED}, /* Peripheral */
    {0xffff0000, 0xffff0fff, FH_DDR_START, SECT_TO_PAGE, PAGE_ROX_CB, PAGE_MAPPED}, /* Vector */
};

unsigned int get_mmu_table(struct mem_desc **mtable)
{
    *mtable = fh_mem_desc;

    return sizeof(fh_mem_desc)/sizeof(struct mem_desc);
}
