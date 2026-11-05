# ESPUI ViewModel 规范（E0 / E1 / E2）

代码：`components/espui/core/espui_view_model.h`

## 1. 为什么需要 ViewModel

Theme 只消费 ViewModel，不接触：

- Node Snapshot / Event JSON
- WebSocket / HTTP
- 协议字段名与版本
- 原始时间戳格式

这样更换协议、增加第二个 Adapter 或更换主题渲染器时，主题代码不需要跟着改。

## 2. 维度

```text
Terminal ViewModel
├── Node        在线/连接中/离线/同步中，node_id、名称、最后同步
├── Agents      固定上限 8，状态、活跃 Session 数
├── Sessions    固定上限 8，状态、标题、持续时间
├── Attention   固定上限 6，审批/异常/完成，摘要与计数
└── Device      亮度、夜间模式、动画开关/速度、当前主题
```

所有字符串都是固定长度数组，不使用指针或动态分配。容量宏：

```c
ESPUI_MAX_AGENTS     8
ESPUI_MAX_SESSIONS   8
ESPUI_MAX_ATTENTION  6
ESPUI_ID_MAX         40
ESPUI_NAME_MAX       32
ESPUI_TITLE_MAX      48
```

## 3. 业务态与设备态

```text
业务态：node / agents / sessions / attention / 计数
设备态：device.brightness / night_mode / motion_enabled /
        animation_speed_percent / active_theme_id
```

规则：

- 网络/状态任务只拥有业务态。
- UI Core 拥有设备态；用户设置不会因为一次 Snapshot 被覆盖。
- `espui_view_model_copy_business(dst, src)` 保留 `dst.device`，替换业务态并递增 `dst.revision`。
- `espui_view_model_reset(model, keep_device)` 清空业务态；`keep_device=true` 用于 Snapshot 全量替换。

## 4. revision

- `revision` 是“模型发生过变化”的本地单调计数，不是 Node 的 sequence。
- `init()` 从 1 开始。
- 每次 upsert/remove/touch/attention 变化递增。
- `copy_business()` 在目标上递增，不采用来源 revision，避免回退。
- Node sequence 单独保存在 `meet_state_adapter_t.last_sequence`，用于 E4 断线补偿。

## 5. 固定容量行为

- `upsert_*` 返回可写指针；容量满返回 `NULL`，不覆盖旧数据。
- `remove_*` / `resolve_attention` 返回是否存在。
- Snapshot 中协议容量（8/8/6）与 ViewModel 一致；Protocol 校验会拒绝超限。
- 若多 Agent 的 Session 总数超过 8，State Adapter 跳过放不下的 Session，不破坏已有数据。
- Attention 计数由 `recount` 从 `in_use` 项重算，避免手工计数漂移。

## 6. 状态枚举

```c
espui_node_status_t    OFFLINE / CONNECTING / ONLINE / SYNCING
espui_agent_status_t   UNKNOWN / IDLE / BUSY / ERROR / OFFLINE
espui_session_status_t UNKNOWN / RUNNING / WAITING_APPROVAL /
                       COMPLETED / FAILED / INTERRUPTED
espui_attention_kind_t APPROVAL / ERROR / COMPLETION
```

`active_session_count` 只统计 `RUNNING` 与 `WAITING_APPROVAL`。

## 7. 线程与所有权

- UI Task 是活动 ViewModel 的唯一写者。
- 其他任务只能构造副本并调用 `espui_post_view_model()`。
- 事件队列满返回 `ESP_ERR_TIMEOUT`；调用方应丢弃旧状态或稍后重试，不得阻塞网络任务。
- 主题只能读 ViewModel；不得写入或保存指针。

## 8. State Adapter

代码：`components/state/state_adapter.c`

E2 已实现 Snapshot → ViewModel：

```text
meet_protocol_snapshot_t
  → validate schema / counts / ids
  → ViewModel reset(keep_device=true)
  → Node 映射
  → Agent / Session 映射
  → Pending permission → Attention(APPROVAL)
  → last_sequence = snapshot.last_sequence
```

- 时间字段使用设备 uptime `now_ms`（到达时间），不混用 Node 墙钟。
- 非 `pending` 的权限不进入 Attention。
- Snapshot 带 `last_sequence`，但不回发事件；E4 的 Event 追加逻辑仍为 stub。
- 事件接口 `meet_state_adapter_apply_event()` 返回 `ESP_ERR_NOT_SUPPORTED`，留给 E4。

## 9. Mock 状态

代码：`components/state/mock_state.c`

四个阶段循环，每 3 秒：

1. Agent busy / Session running
2. Approval requested / Session waiting
3. Session completed / Agent idle / completion attention
4. Node offline / Agent offline

它通过 `espui_post_view_model()` 进入 UI Task，与未来真实网络路径一致。设备态由 UI Core 合并保留。

## 10. 未来扩展

- 多 Node：ViewModel 需要节点集合，当前按“一个主 Node”实现；协议结构保留 `node_id`。
- 更多 Agent/Session：优先提升固定上限或分页，不引入无界历史。
- Event 增量：在 State Adapter 中按 sequence 去重、补发和 Snapshot 重建；ViewModel 容量不变。
