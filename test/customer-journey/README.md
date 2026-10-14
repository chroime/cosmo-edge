# CosmoEdge 逐用例测试套件

在 Codex 中输入：

> 使用 $cosmoedge-full-test，对 CosmoEdge `<设备IP>`，平台用户名 `<用户名>`、密码 `<密码>`，按完整用例库执行测试并输出逐用例报告。

技能负责实际调用浏览器/API/精度工具；`tools/customer_journey.py` 是持久化队列和报告工具，不是无人值守浏览器执行器。修改凭据的用户接管、物理外设确认及尚未授权的高影响操作仍需按实际条件处理，不会因一句全量测试而绕过审批。

[`CosmoEdge-客户旅程测试用例.xlsx`](CosmoEdge-客户旅程测试用例.xlsx) 是唯一维护入口，当前版本为 1.4.0。从原 v0.1 工作簿持续保留客户旅程及覆盖表，现有 327 条父用例、1445 条验收检查。修改“详细用例”及设备/参数/组件/接口矩阵，下一轮直接读取 XLSX。额外验收点写在“补充检查”。历史测试报告仍对应各轮冻结的用例库版本，不自动覆盖新增用例。

`suite.json` 由 XLSX 编译生成，提交它是为了在 GitHub 查看文字差异，不手工维护。`coverage.json` 仅为早期源码盘点参考，不参与执行；新增功能需同步工作簿内用例及覆盖表。每轮冻结 `casebook.snapshot.xlsx` 和 `suite.snapshot.json`，并记录 SHA-256。历史结果始终对应当时版本。

`resources.example.json` 是不带凭据的通用资源格式。实际账号、密码和路径只放在 ignored 的 `output/agent-runs/` 私有配置。用户每轮只需给目标设备登录信息；固定摄像机、NVR、服务器和素材配置可复用。默认配置路径见技能。允许明文不代表允许将配置提交到开源仓库。

支持可迁移私有资源包：包内 `resources.private.json` 使用相对路径，`manifest.json` 保存素材大小和 SHA-256。新电脑复制整个包，在仓库根运行一次 `python <资源包根目录>/kit.py configure`，即可保存本机根目录；后续不必逐项修改素材路径。也可用 `COSMO_JOURNEY_RESOURCE_ROOT` 临时覆盖根目录。新电脑需要 Python 3 和本仓库；包内携带配套工具，仍读取仓库内的正式 XLSX，不另建用例主库。

启动前运行 `python <资源包根目录>/kit.py check --full --network`；首次迁移使用完整哈希检查，日常可省略 `--full`，只检查文件存在性和大小。`--network` 仅探测配置中的 TCP 端口，不登录、不修改设备，也不证明协议认证或算法通过。检查列出缺口及受影响用例，保留用户已确认的不适用项；缺口不会把整轮用例统一阻塞。`init` 会自动执行本地资源预检。资源检查报告默认写入包内 `checks/latest.json`；配置损坏返回错误，资源缺口作为报告数据交由 Codex 按用例处理。

新建的可迁移运行保存资源配置快照和相对配置引用。复制该轮完整目录并重新配置同一资源包根目录后可续测；共享配置的后续修改不覆盖历史快照。旧版运行仍兼容，但旧绝对路径不会被自动改写。照片、模型、升级包、账号、根目录配置和实测记录均不提交 GitHub。精度服务器的脚本/样本及外部接收服务仍是远端依赖，资源包不等于离线测试环境。

最终执行 `report --run <run>`，同时生成以下报告：

- `CosmoEdge-逐用例测试结果.xlsx`：在冻结工作簿中按用例 ID 回填状态、实际结果、未通过原因、失败步骤、证据、版本、时间和清理情况；总览自动计数，“步骤与检查结果”逐项列明结果，有重测时附“重测历史”。
- `report.html`：支持编号/模块/状态筛选及“仅看未通过”，并链接结果 XLSX 和证据。
- 未通过原因：XLSX 的“原因与补测”和 HTML 均列明原因、剩余检查、下一步、处理方及证实程度；报告单列未分析项和原因未知项。`explain` 可补充说明，保留原实测状态；不得把潜在资源缺口推断为历史中止原因。
- `results.json` / `summary.json`：持久化结果、重试历史及汇总。每次 `record` 即时更新 HTML/JSON；XLSX 由 `report` 刷新，避免每步重写整个文件。

结果不覆盖主库。保留整个 run 目录才能打开相对路径证据；单独发送 XLSX 仍可看结论，但不附带证据文件。精度样本作为用例证据，不替代功能用例数量。历史导入明确标注“未按此版 XLSX 重测”，不能据此通过新版本验收。

读取/校验/记录使用 Python 3 标准库。XLSX 导出使用 Codex 随附的 Node 和 `@oai/artifact-tool`，不安装仓库依赖。若运行时位于其他路径，先用 `load_workspace_dependencies` 获取路径，再设置 `COSMO_JOURNEY_NODE` 和 `COSMO_JOURNEY_NODE_MODULES`。缺少导出运行时会明确失败，已保存的 HTML/JSON 不丢失。

运行和维护方式见 [技能入口](skill/SKILL.md)、[执行说明](skill/references/execution.md) 和 [维护说明](skill/references/maintenance.md)。

修改主库后的检查：

```text
python tools/customer_journey.py compile
python tools/customer_journey.py compile --check
python tools/customer_journey.py validate
python -m unittest discover -s test/customer-journey -p "test_*.py"
npm run docs:verify
```

Windows 终端使用 UTF-8（例如 PowerShell 设置 `$env:PYTHONUTF8='1'`）。提交范围为本目录、`tools/customer_journey*.py`、`tools/customer_journey_xlsx.mjs` 及入口说明；不要提交 `output/agent-runs/` 中的账号、个人素材和实测报告。
