## Agent skills

### Native migration
Start with `.scratch/native-migration/spec.md` for requirements, decisions and task status.
Follow its links for task details and verification evidence.

### Code comments

- 默认不新增代码注释，包括文档注释；优先用准确命名、类型和清晰结构表达意图。宁可没有注释，也不要写无用注释；拿不准就不写。
- 例外仅限法律/许可、必要工具指令，以及代码和类型无法表达、遗漏会造成误用的必要约束（如外部协议、单位、所有权、线程或生命周期）。普通说明压到一两行，外部约束注明可核对的来源；没有例外就保持零新增注释。
- 禁止逐句翻译代码、复述签名或常量、章节横幅、文件职责介绍、修改历史、无跟踪事项的 TODO，以及给每个函数/类型套模板文档注释。设计理由和验证经过写入对应文档或提交记录。
- 交付前检查本次 diff 中新增或修改的注释，删除不符合上述例外的内容；不为写注释制造抽象或扩展任务范围。

### Issue tracker
Track specs and issues in local Markdown under `.scratch/`.
Read `docs/agents/issue-tracker.md` before tracker operations.

### Triage labels
Use the five default triage roles.
Read `docs/agents/triage-labels.md` before triage.

### Domain docs
Use a single-context layout.
Read `docs/agents/domain.md` before domain exploration or design changes.
