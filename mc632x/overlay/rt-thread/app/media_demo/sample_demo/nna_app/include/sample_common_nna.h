#ifndef __SAMPLE_COMMON_NNA_H__
#define __SAMPLE_COMMON_NNA_H__
#include "vpu_config.h"
#include "vpu_chn_config.h"
#include "enc_config.h"
#include "overlay/include/overlay_gbox.h"

#if defined FH_NN_ENABLE || defined FH_NN_AINR_ENABLE
#include "nna_config.h"

#ifndef FH_NN_DET_FPS
#define FH_NN_DET_FPS 10
#endif

#define BOX_MAX_NUM 50

FH_SINT32 sample_fh_nna_obj_detect_start(FH_SINT32 grp_id);
FH_SINT32 sample_fh_nna_obj_detect_stop(FH_SINT32 grp_id);

FH_SINT32 sample_fh_nna_ivs_start(FH_SINT32 grp_id);
FH_SINT32 sample_fh_nna_ivs_stop(FH_SINT32 grp_id);

/* AI 叠加层总开关：0 = 隐藏绊线/周界/人形框，检测和事件继续。
 * 只写 volatile 标志，真正的 GBOX 操作在 NNA 线程里完成。*/
void sample_nna_overlay_set(int on);
int  sample_nna_overlay_get(void);

/* 最近一帧检测到的人形数量；NNA 没跑时返回 0。*/
int  sample_nna_person_count(void);

/* 最近一帧所有目标里的最大置信度，0-100；没有目标返回 0。*/
int  sample_nna_last_conf(void);

FH_SINT32 sample_fh_nna_gesture_rec_start(FH_SINT32 grp_id);
FH_SINT32 sample_fh_nna_gesture_rec_stop(FH_SINT32 grp_id);

FH_SINT32 sample_fh_nna_face_snap_start(FH_SINT32 grp_id);
FH_SINT32 sample_fh_nna_face_snap_stop(FH_SINT32 grp_id);

FH_SINT32 sample_fh_nna_face_rec_start(FH_SINT32 grp_id);
FH_SINT32 sample_fh_nna_face_rec_stop(FH_SINT32 grp_id);

FH_SINT32 sample_fh_nna_lprec_start(FH_SINT32 grp_id);
FH_SINT32 sample_fh_nna_lprec_stop(FH_SINT32 grp_id);

FH_SINT32 sample_fh_ainr_start(FH_SINT32 grp_id);
FH_SINT32 sample_fh_ainr_stop(FH_SINT32 grp_id);
#endif

#endif // __SAMPLE_COMMON_NNA_H__
