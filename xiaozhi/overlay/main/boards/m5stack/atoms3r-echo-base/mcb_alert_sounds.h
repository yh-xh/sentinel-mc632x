#ifndef MCB_ALERT_SOUNDS_H
#define MCB_ALERT_SOUNDS_H

/*
 * 事件播报音：由 edge-tts 生成、ffmpeg 转成 48k mono Opus OGG，
 * 放在 main/assets/common/ 下，随固件一起嵌入。
 *
 * 不放进 lang_config.h，因为那个文件是 scripts/gen_lang.py 生成的，
 * 重新生成会把手工添加的内容冲掉。
 */
#include <cstddef>
#include <string_view>

namespace mcb_alert {

extern const char ogg_alert_person_start[] asm("_binary_alert_person_ogg_start");
extern const char ogg_alert_person_end[]   asm("_binary_alert_person_ogg_end");

extern const char ogg_alert_area_start[]   asm("_binary_alert_area_ogg_start");
extern const char ogg_alert_area_end[]     asm("_binary_alert_area_ogg_end");

static const std::string_view ALERT_PERSON {
    static_cast<const char*>(ogg_alert_person_start),
    static_cast<size_t>(ogg_alert_person_end - ogg_alert_person_start)
};

static const std::string_view ALERT_AREA {
    static_cast<const char*>(ogg_alert_area_start),
    static_cast<size_t>(ogg_alert_area_end - ogg_alert_area_start)
};

}  // namespace mcb_alert

#endif  // MCB_ALERT_SOUNDS_H
