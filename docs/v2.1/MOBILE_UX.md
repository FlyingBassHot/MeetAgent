# MeetAgent Mobile UX 改造计划

> 状态：P0 完成（2026-11-08），P1 进行中  
> 负责人：`omp`  
> 范围：只改 `mobile/`；Node / 协议 / 根文档由主会话维护  
> 目标：从“移动端网页控制台”变成“原生感移动 App”，同时保留深色终端美学

## 1. 已确认决策

- **视觉 A**：保留当前深色终端美学，但减少卡片边框、压缩技术信息、增加留白和原生列表感。
- **文案 A**：内容以中文为主；底部 Tab 标签保留 `Home / Sessions / Activity / Settings`。
- **导航 A**：保留 4 个 Tab；Activity 页面标题改为「动态」，并做事件降噪。
- **技术信息**：统一收进 Settings →「开发者信息」一个入口，主界面默认不展示。
- **实施方式**：分 P0 / P1 / P2 三阶段，每阶段验收后再进入下一阶段。

## 2. 信息可见性

### 主界面默认显示

- Agent / Session 名称
- 运行状态、待审批数量
- 最近活动时间
- 人类可读的事件摘要
- 当前连接是否在线
- 待审批权限的描述和操作按钮

### 主界面默认隐藏

| 信息 | 统一入口 |
|---|---|
| Session ID | 开发者信息 / 详情页菜单复制 |
| Node ID、hostname、platform、version | 开发者信息 |
| Client ID、Token 预览、capabilities、permissions | 开发者信息 |
| 事件 sequence、event_id | 开发者信息（原始字段开关） |
| 原始 payload、adapter、source | 开发者信息（原始字段开关） |
| RTT、同步时间、事件缓存条数 | 开发者信息 |
| WSS 路径、协议版本、配对 TTL | 开发者信息 |
| working_directory | Session 详情折叠区 |

## 3. 页面改造

### Home → 概览

- 页面标题「概览」，Tab 仍为 `Home`
- 删除 4 个统计块，改成一行紧凑状态：`1 Agent · 1 运行中 · 0 待审批`
- 待审批/异常保持最高优先级
- 活跃 Session 最多 3–4 条
- 最近动态最多 3–5 条
- 删除 Agent 状态独立区块（单 Agent 时）
- 删除底部 Node 技术信息卡片

### Sessions → 会话

- 页面标题「会话」
- 搜索默认收起，右上角搜索按钮展开
- 筛选改为「筛选」按钮 + 底部面板（P1 可升级原生 Sheet）
- 列表行最多 3 行：
  1. 标题
  2. 状态 + Agent + 相对时间
  3. 待审批 badge（如有）
- 默认隐藏 Session ID、工作目录
- 运行中 / 等待中优先排序

### Session 详情

- 原生顶部导航：返回按钮 + 标题（1 行省略）+ 状态 + 「…」菜单
- 「…」菜单：Interrupt、复制 Session ID、打开原始事件（开发者模式）
- 删除 Session ID、事件数量、全局 sequence
- 时间线改为聊天流：
  - 用户 Prompt 气泡
  - Agent 消息聚合
  - 工具调用折叠成一张小卡
  - 权限请求内联
- 删除单独「本机发送记录」区块，Prompt 合并到时间线
- 输入框去掉 `Ctrl / ⌘ + Enter` 和字符计数
- Interrupt 不再使用双击文字按钮；改为菜单/Action Sheet（P1 原生确认）

### Activity → 动态

- 页面标题「动态」，Tab 仍为 `Activity`
- 默认隐藏 `message.delta`、heartbeat、sequence、event_id、原始 payload
- 只展示重要事件：Session 开始/完成/失败、审批、Agent 上下线、工具失败
- 按「今天 / 昨天 / 更早」分组
- 筛选收进「筛选」按钮
- 原始事件字段由开发者信息中的开关控制

### Settings → 设置

- 改成原生分组列表：
  - 连接
  - 通知
  - 外观
  - 关于
  - 开发者信息
- 主设置页移除：Client ID、Token 预览、capabilities、权限列表、NodeInfoCard、ConnectionDiagnostics
- 统一「开发者信息」页面包含：
  - 当前 Node / 连接详情
  - Client ID / Token 预览 / 权限 / capabilities
  - RTT / 同步 / 协议版本 / WSS 路径
  - 原始事件字段开关（默认关）
  - 诊断信息

### Connect → 连接

- 页面标题「连接」
- 主流程只保留：Node 地址、配对码、设备名称
- 「检测 Node」默认只显示名称和在线状态
- Node ID / platform / version / Adapter / WSS 路径收进「高级信息」
- 已有 Token 收进高级选项
- Mock 模式作为次级「演示模式」入口

## 4. 阶段计划

### P0：信息减法 + 页面骨架

- Home / Sessions / Session 详情 / Activity / Settings / Developer / Connect 的布局与信息层级调整
- 删除主界面技术字段
- 统一开发者信息入口
- 中文页面标题，英文 Tab
- 不引入新的原生插件
- 验收：无 ID/序号/原始 payload 泄漏；Home ≤ 3 区块；Session 行 ≤ 3 行；360px 无横向溢出

### P1：原生交互

- 下拉刷新
- 筛选/审批底部 Sheet
- Interrupt / 拒绝审批 Action Sheet
- Haptics
- Android 返回键
- 键盘弹起时输入框适配
- 骨架屏 / 滚动位置保持 / 页面切换动画

### P2：视觉打磨

- 字号、间距、圆角、分隔线
- 状态色、图标一致性
- 空状态 / 错误态 / 加载态
- 启动图、App 图标、状态栏颜色
- 360 / 390px 真机截图验收

## 5. 验收原则

- 首屏回答：“现在有没有需要我处理的事？”
- 主要操作（审批、Interrupt、发送 Prompt）尽量 2 次点击内完成
- 默认界面不出现内部 ID、序号、原始事件字段
- 所有功能（Prompt、Interrupt、权限、断网恢复）不回归
- Android 真机 + 移动端 Web 都验收
