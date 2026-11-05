# MeetAgent V2.1 状态板

更新时间：2026-11-05

| 模块 | 负责人 | 当前阶段 | 状态 | 下一步 |
|---|---|---|---|---|
| Core / Node | 主会话 | M0 + M1 | 可运行 | M3 真实双端集成 |
| Protocol v2 | 主会话 | M0 | 完成 | 随实现补兼容测试 |
| Mobile | omp | M3 | 完成（真实 Node Prompt / Interrupt / 审批 / 重连） | M4 多客户端 |
| Terminal / ESPUI | pi | E0–E2 + E3a | 暂停（host tests 805/805，33 files syntax） | 等 ESP32 真机 + ESP-IDF |
| Cloud relay | 主会话 | 可选 | 保留 V1 实现 | M6 再验证混合模式 |

## 已完成

- V2.1 文档统一；`v1.0-phase3` tag
- `pkg/clientproto`：hello/welcome、snapshot、event、command/result、pair
- Node standalone：HTTP + WS `:8787`、`/healthz`、`/api/v2/info`、`/api/v2/pair`、`/api/v2/snapshot`、`/api/v2/clients`
- 配对：6 位码、TTL、Token SHA-256、持久化、撤销
- Client Registry：client_id/type/capabilities/permissions/last_sequence、同 ID 替换
- Event Hub：Node sequence、bounded cache、snapshot、replay、raw fan-out（Cloud 可选）
- Command Gateway：幂等、超时、client capability + permission + adapter capability
- Adapter Manifest + Mock Adapter + `cmd/simulator`
- mDNS `_meetagent._tcp`（avahi-browse 实测）
- 测试：`go test ./... -race -count=1` 通过
- Mobile：`npm run typecheck` / `npm run build` 通过；390/1440 截图
- Terminal：host tests 620/620；syntax 31/31；ESP-IDF 未安装

## 真实联调（主会话）

- Mock Node：`configs/node-mock.yaml` + simulator：配对、prompt、message/tool 事件通过
- OpenCode Node：`configs/node.yaml` + simulator：36 个真实 Session，prompt `V21-SIM` 返回成功
- Admin：`GET /api/v2/clients` 正确列出模拟器；撤销后 Token 失效（集成测试）
- mDNS：`avahi-browse -rt _meetagent._tcp` 能看到 node-001:8787

## 阻塞 / 暂停

- **ESP32 模块暂停**：硬件未到、ESP-IDF 未安装；Terminal 停在 E3a，不继续写 ESP 代码
- Mobile：使用 5174，避免与 V1 的 5173 冲突
- 语音/STT：按用户要求暂缓
- 多客户端/同 Token 多标签页：M4 处理
