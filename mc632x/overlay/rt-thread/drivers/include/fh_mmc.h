/**
 * Copyright (c) 2015-2019 Shanghai Fullhan Microelectronics Co., Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 *
 * Change Logs:
 * Date           Author       Notes
 * 2019-04-12               the first version
 *
 */

#ifndef __FH_MMC_H__
#define __FH_MMC_H__

#include "fh_def.h"
#include <drivers/mmcsd_core.h>

#define OFFSET_SDC_CTRL (0x0000)
#define OFFSET_SDC_PWREN (0x0004)
#define OFFSET_SDC_CLKDIV (0x0008)
#define OFFSET_SDC_CLKSRC (0x000C)
#define OFFSET_SDC_CLKENA (0x0010)
#define OFFSET_SDC_TMOUT (0x0014)
#define OFFSET_SDC_CTYPE (0x0018)
#define OFFSET_SDC_BLKSIZ (0x001C)
#define OFFSET_SDC_BYTCNT (0x0020)
#define OFFSET_SDC_INTMASK (0x0024)
#define OFFSET_SDC_CMDARG (0x0028)
#define OFFSET_SDC_CMD (0x002C)
#define OFFSET_SDC_RESP0 (0x0030)
#define OFFSET_SDC_RESP1 (0x0034)
#define OFFSET_SDC_RESP2 (0x0038)
#define OFFSET_SDC_RESP3 (0x003C)
#define OFFSET_SDC_MINTSTS (0x0040)
#define OFFSET_SDC_RINTSTS (0x0044)
#define OFFSET_SDC_STATUS (0x0048)
#define OFFSET_SDC_FIFOTH (0x004C)
#define OFFSET_SDC_CDETECT (0x0050)
#define OFFSET_SDC_WRTPRT (0x0054)
#define OFFSET_SDC_GPIO (0x0058)
#define OFFSET_SDC_TCBCNT (0x005C)
#define OFFSET_SDC_TBBCNT (0x0060)
#define OFFSET_SDC_DEBNCE (0x0064)
#define OFFSET_SDC_USRID (0x0068)
#define OFFSET_SDC_VERID (0x006C)
#define OFFSET_SDC_HCON (0x0070)
#define OFFSET_SDC_UHS_REG (0x0074)
#define OFFSET_SDC_RST_N (0x0078)
#define OFFSET_SDC_BMOD (0x0080)
#define OFFSET_SDC_PLDMND (0x0084)
#define OFFSET_SDC_DBADDR (0x0088)
#define OFFSET_SDC_IDSTS (0x008C)
#define OFFSET_SDC_IDINTEN (0x0090)
#define OFFSET_SDC_DSCADDR (0x0094)
#define OFFSET_SDC_BUFADDR (0x0098)
#define OFFSET_SDC_CARDTHRCTL (0x0100)
#define OFFSET_SDC_BACK_END_POWER (0x0104)
#define OFFSET_SDC_FIFO (0x0200)

#define MMC_CMD_FLAG_RESPONSE_EXPECTED BIT(6)
#define MMC_CMD_FLAG_LONG_RESPONSE BIT(7)
#define MMC_CMD_FLAG_CHECK_RESP_CRC BIT(8)
#define MMC_CMD_FLAG_DATA_EXPECTED BIT(9)
#define MMC_CMD_FLAG_WRITE_TO_CARD BIT(10)
#define MMC_CMD_FLAG_DATA_STREAM BIT(11)
#define MMC_CMD_FLAG_AUTO_STOP BIT(12)
#define MMC_CMD_FLAG_WAIT_PREV_DATA BIT(13)
#define MMC_CMD_FLAG_STOP_TRANSFER BIT(14)
#define MMC_CMD_FLAG_SEND_INIT BIT(15)
#define MMC_CMD_FLAG_SWITCH_VOLTAGE BIT(28)

#define MMC_STATUS_DATA_BUSY BIT(9)
#define MMC_CTRL_CONTROLLER_RESET BIT(0)
#define MMC_CTRL_FIFO_RESET BIT(1)
#define MMC_CTRL_DMA_RESET BIT(2)
#define MMC_CTRL_INT_ENABLE BIT(4)
#define MMC_CTRL_USE_DMA BIT(25)
#define MMC_CMD_START_CMD BIT(31)
#define MMC_BMOD_RESET BIT(0)
#define MMC_BMOD_BURST_INCR BIT(1)

#define MMC_CARD_WIDTH_1BIT (0)
#define MMC_CARD_WIDTH_4BIT (1)

#define MMC_INT_STATUS_CARD_DETECT BIT(0)
#define MMC_INT_STATUS_RESPONSE_ERROR BIT(1)
#define MMC_INT_STATUS_CMD_DONE BIT(2)
#define MMC_INT_STATUS_TRANSFER_OVER BIT(3)
#define MMC_INT_STATUS_TX_REQUEST BIT(4)
#define MMC_INT_STATUS_RX_REQUEST BIT(5)
#define MMC_INT_STATUS_RESP_CRC_ERROR BIT(6)
#define MMC_INT_STATUS_DATA_CRC_ERROR BIT(7)
#define MMC_INT_STATUS_RESPONSE_TIMEOUT BIT(8)
#define MMC_INT_STATUS_READ_TIMEOUT BIT(9)
#define MMC_INT_STATUS_STARVATION_TIMEOUT BIT(10)
#define MMC_INT_STATUS_OVERRUN_UNDERRUN BIT(11)
#define MMC_INT_STATUS_HARDWARE_LOCKED BIT(12)
#define MMC_INT_STATUS_START_BIT_ERROR BIT(13)
#define MMC_INT_STATUS_AUTO_CMD_DONE BIT(14)
#define MMC_INT_STATUS_END_BIT_ERROR BIT(15)
#define MMC_INT_STATUS_SDIO BIT(16)
#define MMC_INT_STATUS_ALL (~0)

#define MMC_INIT_STATUS_DATA_ERROR                                    \
    (MMC_INT_STATUS_DATA_CRC_ERROR | MMC_INT_STATUS_START_BIT_ERROR | \
     MMC_INT_STATUS_END_BIT_ERROR | MMC_INT_STATUS_STARVATION_TIMEOUT)

#define MMC_USE_DMA

#ifdef MMC_USE_DMA
#define MMC_INT_STATUS_DATA \
    (MMC_INT_STATUS_TRANSFER_OVER | MMC_INIT_STATUS_DATA_ERROR)
#else
#define MMC_INT_STATUS_DATA                                      \
    (MMC_INT_STATUS_TRANSFER_OVER | MMC_INIT_STATUS_DATA_ERROR | \
     MMC_INT_STATUS_TX_REQUEST | MMC_INT_STATUS_RX_REQUEST)
#endif

#define MMC_DMA_DESC_BUFF_SIZE (0x1f00)

/* 命令响应超时：SD 规范里卡必须在 0.1s 内应答，2s 已经非常宽。
 * 原来 5s，一旦卡不应答，FatFs 卷锁会被占住 5s，网页/录像全部卡住；
 * 缩短到 2s 让复位和重试来得更快。*/
#define MMC_CMD_DONE_TIMEOUT (RT_TICK_PER_SECOND * 2)
#define MMC_DMA_DONE_TIMEOUT (RT_TICK_PER_SECOND * 5)
#define MMC_CARD_IDLE_TIMEOUT (RT_TICK_PER_SECOND * 5)
#define MMC_UPDATE_CLK_TIMEOUT (RT_TICK_PER_SECOND * 1)
#define MMC_SEND_CMD_TIMEOUT (RT_TICK_PER_SECOND * 1)
#define MMC_RESET_FIFO_TIMEOUT (RT_TICK_PER_SECOND * 1)
#define MMC_RESET_SDC_TIMEOUT (RT_TICK_PER_SECOND * 1)


/* INT register */
#define SDIO_INT_STA_CD     (1 << 0)
#define SDIO_INT_STA_RE     (1 << 1)
#define SDIO_INT_STA_CDONE  (1 << 2)
#define SDIO_INT_STA_DTO    (1 << 3)
#define SDIO_INT_STA_TXDR   (1 << 4)
#define SDIO_INT_STA_RXDR   (1 << 5)
#define SDIO_INT_STA_RCRC   (1 << 6)
#define SDIO_INT_STA_DCRC   (1 << 7)
#define SDIO_INT_STA_RTO    (1 << 8)
#define SDIO_INT_STA_DRTO   (1 << 9)
#define SDIO_INT_STA_HTO    (1 << 10)
#define SDIO_INT_STA_FRUN   (1 << 11)
#define SDIO_INT_STA_HLE    (1 << 12)
#define SDIO_INT_STA_SBE    (1 << 13)
#define SDIO_INT_STA_ACD    (1 << 14)
#define SDIO_INT_STA_EBE    (1 << 15)


#define FH_SDIO_HW_ERR  (SDIO_INT_STA_RE | SDIO_INT_STA_RCRC | SDIO_INT_STA_DCRC |\
            SDIO_INT_STA_RTO | SDIO_INT_STA_DRTO | SDIO_INT_STA_HTO | SDIO_INT_STA_FRUN |\
            SDIO_INT_STA_HLE | SDIO_INT_STA_SBE | SDIO_INT_STA_EBE)

typedef union {
    struct
    {
        UINT32 reserved : 1;                         /* 0~15 */
        UINT32 disable_interrupt_on_completion : 1;  /* 16~31 */
        UINT32 last_descriptor : 1;                  /* 0~15 */
        UINT32 first_descriptor : 1;                 /* 16~31 */
        UINT32 sencond_address_chained : 1;          /* 0~15 */
        UINT32 end_of_ring : 1;                      /* 16~31 */
        UINT32 reserved_29_6 : 24;                   /* 0~15 */
        UINT32 card_error_summary : 1;               /* 16~31 */
        UINT32 own : 1;                              /* 16~31 */
    } bit;
    UINT32 dw;
} MMC_DMA_Descriptor0;

typedef union {
    struct
    {
        UINT32 buffer1_size : 13;   /* 0~15 */
        UINT32 buffer2_size : 13;   /* 16~31 */
        UINT32 reserved_26_31 : 6;  /* 0~15 */
    } bit;
    UINT32 dw;
} MMC_DMA_Descriptor1;

typedef union {
    struct
    {
        UINT32 buffer_addr0 : 32;  /* 0~15 */
    } bit;
    UINT32 dw;
} MMC_DMA_Descriptor2;

typedef union {
    struct
    {
        UINT32 buffer_addr1 : 32; /* 0~15 */
    } bit;
    UINT32 dw;
} MMC_DMA_Descriptor3;

typedef struct
{
    MMC_DMA_Descriptor0 desc0; /* control and status information of descriptor */
    MMC_DMA_Descriptor1 desc1; /* buffer sizes */
    MMC_DMA_Descriptor2 desc2; /* physical address of the buffer 1 */
    MMC_DMA_Descriptor3 desc3; /* physical address of the buffer 2 */
} MMC_DMA_Descriptors;
#ifdef SD1_MULTI_SLOT
struct fh_mmc_slot_selection
{
    rt_uint32_t slotid;
    int vdd_gpio_idx;
};
#endif

struct fh_mmc_obj
{
    int id;
    rt_uint32_t irq;
    rt_uint32_t base;
    rt_uint32_t sd_hz;
    rt_uint32_t sdio_hz;
    int power_pin_gpio;
    rt_uint32_t fifo_depth;
    MMC_DMA_Descriptors *descriptors;
    rt_uint32_t *dma_buf;
    void (*mmc_reset)(struct fh_mmc_obj *);
    rt_uint32_t sd_bus_width;
#ifdef CONFIG_GPIO_SD_CD
    int cd_gpio;  /* custom defined gpio for cd, default -1 when using intenal cd */
#endif
#ifdef SD1_MULTI_SLOT
    void (*mmc_switch_slot)(rt_uint32_t);
    int slotscnt;
    struct fh_mmc_slot_selection *mmc_slot_selections;
#endif
    struct rt_device parent;
#define FH_MMC_PM_STAT_NORMAL    0
#define FH_MMC_PM_STAT_SUSPNED    0x55aaaa55
    rt_uint32_t pm_stat;
    struct rt_mmcsd_io_cfg cur_cfg;
    struct rt_mmcsd_host *cur_host;
    struct rt_mmcsd_req *cur_req;
    struct mmc_driver *cur_driver;

};

void MMC_SetBlockSize(struct fh_mmc_obj *mmc_obj, rt_uint32_t size);
void MMC_SetByteCount(struct fh_mmc_obj *mmc_obj, rt_uint32_t bytes);
rt_uint32_t MMC_GetWaterlevel(struct fh_mmc_obj *mmc_obj);
rt_uint32_t MMC_GetResponse(struct fh_mmc_obj *mmc_obj, int resp_num);
rt_uint32_t MMC_GetRegCmd(struct fh_mmc_obj *mmc_obj);
rt_uint32_t MMC_GetRegCtrl(struct fh_mmc_obj *mmc_obj);
rt_uint32_t MMC_SetInterruptEnable(struct fh_mmc_obj *mmc_obj,
                                        rt_uint32_t mask);
rt_uint32_t MMC_GetInterruptEnable(struct fh_mmc_obj *mmc_obj);
rt_uint32_t MMC_ClearRawInterrupt(struct fh_mmc_obj *mmc_obj,
                                         rt_uint32_t interrupts);
rt_uint32_t MMC_GetRawInterrupt(struct fh_mmc_obj *mmc_obj);
rt_uint32_t MMC_GetStatus(struct fh_mmc_obj *mmc_obj);
rt_uint32_t MMC_GetCardStatus(struct fh_mmc_obj *mmc_obj);
rt_uint32_t MMC_GetUnmaskedInterrupt(struct fh_mmc_obj *mmc_obj);
rt_uint32_t MMC_IsDataStateBusy(struct fh_mmc_obj *mmc_obj);

void MMC_Init(struct fh_mmc_obj *mmc_obj);
int MMC_ResetFifo(struct fh_mmc_obj *mmc_obj);
int MMC_SetCardWidth(struct fh_mmc_obj *mmc_obj, int width);
int MMC_UpdateClockRegister(struct fh_mmc_obj *mmc_obj, int div);
int MMC_SendCommand(struct fh_mmc_obj *mmc_obj, rt_uint32_t cmd,
                    rt_uint32_t arg, rt_uint32_t flags);
int MMC_WriteData(struct fh_mmc_obj *mmc_obj, rt_uint32_t *buf,
                  rt_uint32_t size);
int MMC_ReadData(struct fh_mmc_obj *mmc_obj, rt_uint32_t *buf,
                 rt_uint32_t size);

void MMC_StartDma(struct fh_mmc_obj *mmc_obj);
void MMC_StopDma(struct fh_mmc_obj *mmc_obj);
void MMC_InitDescriptors(struct fh_mmc_obj *mmc_obj, rt_uint32_t *buf,
                         rt_uint32_t size);
rt_uint32_t MMC_GetIdmaStete(struct fh_mmc_obj *mmc_obj);
#endif /* FH_MMC_H_ */
