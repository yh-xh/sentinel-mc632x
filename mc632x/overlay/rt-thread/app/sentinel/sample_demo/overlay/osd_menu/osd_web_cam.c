/*
 * osd_web_cam.c —— LCD 画面源切换的 HTTP 接口
 *
 * 薄薄一层，真正的换绑在 cam_switch.c。放在这里是为了让 HTTP 层
 * 和"怎么切"这件事分开：以后要换成别的切换方式，只改 cam_switch.c。
 */
#include <stdio.h>
#include <string.h>

#include "osd_web_key.h"
#include "osd_web_cam.h"
#include "cam_switch.h"

static void cam_api(int fd, const char *query)
{
    char body[48];
    int  m;

    if (query && query[0])
    {
        m = osd_web_query_get_int(query, "m", -1);
        if (m == CAM_MODE_G0 || m == CAM_MODE_G1)
        {
            /* 切换失败不单独回错误码：下面统一回实际模式，
             * 前端据此把按钮高亮对齐到真实状态。*/
            cam_switch_set(m);
        }
    }

    snprintf(body, sizeof(body), "mode=%d\n", cam_switch_get());
    osd_web_send(fd, "200 OK", "text/plain; charset=utf-8", body);
}

int osd_web_cam_handle(int fd, const char *path, const char *query)
{
    if (!strcmp(path, "/api/cam"))
    {
        cam_api(fd, query);
        return 1;
    }

    return 0;
}
