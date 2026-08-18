# FoxVast ROADMAP / 设计决策

本文档记录 FoxVast 当前(1.4.2)的设计取舍、已知限制与后续路线。凡标"决策"的条目是**有意为之、不是 bug**,请勿在 issue 中当作缺陷提交;标"待定"的是尚未决定、需要路线图裁决的事项。

---

## 已确认的设计决策

### 1. 字节码"加密"是混淆,不是加密
`.fc` 与 `pack=true` 的 `.fx/.fz` 使用**单字节 XOR(0x2A)** 加 varint 压缩与常量去重。这是为减小体积与"防小白直读"的混淆手段,**不做任何安全承诺**,可被轻易还原。
- 决策:保持现状;若要承诺"加密"需替换为真正的加密(见路线图)。
- 相关:`src/vm/far.cpp`(ZIP 打包)、`src/vm/bytecode.cpp`(FC_XOR_KEY)、`src/util/utils.cpp`(XOR)。

### 2. 无布尔类型
`Value::Type` 只有 Int/String/Double/Void/Array/Bytes/Dict/Object/Return/Unknown。条件判断走 `asBool()`,规则:数字 0 为 false、非 0 为 true、空数组/空 dict `false`,**字符串作条件直接抛异常**。`and/or/not` 作用于条件。
- 决策:条件语义已稳定,不引入 bool 类型(路线图中期可能加,见下)。

### 3. 无 int64 / 时间只有秒级
`Value` 只有 `int`(32 位);`fox.sys.time` 的 `now`/`format`/`field` 均为秒级,无毫秒/微秒。
- 决策:接受毫秒级不保证的使用前提;引入 int64 需要动 Value 全量类型系统,列为中期项。

### 4. 字符串是裸字节串
运行时字符串就是 `std::string` 的字节,**无 UTF-8 感知**:`util.str_length` 数的是字节、`str_substring` 按字节切,中文字符串截断可能切出半个字符;大小写转换同样只对 ASCII 有意义。
- 决策:当前原样;UTF-8 感知列为长期项(会破坏现有字节语义的脚本)。

### 5. 内存管理:free 清理 + 无 GC
`free` / `free_all` 的语义是清除全局变量表(OP_UNSET_GLOBAL / OP_CLEAR_GLOBALS),不是引用计数也不是 GC。dict/object 的复制依赖 `std::vector<Value>` / `shared_ptr`,深拷贝链路长。`new()` 字节数组分配有**每函数 512 字节**硬上限(MAX_FUNC_NEW_BYTES),贪吃蛇示例因此限制了蛇长。
- 决策:512 上限与 free 语义保持;重审方案列入路线图(见下)。

### 6. Windows 优先
外部库搜索路径硬编码 Windows(`C:\FoxLibs\`、`C:\Program Files\FoxVast\Libs\`、`.\libs\`),DLL 用 `LoadLibraryA` 加载,`-s` 走 `chcp`。
- 决策:Windows 是第一平台;跨平台列为长期项。

### 7. 解释器与 VM 双轨
`-f` 走旧 AST 解释器(标注 deprecated),`-fc` 走字节码 VM。两条路径并行,行为一致性由 `test/test_games/features.fox` 回归把关(断言 "FEATURES OK")。
- 决策:保留 `-f` 做调试对照,长期目标是只留 VM。

### 8. dict/数组的"不可变风格"
函数参数与赋值是值传递;dict 没有原地修改,改一个键必须整体重建新 dict 字面量并接住返回值(fjson/flog 都遵循这一风格)。数组可变(arr_append/arr_pop/下标赋值)。
- 决策:dict 值传递语义是语言级设计,插件教程已按此风格书写。

### 9. 异常链路依赖 C++ 异常
`try/catch`、`goto`(GotoException)、`break`/`continue` 的控制流由 C++ 异常驱动;库层错误抛 `std::runtime_error`,VM 捕获后做栈回溯。`error("msg")` 抛出的 LangError 可被 `catch (e)` 接住作为普通异常处理。
- 决策:语义稳定,性能代价接受。

---

## 待定事项(需要裁决)

| 事项 | 现状 | 选项 |
|---|---|---|
| bool 类型 | 无,`asBool()` 语义 | A. 保持; B. 加 bool(需改全程) |
| int64 / 毫秒时间 | 32 位 int、秒级 | A. 保持; B. 加 int64Value(改 Value) |
| UTF-8 感知字符串 | 裸字节 | A. 保持; B. 后续引入(破坏兼容) |
| 内存治理 | free 清全局 + 512 上限 | A. 保持; B. 引用计数; C. 真 GC(长期) |
| 字节码保密 | XOR 混淆 | A. 明文声明"混淆"; B. 换真加密(长期) |

---

## 路线图(按优先级)

### 近期(止血,已部分完成)
- [x] 仓库资产入库:`package_plugin/`(fjson/flog)、`dlls/fox_time.cpp`、`libs/system/time/`、`test/test.json`
- [x] `.fc` 二进制移出版本控制并加入 .gitignore(源码即真相)
- [x] README 与代码事实同步(特性表、MIT 许可、构建产物路径),删除空 `docs/FixBug.md`
- [ ] 一键回归:`test/run_tests.bat` 依次跑 `-f/-fc` features/smoke/main 并断言 OK
- [ ] CI:`.github/workflows/build.yml`,Windows runner 上 build + 跑回归断言
- [ ] `foxpkg` 补全:`update`/`list`/`from`,开启 SSL 校验,去掉全局 `aaa` 传参,服务器地址参数化,解压不再依赖外部 `tar`
- [ ] 清理:删除 FFI 空壳目录/文件、空 `FoxRuntime/`、cmake-build 中旧路径(曾用名 FoxLang)构建残留

### 中期
- [ ] bool 类型或明确否决(见待定表)
- [ ] int64 / 毫秒级时间(flog 的毫秒日志依赖于此)
- [ ] 字符串 UTF-8 感知(`str_length`/`str_substring` 按字符)
- [ ] flog:fjson 联动输出 JSON 结构化日志行
- [ ] 日志文件按大小/按天轮转(纯 Fox 可实现)

### 长期
- [ ] 真加密(替代 XOR)或维持混淆定位
- [ ] 内存治理方案(引用计数或 GC)
- [ ] 跨平台(非 Windows 路径、动态库加载方式)
- [ ] FFI:落地 C ABI 调用或删除空壳