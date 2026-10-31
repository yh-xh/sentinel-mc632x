#ifndef _CAM_SWITCH_H_
#define _CAM_SWITCH_H_

/*
 * LCD 画面源切换。
 *
 * VOU 的通道 0 只能接一路视频源，所以"切换摄像头"本质是给 VOU 换绑输入：
 *
 *   模式 0 (默认): G1 chn1 (800x480)   → VOU → LCD   带 OSD 叠加（人形框/绊线/周界）
 *   模式 1:        G0 chn0 (1920x1088)  → VOU → LCD   原始画面，无 OSD
 *
 * 注意：只能用 FH_SYS_UnBindbyDst() 解 VOU 这一个下游。
 * 千万不要用 unbind_vpu_chn() —— 它走 UnBindbySrc，会把该通道的
 * H.264 编码器也一起解掉，RTSP 流就断了。
 *
 * 调用约定：网页、串口 msh、网页终端都可以调用；cam_switch.c 内部用
 * pthread mutex 把 VOU 换绑串行化，不要在 NNA / ISP 线程里直接操作 VOU。
 */

#define CAM_MODE_G1  0   /* G1 chn1 800x480，带 OSD */
#define CAM_MODE_G0  1   /* G0 chn0 1920x1088，原始 */

/* 当前模式，返回 CAM_MODE_* */
int cam_switch_get(void);

/* 执行切换。返回 0 成功；-1 失败（失败时模式回滚到 G1，屏幕不会留在黑屏） */
int cam_switch_set(int mode);

#endif /* _CAM_SWITCH_H_ */
