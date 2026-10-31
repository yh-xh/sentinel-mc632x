#ifndef __SAMPLE_OVERLAY_H__
#define __SAMPLE_OVERLAY_H__
#include "sample_common_media.h"
#include "dsp_ext/FHAdv_OSD_mpi.h"
#include "overlay/include/overlay_gbox.h"
#include "overlay/include/overlay_logo.h"
#include "overlay/include/overlay_mask.h"
#include "overlay/include/overlay_tosd.h"

#ifdef FH_APP_OVERLAY_TOSD_MENU
#include "overlay/osd_menu/osd_main.h"
#endif

FH_SINT32 sample_fh_overlay_start(FH_SINT32 grp_id);

FH_SINT32 sample_fh_overlay_stop(FH_SINT32 grp_id);

#endif /*__SAMPLE_OVERLAY_H__*/
