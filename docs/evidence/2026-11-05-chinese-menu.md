# SKSE 菜单简体中文

菜单分页、玩法选项、声望类别、默认地区名称、操作按钮及状态提示使用简体中文。
显示名称与原有 API 名称分开，操作仍传递原来的类别和地区标识。
插件以 `/utf-8` 编译，设置继续使用 SKSE 菜单。

构建验证：`xmake build -y SLSFReloadedNG SLSFConfigProbe SLSFCoreTests SLSFMenuFrameworkProbe` 通过。
核心测试通过 425 项、150177 个断言；菜单框架探针通过未加载框架时的保护分支。
未运行游戏，中文字体显示需在游戏中确认。

安装说明记录框架字体选项 `PrimaryFont = Futura Condensed (CN)` 和
`EnableChinese = true`。依据为只读的菜单框架 `Usage.md` 第 113–119 行及
默认 INI 的 `[Fonts]` 设置。未修改框架源码或用户的框架配置。
