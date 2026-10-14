# CosmoEdge 逐用例测试套件

在 Codex 中输入：

> 使用 $cosmoedge-full-test，对 CosmoEdge `<设备IP>`，平台用户名 `<用户名>`、密码 `<密码>`，按完整用例库执行测试并输出逐用例报告。

技能负责实际调用浏览器/API/精度工具；`tools/customer_journey.py` 是持久化队列和报告工具，不是无人值守浏览器执行器。修改凭据的用户接管、物理外设确认及尚未授权的高影响操作仍需按实际条件处理，不会因一句全量测试而绕过审批。

[`CosmoEdge-客户旅程测试用例.xlsx`](CosmoEdge-客户旅程测试用例.xlsx) 是唯一维护入口，当前版本为 1.4.0。从原 v0.1 工作簿持续保留客户旅程及覆盖表，现有 327 条父用例、1445 条验收检查。修改“详细用例”及设备/参数/组件/接口矩阵，下一轮直接读取 XLSX。额外验收点写在“补充检查”。历史测试报告仍对应各轮冻结的用例库版本，不自动覆盖新增用例。

`suite.json` 由 XLSX 编译生成，提交它是为了在 GitHub 查看文字差异，不手工维护。`coverage.json` 仅为早期源码盘点参考，不参与执行；新增功能需同步工作簿内用例及覆盖表。每轮冻结 `casebook.snapshot.xlsx` 和 `suite.snapshot.json`，并记录 SHA-256。历史结果始终对应当时版本。

`resources.example.json` 是不带凭据的通用资源格式。实际账号、密码和路径只放在 ignored 的 `output/agent-runs/` 私有配置。用户每轮只需给目标设备登录信息；固定摄像机、NVR、服务器和素材配置可复用。默认配置路径见技能。允许明文不代表允许将配置提交到开源仓库。

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
