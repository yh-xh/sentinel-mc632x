#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include "osd_main.h"
#include "osd_web_key.h"
#include "top/osd_event.h"
#include "config.h"
#include "core/osd_manager.h"
#include "core/osd_screen.h"
#include "mod/user_interface.h"
#include "mod/user_menu.h"
#include "top/dirty_interface.h"
#include "dsp_ext/FHAdv_OSD_mpi.h"
#include "mod/user_string.h"
#include "core/osd_draw.h"

static int osd_exit = 0;
static int osd_started = 0;
void *osd_config(void *arg);
int osd_menu_draw(void);

OSD_MENU_PARA_t g_osd_para = {
    .osd_group_id   = OSD_DEF_GROUP_ID,
    .osd_channel_id = OSD_DEF_CHANNEL,
    .osd_font_size  = OSD_DEF_FONT_SIZE,
    .row_space      = OSD_DEF_RAW_SPACE,
    .pos_offset_x   = OSD_DEF_POS_OFFSET_X,
    .pos_offset_y   = OSD_DEF_POS_OFFSET_Y,
    .font_color     = OSD_DEF_FONT_COLOR,
    .edge_color     = OSD_DEF_EDGE_COLOR,
    .layerMaxWidth  = OSD_DEF_MAX_LAYER_W,
    .layerMaxHeight = OSD_DEF_MAX_LAYER_H,
};

void menu_set_para(OSD_MENU_PARA_t *osd_para)
{
    if (!osd_para)
        return;

    if (osd_para->osd_font_size)
        g_osd_para.osd_font_size = osd_para->osd_font_size;
    if (osd_para->pos_offset_x)
        g_osd_para.pos_offset_x = osd_para->pos_offset_x;
    if (osd_para->pos_offset_y)
        g_osd_para.pos_offset_y = osd_para->pos_offset_y;
    if (osd_para->font_color)
        g_osd_para.font_color = osd_para->font_color;
    if (osd_para->edge_color)
        g_osd_para.edge_color = osd_para->edge_color;
    if (osd_para->layerMaxWidth)
        g_osd_para.layerMaxWidth = osd_para->layerMaxWidth;
    if (osd_para->layerMaxHeight)
        g_osd_para.layerMaxHeight = osd_para->layerMaxHeight;

    g_osd_para.row_space = osd_para->row_space;
    g_osd_para.osd_group_id = osd_para->osd_group_id;
    g_osd_para.osd_channel_id = osd_para->osd_channel_id;
}

void osd_menu_init(OSD_MENU_PARA_t *osd_para)
{

    menu_set_para(osd_para);
    // 屏幕
    screen_initilize();
    screen_clean();
    // 引擎
    manager_initialize();
    // 用户菜单初始化
    init_defaultConf();
    initializeUserMenu();
    // 初始菜单显示
    manager_setRootMenu(getInitMenu());
    manager_drawMenu();

    if (osd_started == 0)
    {
        /* 按键输入源：网页遥控（osd_web_key.c）。
         * 原先这里起的是 osd_config() 线程 —— 它靠每秒轮询 tosd_event.cfg 文件收按键，
         * 但本工程的 msh 没有 echo/vi/ymodem，写不进这个文件；而且它与 osd_menu 核心
         * 代码（全程无锁）并发会出竞态。改由网页线程独占调用，见 osd_web_key.c 注释。 */
        osd_web_key_start();
        osd_started = 1;
    }
}

void osd_menu_paracfg(OSD_MENU_PARA_t *osd_para)
{
    menu_set_para(osd_para);
    manager_setRefresh(1);
    osd_menu_handle(EVENT_NO);
}

void osd_menu_exit(void)
{
    // 屏幕
    osd_exit = 1;
    osd_started = 0;
}

void osd_menu_handle(int event)
{
    /* printf("event:%d\n",event); */
    if (event == EVENT_NO) {
        if (manager_needsRefresh()){
            manager_drawMenu();
            manager_setRefresh(0);
        }
    }

    else if (event == EVENT_TIMEOUT) {
        manager_setRootMenu(getSleepMenu());
        manager_doOperation(event);
        manager_drawMenu();
    }

    else {
        manager_doOperation(event);
        manager_drawMenu();
    }
}

static FHT_OSD_TextLine_t text_line_cfg[MAX_COMPONENT_PER_NUM];
static FH_CHAR string_buf[MAX_COMPONENT_PER_NUM][MAX_CHAR_NUM_PER_OSD];
static FH_UINT32 string_id = 0;
static FH_UINT32 layer_start_x = 0;
static FH_UINT32 layer_start_y = 0;

int osd_menu_config(FH_UINT32 x, FH_UINT32 y, FH_UINT32 id, FH_CHAR *string)
{
    if (strlen(string) > MAX_CHAR_NUM_PER_OSD)
    {
        printf("osd_menu_config error: strlen(string) > MAX_STR_LEN!\n");
        return 0;
    }
    if ((id & 0xff) == 0)
    {
        // printf("osd_menu_draw clean\n");
        string_id = 0;
        memset(&text_line_cfg[0], 0, sizeof(FHT_OSD_TextLine_t) * MAX_COMPONENT_PER_NUM);
        memset(&string_buf[0][0], 0, sizeof(string_buf));
    }
    strcpy(string_buf[string_id], string);
    text_line_cfg[string_id].textInfo = string_buf[string_id];
    text_line_cfg[string_id].textEnable    = 1;             /* 使能自定义text */
    text_line_cfg[string_id].timeOsdEnable = 0;             /* 使能时间显示 */
    text_line_cfg[string_id].textLineWidth = g_osd_para.osd_font_size * 36;/* 每行最多显示36个像素宽度为16的字符, 超过后自动换行，对于TVI菜单场景这里应该大一些 */
    text_line_cfg[string_id].linePositionX = g_osd_para.pos_offset_x + x;                          /* 左上角起始点水平方向像素位置 */
    text_line_cfg[string_id].linePositionY = g_osd_para.pos_offset_y + y;                          /* 左上角起始点垂直方向像素位置 */
    if (string_id == 0)
    {
        layer_start_x = text_line_cfg[0].linePositionX;
        layer_start_y = text_line_cfg[0].linePositionY;
    }
    text_line_cfg[string_id].lineId = id;
    text_line_cfg[string_id].enable = 1;
    string_id++;
    printf("string_id %d strlen(string) %d id %x layer_start_x %d layer_start_y %d\n", string_id, strlen(string), id, layer_start_x, layer_start_y);
    return 0;
}

int osd_menu_draw(void)
{
    FH_SINT32 ret = 0;
    FHT_OSD_CONFIG_t osd_cfg;
    FHT_OSD_Layer_Config_t  pOsdLayerInfo[1];
    FHT_OSD_Ex_TextLine_t TextCfg;
    // printf("osd_menu_draw\n");
    memset(&osd_cfg, 0, sizeof(osd_cfg));
    memset(&pOsdLayerInfo[0], 0, sizeof(FHT_OSD_Layer_Config_t));
    memset(&TextCfg, 0, sizeof(FHT_OSD_Ex_TextLine_t));
    /* 不旋转 */
    osd_cfg.osdRotate        = 0;
    osd_cfg.pOsdLayerInfo = &pOsdLayerInfo[0];
    /* 设置layer个数 */
    osd_cfg.nOsdLayerNum     = 1;

    pOsdLayerInfo[0].layerStartX = layer_start_x;
    pOsdLayerInfo[0].layerStartY = layer_start_y;
    /* 设置字符大小,像素单位 */
    pOsdLayerInfo[0].osdSize     = g_osd_para.osd_font_size;/* 这里应该从你那获取字体大小 */

    /* 设置字符颜色为白色 */
    pOsdLayerInfo[0].normalColor.fAlpha = (g_osd_para.font_color >> 24) & 0xff;
    pOsdLayerInfo[0].normalColor.fRed   = (g_osd_para.font_color >> 16) & 0xff;
    pOsdLayerInfo[0].normalColor.fGreen = (g_osd_para.font_color >> 8) & 0xff;
    pOsdLayerInfo[0].normalColor.fBlue  = (g_osd_para.font_color) & 0xff;

    /* 设置字符反色颜色为黑色 */
    pOsdLayerInfo[0].invertColor.fAlpha = 255;/* 这里无用 */
    pOsdLayerInfo[0].invertColor.fRed   = 0;
    pOsdLayerInfo[0].invertColor.fGreen = 0;
    pOsdLayerInfo[0].invertColor.fBlue  = 0;

    /* 设置字符钩边颜色为黑色 */
    pOsdLayerInfo[0].edgeColor.fAlpha = (g_osd_para.edge_color >> 24) & 0xff;
    pOsdLayerInfo[0].edgeColor.fRed   = (g_osd_para.edge_color >> 16) & 0xff;
    pOsdLayerInfo[0].edgeColor.fGreen = (g_osd_para.edge_color >> 8) & 0xff;
    pOsdLayerInfo[0].edgeColor.fBlue  = (g_osd_para.edge_color) & 0xff;

    pOsdLayerInfo[0].layerMaxWidth = g_osd_para.layerMaxWidth;	/* 这里是配置画布的大小 */
    pOsdLayerInfo[0].layerMaxHeight = g_osd_para.layerMaxHeight;
    pOsdLayerInfo[0].bkgColor.fAlpha = 0;

    pOsdLayerInfo[0].layerId = OSD_MENU_LAYER_ID;
    /* 钩边像素为1 */
    pOsdLayerInfo[0].edgePixel        = 1;/* 0是不钩边 */

    /* 反色控制 */
    pOsdLayerInfo[0].osdInvertEnable  = FH_OSD_INVERT_DISABLE; /*disable反色功能*/
    pOsdLayerInfo[0].layerFlag = FH_OSD_LAYER_USE_TWO_BUF;

    FH_UINT32 vpuid = g_osd_para.osd_group_id;/* 应由上层宏或变量控制 */
    FH_UINT32 channel = g_osd_para.osd_channel_id;/* 应由上层宏或变量控制 */
    ret = FHAdv_Osd_Ex_SetText(vpuid, channel, &osd_cfg);
    if (ret != FH_SUCCESS)
    {
        printf("FHAdv_Osd_Ex_SetText failed with %x\n", ret);
        return ret;
    }

    TextCfg.LineNum = string_id;
    TextCfg.Reconfig = 1;
    TextCfg.pLineCfg = &text_line_cfg[0];
    ret = FHAdv_Osd_Ex_SetTextLine(vpuid, channel, pOsdLayerInfo[0].layerId, &TextCfg);
    if (ret != FH_SUCCESS)
    {
        printf("FHAdv_Osd_Ex_SetTextLine failed with %x\n", ret);
        return ret;
    }

    return ret;
}

int osd_menu_clean(FH_UINT32 id)
{
    FH_SINT32 ret = 0;
    FHT_OSD_Ex_TextLine_t TextCfg;
    FHT_OSD_TextLine_t text_line_cfg[1];
    FH_UINT32 vpuid = g_osd_para.osd_group_id;/* 应由上层宏或变量控制 */
    FH_UINT32 channel = g_osd_para.osd_channel_id;/* 应由上层宏或变量控制 */

    if (id == 255)  /* 作用是osd_menu_clean函数不再剩下，仅当id = 255时对TOSD进行清屏 */
    {
        memset(&text_line_cfg[0], 0, sizeof(FHT_OSD_TextLine_t));

        text_line_cfg->lineId = 0;
        text_line_cfg->enable = 0;

        TextCfg.LineNum = 0;
        TextCfg.Reconfig = 1;
        TextCfg.pLineCfg = &text_line_cfg[0];
        ret = FHAdv_Osd_Ex_SetTextLine(vpuid, channel, OSD_MENU_LAYER_ID, &TextCfg);
        if (ret != FH_SUCCESS)
        {
            printf("FHAdv_Osd_Ex_SetTextLine failed with %x\n", ret);
            return ret;
        }
    }
    return ret;
}

#define OSD_TIMEOUT_TIMER 1000000
#define OSD_EVENT_FILE "tosd_event.cfg"
void *osd_config(void *arg)
{
    FILE *ftosd_event;
    char string_event[16];
    int length, no_event_cnt = 0;
    while(!osd_exit)
    {
        memset(string_event, 0, 16);
        ftosd_event = fopen(OSD_EVENT_FILE, "rb");
        if (!ftosd_event)
        {
            sleep(1);
            printf("[Error]: Open File OSD_EVENT_FILE fail \n");
            continue;
        }
        fseek(ftosd_event, 0L, SEEK_END);
        length = ftell(ftosd_event);
        fseek(ftosd_event, 0L, SEEK_SET);
        fread(string_event, 1, length-1, ftosd_event);
        fclose(ftosd_event);
        if(length == 0){
            ;
        }else{
            no_event_cnt = 0;
            ftosd_event = fopen(OSD_EVENT_FILE, "wb");
            if (!ftosd_event)
            {
                printf("[Error]: Open File OSD_EVENT_FILE fail \n");
                sleep(1);
                continue;
            }
            fclose(ftosd_event);
        }

        if(!strcmp(string_event, "ENTER")){
            osd_menu_handle(EVENT_ENTER_CLICK);
            // printf("EVENT_ENTER\n");
        }else if(!strcmp(string_event, "UP")){
            osd_menu_handle(EVENT_UP_CLICK);
            // printf("EVENT_UP\n");
        }else if(!strcmp(string_event, "DOWN")){
            osd_menu_handle(EVENT_DOWN_CLICK);
            // printf("EVENT_DOWN\n");
        }else if(!strcmp(string_event, "LEFT")){
            osd_menu_handle(EVENT_LEFT_CLICK);
            // printf("EVENT_LEFT\n");
        }else if(!strcmp(string_event, "RIGHT")){
            osd_menu_handle(EVENT_RIGHT_CLICK);
            // printf("EVENT_RIGHT\n");
        }else if(no_event_cnt > OSD_TIMEOUT_TIMER){
            osd_menu_handle(EVENT_TIMEOUT);
            // printf("EVENT_TIMEOUT\n");
            no_event_cnt = 0;
        }else{
            osd_menu_handle(EVENT_NO);
            // printf("EVENT_NO\n");
            no_event_cnt++;
        }
        sleep(1);
    }
    osd_exit = 0;
    return NULL;
}
