# 活动现场 · 活动票务与报名系统（实验三）

根据实验二的模块划分和 UML 图实现的 Windows 桌面程序，使用 C++17 编写。
- **界面**：Dear ImGui，Win32 + DirectX 11 后端，沿用实验二的天空蓝毛玻璃风格和吉祥物；
- **数据**：保存在 exe 旁边的文本文件 `ticketing_data.txt` 中，重新打开程序后数据不丢失。

## 环境

| 工具 | 版本 | 说明 |
| --- | --- | --- |
| g++（MinGW-w64） | 13.2 | 需要在 PATH 中 |
| CMake | ≥ 3.20 | |
| Ninja | 任意 | CMakePresets 使用 Ninja 生成器 |
| VS Code 扩展 | — | C/C++、CMake Tools |
| cpplint | 2.0 | `pip install cpplint`，仅用于代码风格检查 |

## 在 VS Code 中编译运行

1. 用 VS Code 打开本目录（`code/`）。CMake Tools 会读取 `CMakePresets.json`，选择配置预设 **MinGW g++ · Debug**。
2. 编译：按 `F7`，或运行任务“构建”。
3. 运行：选择启动目标 `ticketing_app`。
   - 按 `Shift+F5` 直接运行；
   - 按 `F5` 用 gdb 调试（配置见 `.vscode/launch.json`）。
4. 测试：运行任务“运行测试”，或执行 `ctest --preset mingw-debug`。
5. 代码风格检查：运行任务“代码风格检查（cpplint）”。

命令行等价操作：

```bash
cmake --preset mingw-debug
cmake --build --preset mingw-debug
ctest --preset mingw-debug
build/debug/ticketing_app.exe
python -m cpplint --recursive src tests
```

## 演示账号

第一次运行时会写入演示数据，各账号的密码均为 `123456`：

| 账号 | 角色 | 显示名 |
| --- | --- | --- |
| alice、bob、carol、dave | 参与者 | 小雨、小鹿、林同学、阿明 |
| org、org2 | 组织者 | 文艺部、陶艺社 |
| checker、checker2 | 检票员 | 阿杰、小林 |
| admin | 管理员 | 平台管理员 |

**模拟时钟**：程序使用模拟时钟，初始为 2026-11-01 10:00。导航栏的“+1 小时”“+1 天”按钮可以推进时间，用来演示：
- 报名截止后候补失效；
- 检票窗口开放；
- 场次结束后标记缺席，活动自动结束。

例如，“弦乐四重奏午间音乐会”的检票时间为当天 11:30。推进 2 小时后，用 checker 登录即可检票。

**重置数据**：删除 `build/debug/ticketing_data.txt` 后重新运行，即可恢复演示数据。

## 代码结构与实验二模块的对应

```
src/
├── main.cc                  程序入口：窗口、DirectX 11、主循环
├── common/                  时间工具与模拟时钟、文件读写、返回结果 Status
├── domain/model.*           领域模型（对应实验二类图）
├── data/store.*             S2 数据持久化：数据表、事务、文本文件存取
├── service/                 业务层，每个模块一组 .h/.cc
│   ├── account_service      M1 账户与权限
│   ├── activity_service     M2 活动管理（含 M7 检票员分配）
│   ├── review_service       M3 活动审核
│   ├── query_service        M4 活动查询
│   ├── registration_service M5 报名与候补 + M6 票券
│   ├── checkin_service      M7 检票签到
│   ├── notifier             M8 消息通知
│   ├── comment_service      M9 评论与评分
│   ├── favorite_service     M10 收藏与个人中心
│   ├── stats_service        M11 名单与统计
│   ├── audit_log            M12 操作日志
│   ├── scheduler            S1 定时调度
│   └── system               组装以上模块，并写入演示数据
└── ui/                      表现层，对应实验二的 10 个界面
    ├── theme.*              颜色、卡片、标签、按钮和吉祥物
    ├── app.*                页面框架：导航栏、提示条
    ├── screens_public.cc    界面 1～3：登录、活动广场、活动详情
    ├── screens_personal.cc  界面 4～6：我的票券、消息与收藏、检票
    └── screens_manage.cc    界面 7～10：活动编辑、名单统计、审核台、平台统计
tests/core_test.cc           业务规则测试（13 组，133 项检查）
third_party/                 Dear ImGui v1.92.9b（MIT 许可）、stb_image（公有领域）
assets/mascot.png            吉祥物图片
```

### 与实验二设计的差异

- 实验二是浏览器 / 服务器结构，本次按实验要求实现为桌面程序：
  - 去掉了接口层（HTTP API），界面直接调用业务层；
  - 数据库换成了文本文件加内存事务。
- 实验二的手机界面在桌面程序中改为桌面布局，功能不变。

## 代码风格

遵守 Google C++ 风格，用 cpplint 检查（配置见 `CPPLINT.cfg`，第三方代码不检查）。
- 当前 `src/` 和 `tests/` 的检查结果为 0 个问题；
- 用 `-Wall -Wextra` 编译，没有警告。
