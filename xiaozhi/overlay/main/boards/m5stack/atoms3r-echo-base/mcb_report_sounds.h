#ifndef MCB_REPORT_SOUNDS_H
#define MCB_REPORT_SOUNDS_H

/*
 * 录制报告语音片段（edge-tts 生成，48k mono Opus OGG）。
 * 固件按片段拼接播报：本次拍摄期间，进入 X 人，离开 Y 人。
 */
#include <cstddef>
#include <string_view>

namespace mcb_report {

extern const char ogg_rp_start_start[] asm("_binary_rp_start_ogg_start");
extern const char ogg_rp_start_end[]   asm("_binary_rp_start_ogg_end");
extern const char ogg_rp_in_start[] asm("_binary_rp_in_ogg_start");
extern const char ogg_rp_in_end[]   asm("_binary_rp_in_ogg_end");
extern const char ogg_rp_out_start[] asm("_binary_rp_out_ogg_start");
extern const char ogg_rp_out_end[]   asm("_binary_rp_out_ogg_end");
extern const char ogg_rp_mid_start[] asm("_binary_rp_mid_ogg_start");
extern const char ogg_rp_mid_end[]   asm("_binary_rp_mid_ogg_end");
extern const char ogg_rp_end_start[] asm("_binary_rp_end_ogg_start");
extern const char ogg_rp_end_end[]   asm("_binary_rp_end_ogg_end");
extern const char ogg_rp_peak_start[] asm("_binary_rp_peak_ogg_start");
extern const char ogg_rp_peak_end[]   asm("_binary_rp_peak_ogg_end");
extern const char ogg_num_0_start[] asm("_binary_num_0_ogg_start");
extern const char ogg_num_0_end[]   asm("_binary_num_0_ogg_end");
extern const char ogg_num_1_start[] asm("_binary_num_1_ogg_start");
extern const char ogg_num_1_end[]   asm("_binary_num_1_ogg_end");
extern const char ogg_num_2_start[] asm("_binary_num_2_ogg_start");
extern const char ogg_num_2_end[]   asm("_binary_num_2_ogg_end");
extern const char ogg_num_3_start[] asm("_binary_num_3_ogg_start");
extern const char ogg_num_3_end[]   asm("_binary_num_3_ogg_end");
extern const char ogg_num_4_start[] asm("_binary_num_4_ogg_start");
extern const char ogg_num_4_end[]   asm("_binary_num_4_ogg_end");
extern const char ogg_num_5_start[] asm("_binary_num_5_ogg_start");
extern const char ogg_num_5_end[]   asm("_binary_num_5_ogg_end");
extern const char ogg_num_6_start[] asm("_binary_num_6_ogg_start");
extern const char ogg_num_6_end[]   asm("_binary_num_6_ogg_end");
extern const char ogg_num_7_start[] asm("_binary_num_7_ogg_start");
extern const char ogg_num_7_end[]   asm("_binary_num_7_ogg_end");
extern const char ogg_num_8_start[] asm("_binary_num_8_ogg_start");
extern const char ogg_num_8_end[]   asm("_binary_num_8_ogg_end");
extern const char ogg_num_9_start[] asm("_binary_num_9_ogg_start");
extern const char ogg_num_9_end[]   asm("_binary_num_9_ogg_end");
extern const char ogg_unit_ten_start[] asm("_binary_unit_ten_ogg_start");
extern const char ogg_unit_ten_end[]   asm("_binary_unit_ten_ogg_end");
extern const char ogg_unit_hundred_start[] asm("_binary_unit_hundred_ogg_start");
extern const char ogg_unit_hundred_end[]   asm("_binary_unit_hundred_ogg_end");
extern const char ogg_unit_thousand_start[] asm("_binary_unit_thousand_ogg_start");
extern const char ogg_unit_thousand_end[]   asm("_binary_unit_thousand_ogg_end");

static const std::string_view RP_START {
    static_cast<const char*>(ogg_rp_start_start),
    static_cast<size_t>(ogg_rp_start_end - ogg_rp_start_start)
};
static const std::string_view RP_IN {
    static_cast<const char*>(ogg_rp_in_start),
    static_cast<size_t>(ogg_rp_in_end - ogg_rp_in_start)
};
static const std::string_view RP_OUT {
    static_cast<const char*>(ogg_rp_out_start),
    static_cast<size_t>(ogg_rp_out_end - ogg_rp_out_start)
};
static const std::string_view RP_MID {
    static_cast<const char*>(ogg_rp_mid_start),
    static_cast<size_t>(ogg_rp_mid_end - ogg_rp_mid_start)
};
static const std::string_view RP_END {
    static_cast<const char*>(ogg_rp_end_start),
    static_cast<size_t>(ogg_rp_end_end - ogg_rp_end_start)
};
static const std::string_view RP_PEAK {
    static_cast<const char*>(ogg_rp_peak_start),
    static_cast<size_t>(ogg_rp_peak_end - ogg_rp_peak_start)
};

static const std::string_view NUM[10] = {
    std::string_view{static_cast<const char*>(ogg_num_0_start),
                    static_cast<size_t>(ogg_num_0_end - ogg_num_0_start)},
    std::string_view{static_cast<const char*>(ogg_num_1_start),
                    static_cast<size_t>(ogg_num_1_end - ogg_num_1_start)},
    std::string_view{static_cast<const char*>(ogg_num_2_start),
                    static_cast<size_t>(ogg_num_2_end - ogg_num_2_start)},
    std::string_view{static_cast<const char*>(ogg_num_3_start),
                    static_cast<size_t>(ogg_num_3_end - ogg_num_3_start)},
    std::string_view{static_cast<const char*>(ogg_num_4_start),
                    static_cast<size_t>(ogg_num_4_end - ogg_num_4_start)},
    std::string_view{static_cast<const char*>(ogg_num_5_start),
                    static_cast<size_t>(ogg_num_5_end - ogg_num_5_start)},
    std::string_view{static_cast<const char*>(ogg_num_6_start),
                    static_cast<size_t>(ogg_num_6_end - ogg_num_6_start)},
    std::string_view{static_cast<const char*>(ogg_num_7_start),
                    static_cast<size_t>(ogg_num_7_end - ogg_num_7_start)},
    std::string_view{static_cast<const char*>(ogg_num_8_start),
                    static_cast<size_t>(ogg_num_8_end - ogg_num_8_start)},
    std::string_view{static_cast<const char*>(ogg_num_9_start),
                    static_cast<size_t>(ogg_num_9_end - ogg_num_9_start)},
};

static const std::string_view UNIT_TEN {
    static_cast<const char*>(ogg_unit_ten_start),
    static_cast<size_t>(ogg_unit_ten_end - ogg_unit_ten_start)
};
static const std::string_view UNIT_HUNDRED {
    static_cast<const char*>(ogg_unit_hundred_start),
    static_cast<size_t>(ogg_unit_hundred_end - ogg_unit_hundred_start)
};
static const std::string_view UNIT_THOUSAND {
    static_cast<const char*>(ogg_unit_thousand_start),
    static_cast<size_t>(ogg_unit_thousand_end - ogg_unit_thousand_start)
};

}  // namespace mcb_report

#endif  // MCB_REPORT_SOUNDS_H
