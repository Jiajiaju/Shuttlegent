---
name: level-review
description: 按清单检查当前关卡：光源数量、未命名 Actor、隐藏的 Actor。用户要求检查关卡或做关卡自查时使用。
---

# 关卡自查（Shuttlegent 示例技能）

1. 用 `sample_count_actors` 统计各类 Actor 数量，光源（类名含 Light）超过 20 个时提醒。
2. 用 `list_actors` 找标签仍是默认名（以类名加数字结尾）的 Actor，列出前 10 个。
3. 对选中的 Actor 用 `get_properties` 读 `bHidden`，列出在游戏中隐藏的。
4. 只汇报，不修改；需要改名时建议用户确认后再调用 `sample_set_actor_label`。
