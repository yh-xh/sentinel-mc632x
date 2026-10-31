#ifndef __NNA_CONFIG_H__
#define __NNA_CONFIG_H__
#include "sample_common_media.h"
#include "dsp/fh_nna_mpi.h"
#include "fh_nnprimary_det_post_mpi.h"

#if defined(FH_FACE_SNAP_ENABLE) || defined(FH_FACE_REC_ENABLE)
#include "fh_nnfacedet_full_post_mpi.h"
#include "fha_face_quality_mpi.h"
#endif

#if defined(FH_FACE_REC_ENABLE)
#include "fha_face_align_mpi.h"
#include "fh_nnfacerec_post_mpi.h"
#include "fha_face_rec_mpi.h"
#endif

#if defined(FH_GESTURE_REC_ENABLE)
#include "fh_nngesturedet_post_mpi.h"
#include "fh_nngesturerec_post_mpi.h"
#include "fha_imgcrop_mpi.h"
#endif

#if defined(FH_LP_REC_ENABLE)
#include "fh_nnlpdet_full_post_mpi.h"
#include "fh_nnlprec_post_mpi.h"
#include "fha_plate_rectify_mpi.h"
#endif

#define NN_RESULT_NUM 50
#define NN_MAX_CHN 8
#define LANDMARK_PTS_NUM 5
#define FH_REC_EMBEDDING_LEN 128

#define FH_LP_LANDMARK_PTS_NUM 4
#define FH_LP_LENGTH 8

#define NN_DEBUG 0

typedef FH_VOID *NN_HANDLE;

typedef enum
{
    PERSON_DET = 0,          // 人型检测 | [ ]
    PERSON_VEHICLE_DET,      // 人车检测 | [ ]
    PERSON_VEHICLE_FIRE_DET, // 人车火检测 | [ ]
    PERSON_KPT_DET,          // 人型5点关键点检测 | [ ]
    FACE_DET,                // 人脸检测 | [ ]
    FACE_KPT_DET,            // 人脸5点关键点检测 | [ ]
    FACE_68_KPT_DET,         // 人脸68点关键点检测 | [ ]
    FACE_REC,                // 人脸识别 | [ ]
    FACE_ATTR,               // 人脸属性 | [ ]
    LP_DET,                  // 车牌检测 | [ ]
    LP_REC,                  // 车牌识别 | [ ]
    FIRE_DET,                // 火焰检测 | [ ]
    PET_DET,                 // 宠物检测 | [ ]
    LIVE_DET,                // 活体检测 | [ ]
    FIVE_IN_ONE,             // 检测5合一 | [ ]
    GES_DET,                 // 手势检测
    GES_REC,                 // 手势识别
    MODEL_MAX,               // max
} NN_TYPE;

typedef struct
{
    FH_FLOAT x; // 矩形框x坐标 | [0.0 - 1.0]
    FH_FLOAT y; // 矩形框y坐标 | [0.0 - 1.0]
    FH_FLOAT w; // 矩形框宽度　| [0.0 - 1.0]
    FH_FLOAT h; // 矩形框高度　| [0.0 - 1.0]
} NN_RECT;

typedef struct
{
    FH_FLOAT x;
    FH_FLOAT y;
} NN_POINT;

typedef struct
{
    FH_UINT16 class_id; // 类别索引 | [ ]
    NN_RECT box;        // 矩形信息 | [ ]
    FH_FLOAT conf;      // 置信度 | [0.0 - 1.0]
    NN_POINT landmark[FH_LP_LANDMARK_PTS_NUM];
    FH_FLOAT occlude[FH_LP_LANDMARK_PTS_NUM];
    FH_FLOAT quality;
} NN_RESULT_LP_DET_INFO;

typedef struct
{
    FH_UINT64 frame_id;                             // 矩形信息对应的帧序 | [ ]
    FH_UINT64 time_stamp;                           // 矩形信息对应帧的时间戳 | [ ]
    FH_UINT32 boxNum;                               // 矩形个数 | [ ]
    NN_RESULT_LP_DET_INFO detBBox[FH_MAX_BBOX_NUM]; // 具体矩形信息 | [ ]
} NN_RESULT_LP_DET;

typedef struct
{
    FH_UINT64 frame_id;              // 信息对应的帧序 | [ ]
    FH_UINT64 time_stamp;            // 信息对应帧的时间戳 | [ ]
    FH_UINT8 val_dec[FH_LP_LENGTH];  // decoded license-plate-char idx,[0-82]
    FH_FLOAT prob_dec[FH_LP_LENGTH]; // probability for each decoded char,[0.0-1.0]
    FH_UINT8 plate_len;              // decoded license-plate-char length,[0-8]
} NN_RESULT_LP_REC;

typedef enum
{
    GESTURE_UNDEFINED = 0,
    GESTURE_FIST = 1,
    GESTURE_FOUR = 2,
    GESTURE_OK = 3,
    GESTURE_PEACE = 4,
    GESTURE_STOP = 5,
} GESTURE_TYPE_INFO;

typedef struct
{
    FH_UINT64 frame_id;   // 矩形信息对应的帧序 | [ ]
    FH_UINT64 time_stamp; // 矩形信息对应帧的时间戳
    FH_FLOAT conf;        // probability for each decoded guesture,[0.0-1.0]
    GESTURE_TYPE_INFO type_info;
} NN_RESULT_GESTURE_REC;

typedef struct
{
    FH_UINT16 class_id; // 类别索引 | [ ]
    FH_RECT_T box;      // 矩形信息 | [ ]
    FH_FLOAT conf;      // 置信度 | [0.0 - 1.0]
} NN_RESULT_GESTURE_DET_INFO;

typedef struct
{
    FH_UINT64 frame_id;                                       // 矩形信息对应的帧序 | [ ]
    FH_UINT64 time_stamp;                                     // 矩形信息对应帧的时间戳 | [ ]
    FH_UINT32 nn_result_num;                                  // 矩形个数 | [ ]
    NN_RESULT_GESTURE_DET_INFO nn_result_info[NN_RESULT_NUM]; // 具体矩形信息 | [ ]
} NN_RESULT_GESTURE_DET;

typedef struct
{
    FH_UINT64 frame_id;   // 矩形信息对应的帧序 | [ ]
    FH_UINT64 time_stamp; // 矩形信息对应帧的时间戳 | [ ]

    FH_FLOAT scale;
    FH_SINT32 zero_point;               // int32_t
    FH_UINT8 val[FH_REC_EMBEDDING_LEN]; // uint8_t
    FH_UINT32 length;                   // uint32_t
} NN_RESULT_FACE_REC;                   // embedding_t;

typedef struct nn_result_face_snap_info
{
    FH_UINT16 class_id; // 类别索引 | [ ]
    NN_RECT box;        // 矩形信息 | [ ]
    FH_FLOAT conf;      // 置信度 | [0.0 - 1.0]
    NN_POINT landmark[LANDMARK_PTS_NUM];
    FH_FLOAT occlude[LANDMARK_PTS_NUM];
    FH_FLOAT quality;
} NN_RESULT_FACE_SNAP_INFO;

typedef struct nn_result_face_snap
{
    FH_SINT32 nn_result_num;
    NN_RESULT_FACE_SNAP_INFO nn_result_info[NN_RESULT_NUM];
} NN_RESULT_FACE_SNAP;

typedef struct nn_result_info
{
    FH_UINT32 class_id; // 类别索引　| [ ]
    NN_RECT box;        // 矩形信息 w,h为0时，则为一个点 | [ ]
    FH_FLOAT conf;      // 置信度　　| [0.0 - 1.0]
} NN_RESULT_DET_INFO;

typedef struct nn_result_det
{
    FH_SINT32 nn_result_num;
    NN_RESULT_DET_INFO nn_result_info[NN_RESULT_NUM];
} NN_RESULT_DET;

typedef struct nn_result
{
    NN_TYPE net_type;
    union
    {
        NN_RESULT_DET result_det;                 // 检测结果 | [ ]
        NN_RESULT_FACE_SNAP result_face_snap;     // 抓拍结果 | [ ]
        NN_RESULT_FACE_REC result_face_rec;       // 识别结果 | [ ]
        NN_RESULT_GESTURE_DET result_gesture_det; // 手势检测结果 | [ ]
        NN_RESULT_GESTURE_REC result_gesture_rec; // 手势识别结果 | [ ]
        NN_RESULT_LP_DET result_lp_det;           // 车牌检测结果 | [ ]
        NN_RESULT_LP_REC result_lp_rec;           // 车牌识别结果 | [ ]
    };
} NN_RESULT;

typedef enum
{
    IMAGE_TYPE_RGB888 = (1 << 1), // 输入图像的颜色格式 | [ ]
    IMAGE_TYPE_RRGGBB = (1 << 2)
} NN_IMAGE_TYPE;

typedef struct nn_input_data
{
    MEM_DESC data;           // 输入图像的地址信息 | [ ]
    FH_UINT32 width;         // 输入图像数据的宽 | [ ]
    FH_UINT32 height;        // 输入图像数据的高 | [ ]
    FH_UINT32 stride;        // 输入图像的stride | [ ]
    NN_IMAGE_TYPE imageType; // 输入图像的像素格式 | [RGB888]
    FH_UINT64 frame_id;      // 帧序 | [ ]
    FH_UINT32 pool_id;       // vb pool id(非VB模式建议配置为-1UL) | []
    FH_UINT64 timestamp;     // 输入图像时间戳 | [ ]
} NN_INPUT_DATA;

typedef struct fh_model_info
{
    FH_VOID *mdlHdl;
    FH_VOID *postHdl;
    FH_CHAR *mdl_path;
    FH_UINT32 mdl_width;
    FH_UINT32 mdl_height;
    NN_TYPE type;
    FH_SINT32 task_count;
} FH_MODEL_INFO;

typedef struct
{
    FH_SINT32 id;
    FH_MODEL_INFO *model_info;
    FH_VOID *tskHdl;
} NN_HANDLE_t;

FH_SINT32 nna_init(FH_SINT32 nn_id, NN_HANDLE *nn_handle, NN_TYPE nn_type, FH_CHAR *model_path, FH_SINT32 model_width, FH_SINT32 model_height);

/* 当前已加载模型的 case id（.nbg 头偏移 0x0C）；0 = 未知。*/
FH_UINT32 nna_get_model_case(void);

FH_SINT32 nna_release(FH_SINT32 nn_id, NN_HANDLE *nn_handle);

FH_SINT32 nna_get_obj_det_result(NN_HANDLE nn_handle, NN_INPUT_DATA nn_input_data, NN_RESULT *nn_result);

FH_SINT32 nna_get_face_snap_result(NN_HANDLE nn_handle, NN_INPUT_DATA nn_input_data, NN_RESULT *nn_result);

FH_SINT32 nna_get_face_rec_result(NN_HANDLE nn_handle, NN_INPUT_DATA nn_input_data, NN_RESULT *nn_result);

FH_SINT32 nna_get_ges_det_result(NN_HANDLE nn_handle, NN_INPUT_DATA nn_input_data, NN_RESULT *nn_result);

FH_SINT32 nna_get_ges_rec_result(NN_HANDLE nn_handle, NN_INPUT_DATA nn_input_data, NN_RESULT *nn_result);

FH_SINT32 get_lprec_result(NN_HANDLE nn_handle, NN_INPUT_DATA nn_input_data, NN_RESULT *nn_result);

FH_SINT32 get_lpdet_result(NN_HANDLE nn_handle, NN_INPUT_DATA nn_input_data, NN_RESULT *nn_result);
#endif // __NNA_CONFIG_H__
