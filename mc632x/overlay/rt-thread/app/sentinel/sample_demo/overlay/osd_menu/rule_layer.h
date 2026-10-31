#ifndef _RULE_LAYER_H_
#define _RULE_LAYER_H_

/*
 * LCD 专用规则线图层（VOU graphic layer）。
 *
 * 绊线/周界框只显示在 LCD 上，不进编码流，所以事件视频里没有它们；
 * 网页实时预览的规则线由看板页面自己用 CSS 画。
 *
 * 初始化必须放在 _vlcview() 之后；失败只打印，不影响视频和网页。
 */
int  rule_layer_init(void);

/* 跟随网页"显示叠加"开关 */
void rule_layer_set_overlay(int on);

/* 跟随 LCD 画面源：接 G1（AI 主摄）才显示，接 G0 原始画面时隐藏 */
void rule_layer_set_cam(int g1);

#endif /* _RULE_LAYER_H_ */
