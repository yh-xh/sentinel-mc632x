/**
 * Copyright (c) 2015-2019 Shanghai Fullhan Microelectronics Co., Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 *
 * Change Logs:
 * Date           Author       Notes
 * 2019-01-24     tangyh    the first version
 *
 */
#include <rtdef.h>
#include <rthw.h>
#include <rtdevice.h>
#include <drivers/mmcsd_core.h>
#include <fh_mmc.h>
#include "board_info.h"
#include "dma_mem.h"
#include "gpio.h"
#include "pinctrl.h"
#include "mmc.h"
#include "delay.h"
#ifdef RT_USING_PM
#include <pm.h>
#endif
/* #define FH_MMC_DEBUG */
#define MMC_USE_INTERNAL_BUF
#define PM_MMC_WAIT_MAX_TIME 0xffff
#if defined(FH_MMC_DEBUG) && defined(RT_DEBUG)
#define PRINT_MMC_DBG(fmt, args...)                           \
    do                                                        \
    {                                                         \
        rt_kprintf("FH_MMC_DEBUG: tick-%d, ", rt_tick_get()); \
        rt_kprintf(fmt, ##args);                              \
    } while (0)
#else
#define PRINT_MMC_DBG(fmt, args...) \
    do                              \
    {                               \
    } while (0)
#endif

#define PRINT_MMC_REGS(base)                                      \
    do                                                            \
    {                                                             \
        int i_for_marco;                                          \
        rt_uint32_t addr;                                         \
        for (i_for_marco = 0; i_for_marco < 20; i_for_marco++)    \
        {                                                         \
            addr = base + i_for_marco * 4 * 4;                    \
            rt_kprintf("0x%x: 0x%x, 0x%x, 0x%x, 0x%x\n", addr,    \
                       GET_REG(addr + 0x0), GET_REG(addr + 0x4),  \
                       GET_REG(addr + 0x8), GET_REG(addr + 0xc)); \
        }                                                         \
    } while (0)

#ifndef MMC_INTERNAL_DMA_BUF_SIZE
#define MMC_INTERNAL_DMA_BUF_SIZE (64 * 1024)
#endif

extern void mmu_clean_invalidated_dcache(rt_uint32_t buffer, rt_uint32_t size);
extern void mmu_invalidate_dcache(rt_uint32_t buffer, rt_uint32_t size);

static void printf_buf(unsigned int *p_32, unsigned int size_int)
{
    int i;

    for (i = 0; i < size_int; i++)
    {
        if (i !=0 && i%4 == 0)
            rt_kprintf("\n");
        if (i%4 == 0)
            rt_kprintf("[%08x] :: ",(unsigned int)&p_32[i]);
        rt_kprintf("%08x ",p_32[i]);
    }

    rt_kprintf("\n");
}

#ifdef RT_USING_PM
#define MMC_CTRL_IDLE 0
#define MMC_CTRL_BUSY 1
static rt_uint32_t fh_mmc_get_ctrl_busy_state(struct fh_mmc_obj *p_mmc)
{
    rt_uint32_t ret0, ret1;

    ret0 = MMC_GetStatus(p_mmc);
    ret1 = MMC_GetIdmaStete(p_mmc);
    /*cmd for bit[4-7], data for bit 10, dma state for bit[13-16]*/
    if ((ret0 & 0xf0) || (ret0 & (1 << 10)) || (ret1 & 0x1e000))
        return MMC_CTRL_BUSY;
    else
        return MMC_CTRL_IDLE;
}

static rt_uint32_t fh_mmc_pm_stat_get(struct fh_mmc_obj *p_mmc)
{
    rt_uint32_t ret;

    ret = p_mmc->pm_stat;

    return ret;
}

static void fh_mmc_pm_stat_set(struct fh_mmc_obj *p_mmc, rt_uint32_t pm_stat)
{
    p_mmc->pm_stat = pm_stat;
}
#endif

static int fh_mmc_write_pio(struct mmc_driver *mmc_drv)
{
    struct fh_mmc_obj *mmc_obj = (struct fh_mmc_obj *)mmc_drv->priv;
    struct rt_mmcsd_cmd *cmd   = mmc_drv->cmd;
    struct rt_mmcsd_data *data = RT_NULL;
    rt_uint32_t size;

    if (cmd)
        data = cmd->data;

    if (!data)
    {
        rt_kprintf("ERROR: %s, data is NULL\n", __func__);
        return -RT_EIO;
    }

    size = data->blks * data->blksize;
    PRINT_MMC_DBG("%s, Send %d bytes\n", __func__, size);
    MMC_WriteData(mmc_obj, data->buf, size);
    MMC_ResetFifo(mmc_obj);

    return 0;
}

static int fh_mmc_read_pio(struct mmc_driver *mmc_drv)
{
    struct fh_mmc_obj *mmc_obj = (struct fh_mmc_obj *)mmc_drv->priv;
    struct rt_mmcsd_cmd *cmd   = mmc_drv->cmd;
    struct rt_mmcsd_data *data = RT_NULL;
    rt_uint32_t size;
    int ret;

    if (cmd)
        data = cmd->data;

    if (!data)
    {
        rt_kprintf("ERROR: %s, data is NULL\n", __func__);
        return -RT_EIO;
    }

    size = data->blks * data->blksize;
    PRINT_MMC_DBG("%s, read %d bytes\n", __func__, size);
    ret = MMC_ReadData(mmc_obj, data->buf, size);
    if (ret)
    {
        rt_kprintf("ERROR: %s, fifo IO error, ret: %d\n", __func__, ret);
        return -RT_EIO;
    }

    MMC_ResetFifo(mmc_obj);

    return 0;
}

static inline void fh_mmc_cd_enable(struct fh_mmc_obj *mmc_obj)
{
#ifdef CONFIG_GPIO_SD_CD
    if (mmc_obj->cd_gpio < 0)
#endif
        MMC_SetInterruptEnable(mmc_obj, MMC_INT_STATUS_CARD_DETECT);
}
static void fh_mmc_set_iocfg(struct rt_mmcsd_host *host,
                             struct rt_mmcsd_io_cfg *io_cfg)
{
    rt_uint32_t clkdiv;
    struct mmc_driver *mmc_drv = host->private_data;
    struct fh_mmc_obj *mmc_obj = (struct fh_mmc_obj *)mmc_drv->priv;

    PRINT_MMC_DBG("%s start\n", __func__);

    /* maybe switch power to the card */
    switch (io_cfg->power_mode)
    {
    case MMCSD_POWER_OFF:
        break;
    case MMCSD_POWER_UP:
        PRINT_MMC_DBG("[mmc] power up reset mmc controle %d\n", MMCSD_POWER_UP);
        MMC_Init(mmc_obj);
        fh_mmc_cd_enable(mmc_obj);
        break;
    case MMCSD_POWER_ON:
        break;
    default:
        rt_kprintf("ERROR: %s, unknown power_mode %d\n", __func__,
                   io_cfg->power_mode);
        break;
    }

    /* fixme: read from PMU */
    /* why io_cfg->clock == 0 ? */
    if (io_cfg->clock)
    {
        clkdiv = mmc_obj->sdio_hz / io_cfg->clock / 2;
        MMC_UpdateClockRegister(mmc_obj, clkdiv);
        PRINT_MMC_DBG("io_cfg->clock: %lu, clock in: %lu, clkdiv: %d\n",
                      io_cfg->clock, mmc_obj->sdio_hz, clkdiv);
    }

    if (io_cfg->bus_width == MMCSD_BUS_WIDTH_4)
    {
        MMC_SetCardWidth(mmc_obj, MMC_CARD_WIDTH_4BIT);
        PRINT_MMC_DBG("set to 4-bit mode\n", mmc_obj->sdio_hz, clkdiv);
    }
    else
    {
        MMC_SetCardWidth(mmc_obj, MMC_CARD_WIDTH_1BIT);
        PRINT_MMC_DBG("set to 1-bit mode\n", mmc_obj->sdio_hz, clkdiv);
    }
    rt_memcpy(&mmc_obj->cur_cfg, io_cfg, sizeof(struct rt_mmcsd_io_cfg));
    PRINT_MMC_DBG("%s end\n", __func__);
}

static void fh_mmc_enable_sdio_irq(struct rt_mmcsd_host *host,
                                   rt_int32_t enable)
{
    struct mmc_driver *mmc_drv = host->private_data;
    struct fh_mmc_obj *mmc_obj = (struct fh_mmc_obj *)mmc_drv->priv;
    rt_uint32_t reg;

    PRINT_MMC_DBG("%s start\n", __func__);

    if (enable)
    {
        MMC_ClearRawInterrupt(mmc_obj, MMC_INT_STATUS_SDIO);
        reg = MMC_GetInterruptEnable(mmc_obj);
        reg |= MMC_INT_STATUS_SDIO;
        MMC_SetInterruptEnable(mmc_obj, reg);
    }
    else
    {
        reg = MMC_GetInterruptEnable(mmc_obj);
        reg &= ~MMC_INT_STATUS_SDIO;
        MMC_SetInterruptEnable(mmc_obj, reg);
    }
}
#ifdef SD1_MULTI_SLOT
static void fh_mmc_set_active_slot(struct rt_mmcsd_host *host, rt_int32_t slotidx)
{
    mmcsd_change_local(host, RT_FALSE);  /* force disconnect */

    struct mmc_driver *mmc_drv = (struct mmc_driver *)host->private_data;
    struct fh_mmc_obj *mmc_obj = (struct fh_mmc_obj *)mmc_drv->priv;

    mmc_drv->slotid = slotidx;

    /* disable pow of others */
    int i = 0;

    for (i = 0; i < mmc_obj->slotscnt; i++)
    {
        if (mmc_obj->mmc_slot_selections[i].vdd_gpio_idx > 0 && mmc_obj->mmc_slot_selections[i].slotid != slotidx)
        {
            gpio_direction_output(mmc_obj->mmc_slot_selections[i].vdd_gpio_idx, 1);
        }
    }

    if (mmc_obj->mmc_switch_slot != NULL)
    {
        mmc_obj->mmc_switch_slot(slotidx);
    }

    for (i = 0; i < mmc_obj->slotscnt; i++)
    {
        if (mmc_obj->mmc_slot_selections[i].vdd_gpio_idx > 0 && mmc_obj->mmc_slot_selections[i].slotid == slotidx)
        {
            gpio_direction_output(mmc_obj->mmc_slot_selections[i].vdd_gpio_idx, 0);
        }
    }

    mmcsd_change_local(host, RT_TRUE);  /* force connect */
}
#endif
static rt_int32_t fh_mmc_get_card_status(struct rt_mmcsd_host *host)
{
    PRINT_MMC_DBG("%s, start\n", __func__);
    PRINT_MMC_DBG("%s, end\n", __func__);
    return 0;
}

static void fh_mmc_send_command(struct mmc_driver *mmc_drv,
                                struct rt_mmcsd_cmd *cmd)
{
    struct fh_mmc_obj *mmc_obj = (struct fh_mmc_obj *)mmc_drv->priv;
/* struct rt_mmcsd_host *host = mmc_drv->host; */
    struct rt_mmcsd_req *req   = mmc_drv->req;
    /* fixme: cmd->data or req->data */
    struct rt_mmcsd_data *data;
    int ret;

/* rt_uint32_t retries   = 0; */
    rt_uint32_t cmd_flags = 0;

    PRINT_MMC_DBG("%s, start\n", __func__);

    if (!cmd)
    {
        /* fixme: stop dma */
        rt_kprintf("ERROR: %s, cmd is NULL\n", __func__);
        return;
    }
    data = cmd->data;
    if (data)
    {
        cmd_flags |= MMC_CMD_FLAG_DATA_EXPECTED;
        /* always set data start - also set direction flag for read */
        if (data->flags & DATA_DIR_WRITE)
            cmd_flags |= MMC_CMD_FLAG_WRITE_TO_CARD;

        if (data->flags & DATA_STREAM)
            cmd_flags |= MMC_CMD_FLAG_DATA_STREAM;
    }

    if (cmd == req->stop)
        cmd_flags |= MMC_CMD_FLAG_STOP_TRANSFER;
    else
        cmd_flags |= MMC_CMD_FLAG_WAIT_PREV_DATA;

    switch (resp_type(cmd))
    {
    case RESP_NONE:
        break;
    case RESP_R1:
    case RESP_R5:
    case RESP_R6:
    case RESP_R7:
    case RESP_R1B:
        cmd_flags |= MMC_CMD_FLAG_RESPONSE_EXPECTED;
        cmd_flags |= MMC_CMD_FLAG_CHECK_RESP_CRC;
        break;
    case RESP_R2:
        cmd_flags |= MMC_CMD_FLAG_RESPONSE_EXPECTED;
        cmd_flags |= MMC_CMD_FLAG_CHECK_RESP_CRC;
        cmd_flags |= MMC_CMD_FLAG_LONG_RESPONSE;
        break;
    case RESP_R3:
    case RESP_R4:
        cmd_flags |= MMC_CMD_FLAG_RESPONSE_EXPECTED;
        break;
    default:
        rt_kprintf("ERROR: %s, unknown cmd type %x\n", __func__,
                   resp_type(cmd));
        return;
    }

    if (cmd->cmd_code == GO_IDLE_STATE)
        cmd_flags |= MMC_CMD_FLAG_SEND_INIT;

    /* CMD 11 check switch voltage */
    if (cmd->cmd_code == READ_DAT_UNTIL_STOP)
        cmd_flags |= MMC_CMD_FLAG_SWITCH_VOLTAGE;

    PRINT_MMC_DBG("cmd code: %d, args: 0x%x, resp type: 0x%x, flag: 0x%x\n",
                  cmd->cmd_code, cmd->arg, resp_type(cmd), cmd_flags);
    ret = MMC_SendCommand(mmc_obj, cmd->cmd_code, cmd->arg, cmd_flags);

    if (ret)
        rt_kprintf("ERROR: %s, Send command timeout, cmd: %d, status: 0x%x\n",
                   __func__, cmd->cmd_code, MMC_GetStatus(mmc_obj));
}
static void fh_mmc_perpare_data(struct mmc_driver *mmc_drv)
{
    struct rt_mmcsd_cmd *cmd   = mmc_drv->cmd;
    struct rt_mmcsd_data *data = cmd->data;
    struct fh_mmc_obj *mmc_obj = (struct fh_mmc_obj *)mmc_drv->priv;
    rt_uint32_t data_size;
/* int i; */

    if (!data)
    {
        MMC_SetBlockSize(mmc_obj, 0);
        MMC_SetByteCount(mmc_obj, 0);
        return;
    }

    PRINT_MMC_DBG("%s, start\n", __func__);

    if (MMC_ResetFifo(mmc_obj))
        return;

    data_size = data->blks * data->blksize;

    MMC_SetBlockSize(mmc_obj, data->blksize);

    if (data_size % 4)
        rt_kprintf(
            "ERROR: data_size should be a multiple of 4, but now is %d\n",
            data_size);
    MMC_SetByteCount(mmc_obj, data_size);

    PRINT_MMC_DBG("%s, set blk size: 0x%x, byte count: 0x%x\n", __func__,
                  data->blksize, data_size);

    if (data_size > MMC_DMA_DESC_BUFF_SIZE * mmc_drv->max_desc)
    {
        rt_kprintf(
            "ERROR: %s, given buffer is too big, size: 0x%x, max: 0x%x\n",
            __func__, data_size, MMC_DMA_DESC_BUFF_SIZE * mmc_drv->max_desc);
        return;
    }

    if (data_size > MMC_INTERNAL_DMA_BUF_SIZE)
    {
        rt_kprintf("ERROR: please increase MMC_INTERNAL_DMA_BUF_SIZE.\n");
        return;
    }

#ifdef MMC_USE_DMA
#ifdef MMC_USE_INTERNAL_BUF
    if (data->flags & DATA_DIR_WRITE)
    {
        rt_memcpy(mmc_obj->dma_buf, data->buf, data_size);
        mmu_clean_invalidated_dcache((rt_uint32_t)mmc_obj->dma_buf, data_size);
    }
    else
        mmu_invalidate_dcache((rt_uint32_t)mmc_obj->dma_buf, data_size);
    MMC_InitDescriptors(mmc_obj, (rt_uint32_t *)mmc_obj->dma_buf, data_size);
    MMC_StartDma(mmc_obj);
#else
    MMC_InitDescriptors(mmc_obj, data->buf, data_size);
    mmu_clean_invalidated_dcache((rt_uint32_t)data->buf, data_size);
    MMC_StartDma(mmc_obj);
#endif
#endif
    PRINT_MMC_DBG("%s, end\n", __func__);
}

int fh_mmc_wait_card_idle(struct fh_mmc_obj *mmc_obj)
{
    rt_tick_t start = rt_tick_get();
    int       spins = 0;

    /*
     * 原写法用 “MMC_CARD_IDLE_TIMEOUT(5s) * 1000 次 × udelay(1)” 计时，
     * 实际只有 0.5s 左右，而且是纯忙等。大块写入后卡内部编程/擦除经常
     * 要几百毫秒，超时提前返回会让上层复位控制器 —— 卡还在忙，主机就
     * 开始发下一条命令，接着就是 CMD24 无响应、后面所有命令全超时的
     * “SD 掉卡”。这里改成按 tick 计满 5s。
     *
     * 前 1ms 继续忙等（小写入/元数据操作都是瞬时完成，不能让 10ms 的
     * tick 粒度拖慢它们），超过 1ms 说明卡在做内部编程，改成 mdelay
     * 让出 CPU；请求路径是线程上下文，持有 FatFs 卷锁也不会重入。
     */
    while (MMC_GetStatus(mmc_obj) & MMC_STATUS_DATA_BUSY)
    {
        if ((rt_tick_get() - start) > (rt_tick_t)MMC_CARD_IDLE_TIMEOUT)
            return -RT_ETIMEOUT;

        if (++spins < 100)
            udelay(10);
        else
            rt_thread_mdelay(1);
    }
    return 0;
}

static int fh_mmc_get_response(struct mmc_driver *mmc_drv,
                               struct rt_mmcsd_cmd *cmd)
{
    int i;
    rt_uint32_t tick, timeout, status;
    struct fh_mmc_obj *mmc_obj = (struct fh_mmc_obj *)mmc_drv->priv;

    cmd->resp[0] = 0;
    cmd->resp[1] = 0;
    cmd->resp[2] = 0;
    cmd->resp[3] = 0;

    tick    = rt_tick_get();
    timeout = tick + MMC_CMD_DONE_TIMEOUT;  /* 2s */

    /* fixme: spin_lock_irqsave? */
    while (1)
    {
        status = MMC_GetRawInterrupt(mmc_obj);
        if (status & MMC_INT_STATUS_CMD_DONE)
            break;
        tick   = rt_tick_get();
        if (tick > timeout)
        {
            status = MMC_GetRawInterrupt(mmc_obj);
            if (status & MMC_INT_STATUS_CMD_DONE)
                break;
            PRINT_MMC_DBG(
                "ERROR: %s, get response timeout(cmd is not received by card), RINTSTS: 0x%x, cmd: %d\n",
                __func__, status, cmd->cmd_code);
            return -RT_ETIMEOUT;
        }
    }

    MMC_ClearRawInterrupt(mmc_obj, MMC_INT_STATUS_CMD_DONE);

    for (i = 0; i < 4; i++)
    {
        if (resp_type(cmd) == RESP_R2)
        {
            cmd->resp[i] = MMC_GetResponse(mmc_obj, 3 - i);
            /* fixme : R2 must delay some time here ,when use UHI card, need */
            /* check why */
            /* 1ms */
            /* rt_thread_sleep(RT_TICK_PER_SECOND / 100); */
        }
        else
            cmd->resp[i] = MMC_GetResponse(mmc_obj, i);
    }

    PRINT_MMC_DBG("resp: 0x%x, 0x%x, 0x%x, 0x%x\n", cmd->resp[0], cmd->resp[1],
                  cmd->resp[2], cmd->resp[3]);

    if (status & MMC_INT_STATUS_RESPONSE_TIMEOUT)
    {
        MMC_ClearRawInterrupt(mmc_obj, MMC_INT_STATUS_RESPONSE_TIMEOUT);
        PRINT_MMC_DBG("ERROR: %s, get response timeout, RINTSTS: 0x%x\n",
                      __func__, status);
        return -RT_ETIMEOUT;
    }

    else if (status &
             (MMC_INT_STATUS_RESP_CRC_ERROR | MMC_INT_STATUS_RESPONSE_ERROR))
    {
        MMC_ClearRawInterrupt(mmc_obj, MMC_INT_STATUS_RESP_CRC_ERROR |
                                           MMC_INT_STATUS_RESPONSE_ERROR);
        rt_kprintf(
            "ERROR: %s, response error or response crc error, RINTSTS: 0x%x\n",
            __func__, status);
        /* return -RT_ERROR; */
    }

    return 0;
}

int wait_xfer_done_poll_mode(struct fh_mmc_obj *mmc_obj)
{
    rt_int32_t timeout = 1000000;
    rt_int32_t schedule_timeout = 1000000;
    rt_uint32_t interrupt, status, reg;
    struct rt_thread *thread;

    while (schedule_timeout)
    {
        interrupt = MMC_GetRawInterrupt(mmc_obj);
        if (interrupt & FH_SDIO_HW_ERR)
        {
            /*if write .. wait done.*/
            status = MMC_GetCmd(mmc_obj);
            if (status & (1 << 10))
            {
                while (timeout--)
                {
                    status = MMC_GetRawInterrupt(mmc_obj);
                    if (status & SDIO_INT_STA_DTO)
                        break;
                    udelay(1);
                }
            }
            if (!timeout)
            {
                thread = rt_thread_self();
                rt_kprintf("%s :: thread[%s] wait hw err %x timeout\n",__func__,thread->name);
            }
            rt_kprintf("%s :: got err %x\n",__func__,interrupt);
            return -RT_ERROR;
        }
        if (interrupt & SDIO_INT_STA_DTO)
            break;
        udelay(1);
        schedule_timeout--;
    }

    if (!schedule_timeout)
    {
        thread = rt_thread_self();
        rt_kprintf("%s :: thread[%s] wait xfer done timeout\n",__func__,thread->name);
        return -RT_ETIMEOUT;
    }
    return 0;
}

int check_switch(struct fh_mmc_obj *mmc_obj, struct mmc_driver *mmc_drv)
{

    if (mmc_obj->cur_req != mmc_drv->req)
        return -1;
    /*tbd check hw cmd and arg is euq sw*/
    return 0;
}

static int fh_mmc_start_transfer(struct mmc_driver *mmc_drv, rt_uint32_t *pass_cmd)
{
    struct fh_mmc_obj *mmc_obj = (struct fh_mmc_obj *)mmc_drv->priv;
/* struct rt_mmcsd_host *host = mmc_drv->host; */
/* struct rt_mmcsd_req *req   = mmc_drv->req; */
    struct rt_mmcsd_cmd *cmd   = mmc_drv->cmd;
    struct rt_mmcsd_data *data = RT_NULL;
    int ret;
    int switch_ret;
    rt_uint32_t data_size;
    rt_uint32_t interrupt, status, reg;

    if (!cmd)
        return 0;

    data = cmd->data;
    if (!data)
        return 0;

    PRINT_MMC_DBG("%s, start\n", __func__);
    *pass_cmd = 0;
    data_size = data->blks * data->blksize;
    if (data_size <= 512)
    {
        ret = wait_xfer_done_poll_mode(mmc_obj);
        MMC_ClearRawInterrupt(mmc_obj, MMC_INT_STATUS_ALL);
        if (ret)
            return -RT_ETIMEOUT;
    }
    else
    {
        /* fixme: spin_lock_irqsave(&host->lock, flags); */
        /* open data interrupts */
        reg = MMC_GetInterruptEnable(mmc_obj);
        reg |= MMC_INT_STATUS_DATA;
        MMC_SetInterruptEnable(mmc_obj, reg);

        /* fixme: spin_unlock_irqrestore(&host->lock, flags); */
        mmc_obj->cur_req = mmc_drv->req;
        ret = rt_completion_wait(&mmc_drv->transfer_completion, MMC_DMA_DONE_TIMEOUT);

        reg = MMC_GetInterruptEnable(mmc_obj);
        reg &= ~MMC_INT_STATUS_DATA;
        MMC_SetInterruptEnable(mmc_obj, reg);
        switch_ret = check_switch(mmc_obj, mmc_drv);
        if (switch_ret)
        {
            rt_kprintf("%s :: got thread switch case here...\n",__func__);
            /*tell core to resend cmd again...*/
            mmc_obj->cur_req->cmd->err = -1;
            *pass_cmd = 1;
            return -RT_ETIMEOUT;
        }
        if (ret)
        {
            /* fixme: error handle */
            cmd->err  = ret;
            interrupt = MMC_GetRawInterrupt(mmc_obj);
            status    = MMC_GetStatus(mmc_obj);
            rt_kprintf(
                "ERROR: %s, transfer timeout, ret: %d, RINTSTS: 0x%x, STATUS: 0x%x\n",
                __func__, ret, interrupt, status);
            printf_buf(mmc_obj->base, 0x80);
            /* PRINT_MMC_REGS(mmc_obj->base); */
            return -RT_ETIMEOUT;
        }
    }

    /*
     * 数据 CRC / 起始位错误时 ISR 只把状态写进 data->err，旧代码这里不检查，
     * 于是 FatFs 会把坏块当成写成功——存证 JPEG 就会出现中间缺块、只能显示
     * 上半截。把错误补回 cmd->err，让 mmcsd 核心重试，FatFs 也能拿到真实结果。
     */
    if (data->err)
    {
        rt_kprintf("ERROR: %s, data error 0x%x on cmd %d\n",
                   __func__, (unsigned)data->err, (int)cmd->cmd_code);
        cmd->err = -RT_EIO;
        return -RT_EIO;
    }

    data->bytes_xfered = data->blks * data->blksize;

#ifdef MMC_USE_INTERNAL_BUF
    if (!(data->flags & DATA_DIR_WRITE))
    {
        mmu_invalidate_dcache((rt_uint32_t)mmc_obj->dma_buf, data->bytes_xfered);
        rt_memcpy(data->buf, mmc_obj->dma_buf, data->bytes_xfered);
    }
#endif

    return 0;
}

static void fh_mmc_complete_request(struct mmc_driver *mmc_drv)
{
    struct fh_mmc_obj *mmc_obj = (struct fh_mmc_obj *)mmc_drv->priv;
#ifdef MMC_USE_DMA
    MMC_StopDma(mmc_obj);
#endif
    mmc_drv->cmd  = RT_NULL;
    mmc_drv->req  = RT_NULL;
    mmc_drv->data = RT_NULL;

    /*rt_memset(mmc_obj->descriptors, 0, 4096);*/

    MMC_SetBlockSize(mmc_obj, 0);
    MMC_SetByteCount(mmc_obj, 0);

    mmcsd_req_complete(mmc_drv->host);
}

/*
 * 一次请求失败后控制器可能还停在命令/传输态（CMD 的 start 位不释放），
 * 之后的命令会全部超时，看起来像“卡掉了”，其实只能重启。
 * 这里统一复位控制器并恢复时钟/位宽，让后面的请求还能发出去。
 *
 * 另外做两级降速：30 秒内连续两次复位说明当前时钟下信号/时序已经不可靠
 * （这块板 LCD 和 SD0 共用引脚，1-bit 25MHz 只是经验值），把 SD 时钟减半
 * 再试，最低降到 6.25MHz。降速后一直保持到重启/重新挂载，避免反复抖动。
 */
#define MMC_RECOVER_WINDOW_MS   30000u
#define MMC_RECOVER_BURST_MAX   2u
#define MMC_CLK_FLOOR_HZ        6250000u

static unsigned g_mmc_recover_total;
static unsigned g_mmc_recover_last_ms;
static unsigned g_mmc_recover_burst;

static struct rt_mmcsd_host *g_mmc_host0;

/*
 * 让 mmcsd 核心把卡当作“拔插一次”：删掉块设备和卡对象，再重新
 * CMD0/ACMD41 初始化并重建 mmcblk0[pN]。卡被一次超时留在数据态时，
 * 只复位控制器不够（卡不认新命令），必须重新走一遍枚举。
 * 供 snap_store_remount() 调用；调用前必须已 unmount，且没有打开的文件。
 */
unsigned fh_mmc_recover_count(void)
{
    return g_mmc_recover_total;
}

int fh_mmc_card_reinit(void)
{
    if (g_mmc_host0 == RT_NULL)
    {
        return -1;
    }

    mmcsd_change_local(g_mmc_host0, RT_FALSE);
    rt_thread_mdelay(50);
    mmcsd_change_local(g_mmc_host0, RT_TRUE);
    return 0;
}

static void fh_mmc_recover(struct rt_mmcsd_host *host, struct fh_mmc_obj *mmc_obj)
{
    struct rt_mmcsd_io_cfg cfg = mmc_obj->cur_cfg;
    unsigned now = (unsigned)((rt_tick_get() * 1000u) / RT_TICK_PER_SECOND);

    g_mmc_recover_total++;
    if (g_mmc_recover_last_ms != 0 &&
        (now - g_mmc_recover_last_ms) < MMC_RECOVER_WINDOW_MS)
    {
        g_mmc_recover_burst++;
    }
    else
    {
        g_mmc_recover_burst = 1;
    }
    g_mmc_recover_last_ms = now;

    if (g_mmc_recover_burst >= MMC_RECOVER_BURST_MAX &&
        mmc_obj->sd_hz > MMC_CLK_FLOOR_HZ)
    {
        mmc_obj->sd_hz /= 2;
        if (mmc_obj->sd_hz < MMC_CLK_FLOOR_HZ)
        {
            mmc_obj->sd_hz = MMC_CLK_FLOOR_HZ;
        }

        host->freq_max      = mmc_obj->sd_hz;   /* 之后核层 set_clock 也按这个封顶 */
        host->io_cfg.clock  = mmc_obj->sd_hz;
        cfg.clock           = mmc_obj->sd_hz;
        g_mmc_recover_burst = 0;

        rt_kprintf("[mmc] repeated errors, SD clock -> %u Hz (recover %u)\n",
                   mmc_obj->sd_hz, g_mmc_recover_total);
    }

    /* 让卡把内部编程收尾再复位，否则刚复位就又发命令，卡还没准备好 */
    rt_thread_mdelay(10);

    MMC_Init(mmc_obj);
    fh_mmc_cd_enable(mmc_obj);       /* MMC_Init 会把中断掩码清零 */

    cfg.power_mode = MMCSD_POWER_ON; /* 只恢复时钟/位宽，不再 init 一遍 */
    fh_mmc_set_iocfg(host, &cfg);
}

static void fh_mmc_request(struct rt_mmcsd_host *host, struct rt_mmcsd_req *req)
{
    int ret;
#ifdef RT_USING_PM
    rt_uint32_t pm_state;
#endif
    rt_uint32_t bypass_cmd = 0;
    struct mmc_driver *mmc_drv = host->private_data;
    struct rt_mmcsd_cmd *cmd   = req->cmd;
    struct fh_mmc_obj *mmc_obj = (struct fh_mmc_obj *)mmc_drv->priv;

    PRINT_MMC_DBG("%s start\n", __func__);

    mmc_drv->req = req;
    mmc_drv->cmd = cmd;
    mmc_obj->cur_host = host;
#ifdef RT_USING_PM
    pm_state = fh_mmc_pm_stat_get(mmc_obj);
    if (pm_state == FH_MMC_PM_STAT_SUSPNED)
    {
        cmd->err = RT_EBUSY;
        rt_kprintf("ERROR: %s, data transfer when suspend...\n",__func__);
        mmcsd_req_complete(host);
        return;
    }
#endif

    rt_completion_init(&mmc_drv->transfer_completion);

    ret = fh_mmc_wait_card_idle(mmc_obj);

    if (ret)
    {
        cmd->err = ret;
        rt_kprintf("ERROR: %s, data transfer timeout, status: 0x%x\n", __func__,
                   MMC_GetStatus(mmc_obj));
        fh_mmc_recover(host, mmc_obj);
        mmcsd_req_complete(host);
        return;
    }

    fh_mmc_perpare_data(mmc_drv);

    fh_mmc_send_command(mmc_drv, cmd);
    ret = fh_mmc_get_response(mmc_drv, cmd);
    if (ret)
    {
        cmd->err = ret;
        rt_kprintf("%s,get response returns %d, cmd: %d\n", __func__, ret,
                   cmd->cmd_code);
        /* 响应超时后控制器还停在命令态；不复位的话后续命令会一直超时。
         * 卡可能只是一时忙，复位后由块层重试。*/
        fh_mmc_recover(host, mmc_obj);
        goto out;
    }
    ret = fh_mmc_start_transfer(mmc_drv, &bypass_cmd);
    if (bypass_cmd)
        goto out;

    if (ret)
    {
        /* 小数据传输(<=512B)超时时 start_transfer 的错误码原来被丢掉了，
         * 核层会把这次请求当成成功；控制器也会停在传输状态，之后所有命令
         * 都超时。这里补上错误码并复位控制器。*/
        if (cmd->err == 0)
            cmd->err = ret;

        fh_mmc_recover(host, mmc_obj);
    }

    if (req->stop)
    {
        /* send stop command */
        PRINT_MMC_DBG("%s send stop\n", __func__);
        fh_mmc_send_command(mmc_drv, req->stop);
    }

out:
    fh_mmc_complete_request(mmc_drv);
    PRINT_MMC_DBG("%s end\n", __func__);
}

static const struct rt_mmcsd_host_ops fh_mmc_ops = {
    .request         = fh_mmc_request,
    .set_iocfg       = fh_mmc_set_iocfg,
    .enable_sdio_irq = fh_mmc_enable_sdio_irq,
    .get_card_status = fh_mmc_get_card_status,
#ifdef SD1_MULTI_SLOT
    .set_active_slot = fh_mmc_set_active_slot,
#endif
};

static void fh_mmc_interrupt(int irq, void *param)
{
    struct mmc_driver *mmc_drv = (struct mmc_driver *)param;
    struct fh_mmc_obj *mmc_obj = (struct fh_mmc_obj *)mmc_drv->priv;
/* struct rt_mmcsd_req *req   = mmc_drv->req; */
/* struct rt_mmcsd_cmd *cmd   = mmc_drv->cmd; */
/* struct rt_mmcsd_data *data; */
    rt_uint32_t status;

/* if (cmd && cmd->data) data = cmd->data; */

    status = MMC_GetUnmaskedInterrupt(mmc_obj);
    PRINT_MMC_DBG("unmasked interrupts: 0x%x\n", status);

    if (status & MMC_INT_STATUS_CARD_DETECT)
    {
        rt_uint32_t card_status = MMC_GetCardStatus(mmc_obj);

        if (card_status == CARD_UNPLUGED)
        {
            mmcsd_change_local(mmc_drv->host, RT_FALSE);
            rt_kprintf("card disconnected\n");
        }
        else
        {
            mmcsd_change_local(mmc_drv->host, RT_TRUE);
            rt_kprintf("card connected\n");
        }
    }

    if (status & MMC_INT_STATUS_SDIO)
    {
        rt_completion_done(&mmc_drv->transfer_completion);
    }

    if (status & MMC_INIT_STATUS_DATA_ERROR)
    {
        rt_kprintf("ERROR: %s, data error, status: 0x%x\n", __func__, status);
        mmc_drv->cmd->data->err =  status;
        rt_completion_done(&mmc_drv->transfer_completion);
    }

    if (status & MMC_INT_STATUS_TRANSFER_OVER)
    {
        rt_completion_done(&mmc_drv->transfer_completion);
    }

    if (status & MMC_INT_STATUS_TX_REQUEST)
    {
        fh_mmc_write_pio(mmc_drv);
        rt_completion_done(&mmc_drv->transfer_completion);
    }

    if (status & MMC_INT_STATUS_RX_REQUEST)
    {
        fh_mmc_read_pio(mmc_drv);
        rt_completion_done(&mmc_drv->transfer_completion);
    }

    MMC_ClearRawInterrupt(mmc_obj, MMC_INT_STATUS_ALL);
}

#ifdef CONFIG_GPIO_SD_CD
static void gpio_cd_handler(int irq, void *param)
{
    struct mmc_driver *mmc_drv = (struct mmc_driver *)param;
    struct fh_mmc_obj *mmc_obj = (struct fh_mmc_obj *)mmc_drv->priv;
    rt_uint32_t status;

    rt_uint32_t card_status = MMC_GetCardStatus(mmc_obj);

    if (card_status == CARD_UNPLUGED)
    {
        mmcsd_change_local(mmc_drv->host, RT_FALSE);
        rt_kprintf("card disconnected\n");
    }
    else
    {
        mmcsd_change_local(mmc_drv->host, RT_TRUE);
        rt_kprintf("card connected\n");
    }
}
#endif



#ifdef RT_USING_PM


static int bus_dev_resume(struct fh_mmc_obj *mmc_obj)
{
    struct rt_mmcsd_host *host;
    rt_uint32_t ocr;
    rt_int32_t err;
    rt_int32_t ret = 0;

    host = mmc_obj->cur_host;

    mmcsd_host_lock(host);
    mmcsd_power_up(host);
    mmcsd_go_idle(host);

    mmcsd_send_if_cond(host, host->valid_ocr);
    /*
    * detect SD card
    */
    err = mmcsd_send_app_op_cond(host, 0, &ocr);
    if (!err)
    {
        if (init_sd_hw_only(host, ocr))
        {
            rt_kprintf("%s :: init sd card failed ....\n",__func__);
            ret = -1;
        }
        mmcsd_host_unlock(host);
        return ret;
    }
    /*
    * detect mmc card
    */
    err = mmc_send_op_cond(host, 0, &ocr);
    if (!err)
    {
        if (init_mmc_hw_only(host, ocr))
        {
            rt_kprintf("%s :: init emmc failed ....\n",__func__);
            ret = -2;
        }
        mmcsd_host_unlock(host);
        return ret;
    }

    mmcsd_host_unlock(host);
    return ret;
}




static int pm_suspend(const struct rt_device *device, rt_uint8_t mode)
{
    struct fh_mmc_obj *mmc_obj;
    int hw_stat;
    int wait_timeout = PM_MMC_WAIT_MAX_TIME;

    mmc_obj = rt_container_of(device, struct fh_mmc_obj, parent);
    if (!mmc_obj)
        return RT_ERROR;
    /*first set pm state...to ignore any new request..*/
    fh_mmc_pm_stat_set(mmc_obj, FH_MMC_PM_STAT_SUSPNED);
    do
    {
        hw_stat = fh_mmc_get_ctrl_busy_state(mmc_obj);
        if (hw_stat == MMC_CTRL_IDLE)
            break;
        else
        {
            fh_udelay(1);
            wait_timeout--;
        }
    } while (wait_timeout);

    return RT_EOK;
}

static void pm_resume(const struct rt_device *device, rt_uint8_t mode)
{
    struct fh_mmc_obj *mmc_obj;
    rt_uint32_t status;
    rt_int32_t ret;

    mmc_obj = rt_container_of(device, struct fh_mmc_obj, parent);
    if (!mmc_obj)
        return;
    /*wake up pre request..*/
    if (!(rt_list_isempty(&mmc_obj->cur_driver->transfer_completion.suspended_list)))
        rt_completion_done(&mmc_obj->cur_driver->transfer_completion);
    /*here will do hw reset...*/
    MMC_Init(mmc_obj);

    fh_mmc_pm_stat_set(mmc_obj, FH_MMC_PM_STAT_NORMAL);
    if (mmc_obj->cur_host)
        ret = bus_dev_resume(mmc_obj);
    if (ret)
    {
        if (mmc_obj->cur_host)
        {
            status = MMC_GetCardStatus(mmc_obj);
            if (status == CARD_PLUGED)
                mmcsd_change_local(mmc_obj->cur_host, RT_TRUE);
        }
    }

}

struct rt_device_pm_ops plat_mmc_pm_ops = {
    .suspend_prepare = pm_suspend,
    .resume_prepare = pm_resume
};
#endif

int fh_mmc_probe(void *priv_data)
{
    struct mmc_driver *mmc_drv;
    struct rt_mmcsd_host *host;
    struct fh_mmc_obj *mmc_obj = (struct fh_mmc_obj *)priv_data;
    rt_uint32_t status;

    PRINT_MMC_DBG("%s start\n", __func__);

    mmc_drv = (struct mmc_driver *)rt_malloc(sizeof(struct mmc_driver));
    rt_memset(mmc_drv, 0, sizeof(struct mmc_driver));
    mmc_drv->priv = mmc_obj;

    host = mmcsd_alloc_host();
    if (!host)
    {
        rt_kprintf("ERROR: %s, failed to malloc host\n", __func__);
        return -RT_ENOMEM;
    }
    mmc_obj->cur_driver = mmc_drv;
    mmc_obj->descriptors =
        (MMC_DMA_Descriptors *)fh_dma_mem_malloc_align(4096, 128, "MMC-DESC");
#ifdef MMC_USE_INTERNAL_BUF
    mmc_obj->dma_buf = (rt_uint32_t *)fh_dma_mem_malloc_align(
        MMC_INTERNAL_DMA_BUF_SIZE, 32, "MMC-DMA-BUF");

    if (!mmc_obj->dma_buf)
    {
        rt_kprintf("ERROR: %s, failed to malloc mmc dma buffer\n", __func__);
        return -RT_ENOMEM;
    }
#endif
    if (!mmc_obj->descriptors)
    {
        rt_kprintf("ERROR: %s, failed to malloc dma descriptors\n", __func__);
        return -RT_ENOMEM;
    }

    rt_memset(mmc_obj->descriptors, 0, 4096);
    mmc_drv->max_desc = 4096 / (sizeof(MMC_DMA_Descriptors));
    rt_completion_init(&mmc_drv->transfer_completion);
    host->ops       = &fh_mmc_ops;
    host->freq_min  = MMC_FEQ_MIN;
    host->freq_max  = mmc_obj->sd_hz;
    host->valid_ocr = VDD_32_33 | VDD_33_34;

    host->flags         = MMCSD_MUTBLKWRITE | MMCSD_SUP_HIGHSPEED | MMCSD_SUP_SDIO_IRQ | mmc_obj->sd_bus_width;
    host->max_seg_size  = MMC_DMA_DESC_BUFF_SIZE;
    host->max_dma_segs  = mmc_drv->max_desc;
    host->max_blk_size  = 512;
    host->max_blk_count = MMC_INTERNAL_DMA_BUF_SIZE / host->max_blk_size;
    host->private_data  = mmc_drv;
    host->id     = mmc_obj->id;

    mmc_drv->host = host;

    if (mmc_obj->id == 0)
    {
        g_mmc_host0 = host;
    }

    if (mmc_obj->power_pin_gpio >= 0)
    {
        fh_select_gpio(mmc_obj->power_pin_gpio);
        gpio_request(mmc_obj->power_pin_gpio);
        gpio_direction_output(mmc_obj->power_pin_gpio, 0);
    }
#ifdef CONFIG_GPIO_SD_CD
    if (mmc_obj->cd_gpio >=0)
    {
        fh_select_gpio(mmc_obj->cd_gpio); /* just for compatiblity with fh81 */
        fh_gpio_register_interrupt(mmc_obj->cd_gpio, IRQ_TYPE_EDGE_BOTH,
                                    gpio_cd_handler, (void *)mmc_drv, "mmc0_cd");
    }
#endif
    MMC_Init(mmc_obj);
#ifdef RT_USING_PM
    if (mmc_obj->id == 0)
        rt_device_register(&(mmc_obj->parent), "mmc0", RT_IPC_FLAG_FIFO);
    else
        rt_device_register(&(mmc_obj->parent), "mmc1", RT_IPC_FLAG_FIFO);

    fh_mmc_pm_stat_set(mmc_obj, FH_MMC_PM_STAT_NORMAL);
    rt_pm_device_register(&(mmc_obj->parent), &plat_mmc_pm_ops);
#endif
    if (mmc_obj->id == 0)
    {
        rt_hw_interrupt_install(mmc_obj->irq, fh_mmc_interrupt, (void *)mmc_drv, "mmc_isr_0");
    }
    else if (mmc_obj->id == 1)
    {
        rt_hw_interrupt_install(mmc_obj->irq, fh_mmc_interrupt, (void *)mmc_drv, "mmc_isr_1");
#ifdef SD1_MULTI_SLOT
        if (mmc_obj->slotscnt > 1)
        {
            int i = 0;

            for (i = 0; i < mmc_obj->slotscnt; i++)
            {
                if (mmc_obj->mmc_slot_selections[i].vdd_gpio_idx > 0)
                {
                    gpio_request(mmc_obj->mmc_slot_selections[i].vdd_gpio_idx);
                }
            }
            mmc_drv->slotid = 1;

            for (i = 0; i < mmc_obj->slotscnt; i++)
            {
                if (mmc_obj->mmc_slot_selections[i].vdd_gpio_idx > 0 &&
                    mmc_obj->mmc_slot_selections[i].slotid != mmc_drv->slotid)
                {
                    gpio_direction_output(mmc_obj->mmc_slot_selections[i].vdd_gpio_idx, 1);
                }
            }

            if (mmc_obj->mmc_switch_slot != NULL)
            {
                mmc_obj->mmc_switch_slot(1);
            }

            for (i = 0; i < mmc_obj->slotscnt; i++)
            {
                if (mmc_obj->mmc_slot_selections[i].vdd_gpio_idx > 0 &&
                    mmc_obj->mmc_slot_selections[i].slotid == mmc_drv->slotid)
                {
                    gpio_direction_output(mmc_obj->mmc_slot_selections[i].vdd_gpio_idx, 0);
                }
            }
            mmcsd_init_ctl(host);
        }
#endif
    }
    rt_hw_interrupt_umask(mmc_obj->irq);
    status = MMC_GetCardStatus(mmc_obj);

    if (status == CARD_PLUGED)
        mmcsd_change_local(host, RT_TRUE);

    fh_mmc_cd_enable(mmc_obj);

    PRINT_MMC_DBG("%s end\n", __func__);

    return 0;
}

int fh_mmc_exit(void *priv_data) { return 0; }
struct fh_board_ops mmc_driver_ops = {
    .probe = fh_mmc_probe, .exit = fh_mmc_exit,
};

void rt_hw_mmc_init(void)
{
    PRINT_MMC_DBG("%s start\n", __func__);
    fh_board_driver_register("mmc", &mmc_driver_ops);
    PRINT_MMC_DBG("%s end\n", __func__);
}
