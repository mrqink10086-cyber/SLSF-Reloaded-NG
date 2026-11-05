#include "slsf/ui/MenuText.h"

#include <utility>

namespace slsf::ui {

std::string_view MenuStatusText(Status status) noexcept {
    switch (status) {
    case Status::Ready: return "操作成功";
    case Status::NotReady: return "尚未就绪";
    case Status::Missing: return "缺少依赖";
    case Status::Incompatible: return "版本不兼容";
    case Status::Invalid: return "输入无效";
    case Status::Stale: return "会话已失效";
    }
    return "未知状态";
}

std::string MenuMessageText(std::string_view message) {
    static constexpr std::pair<std::string_view, std::string_view> translations[]{
        {"configuration initialized from defaults", "已使用默认值创建配置"},
        {"configuration file missing and could not be generated", "配置文件不存在，且无法创建"},
        {"configuration file is missing", "配置文件不存在"},
        {"configuration file read failed", "读取配置文件失败"},
        {"configuration JSON is invalid", "配置 JSON 格式无效"},
        {"configuration edit rejected", "配置修改被拒绝"},
        {"configuration write failed", "写入配置失败"},
        {"configuration invalid", "配置无效"},
        {"configuration updated", "配置已更新"},
        {"configuration unchanged", "配置未更改"},
        {"unresolvable location or category", "无法识别地点或声望类别"},
        {"unresolvable source location", "无法识别来源地点"},
        {"unresolvable location", "无法识别地点"},
        {"executor detached", "当前没有活动存档"},
        {"executor suspended", "业务执行已暂停"},
        {"settlement inputs not bound", "结算数据尚未就绪"},
        {"cannot open file", "无法打开文件"},
        {"cannot create temp file", "无法创建临时文件"},
        {"temp file write failed", "写入临时文件失败"},
        {"read failed mid-stream", "文件读取中断"},
        {"replace failed", "替换文件失败"},
        {"must be an array of category names", "必须是声望类别名称数组"},
        {"is not a boolean", "必须为布尔值"},
        {"is not a number", "必须为数值"},
        {"is not finite", "必须为有限数值"},
        {"must be an integer", "必须为整数"},
        {"is out of range", "超出允许范围"},
        {"are out of range", "超出允许范围"},
        {"is not an object", "必须为对象"},
        {"is not an array", "必须为数组"},
        {"is not a string", "必须为字符串"},
        {"is missing", "缺少此项"},
        {"is invalid", "此项无效"},
        {"contain duplicates", "包含重复项"},
        {"exceed limits", "超出数量限制"},
        {"exceeds limits", "超出数量限制"},
    };
    std::string result(message);
    for (const auto& [from, to] : translations) {
        std::size_t position = 0;
        while ((position = result.find(from, position)) != std::string::npos) {
            result.replace(position, from.size(), to);
            position += to.size();
        }
    }
    return result;
}

}
