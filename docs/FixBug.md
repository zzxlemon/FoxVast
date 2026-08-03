# FixBug.md — FoxLang Bug 清单（分级）

> 审查范围：FoxCore 全源码（前端、VM、解释器、标准库、CLI）。
> 所有条目均经过源码定位 + 实际运行验证（`fox -c` / `-fc` / `-f`）。
> 分级定义：
> - **P0 严重**：主流程（-fc 字节码路径）产生错误结果 / 崩溃 / 内存失控，或语言特性完全不可用。
> - **P1 高**：错误结果或崩溃但影响面较小 / 需要特定条件。
> - **P2 中**：功能缺陷、语义不一致、命令不可用。
> - **P3 低**：健壮性 / 安全 / 风格问题。

---

## P0 严重

### P0-1. VM 操作数栈泄漏：独立函数调用语句不弹栈（-fc 主路径）
- **位置**：`FoxCore/src/vm/bytecode.cpp` — `BytecodeStmtHandler::onCall`（第 457-469 行）；VM `OP_CALL`（第 1133-1187 行）
- **现象**：`OP_POP` 从未被编译器发射。语句 `foo()` 编译为 `OP_CALL` 后，函数返回值永远留在栈上：
  1. 循环内裸调用（`while (1) { noop() }`）栈无限增长 → 内存失控；
  2. 函数体内最后一条语句是裸调用、且函数无显式 `ret` 时，末尾隐式 `OP_RETURN` 会把栈顶的"垃圾值"当作函数返回值。
- **复现**（`test_wrongret.fox`）：
  ```
  func helper() -> int { ret 5 }
  func myfunc() -> int {
      helper()
      println("inside myfunc")
  }
  func main() -> void {
      x = myfunc()   # myfunc 从未执行 ret，却"返回"了 5
      println(x)     # 打印 5，本应报错
  }
  ```
  `-fc` 输出 `inside myfunc` 与 `5`；解释器路径同样报错位置错误。
- **修复建议**：语句级调用后发射 `OP_POP`；函数末尾统一补 `OP_RETURN`（见 P0-2）。

### P0-2. 隐式 return 检测失效：`code.back()` 与操作数混淆（-fc）
- **位置**：`FoxCore/src/vm/bytecode.cpp` 第 396 行
  ```cpp
  if (cf.chunk.code.empty() || cf.chunk.code.back() != static_cast<uint8_t>(OpCode::OP_RETURN)) {
  ```
- **现象**：`OP_CALL` 的操作数字节是 `0x00`，与 `OP_RETURN` 的 opcode 相同。函数最后一条语句是"0 参调用"时，`code.back()==0x00` 误判为"已有 return"，不再补 `OP_RETURN`，函数落入 `cf.ip >= code.size()` 分支返回一个伪造的 Void 值。
- **复现**（`test_leak2.fox`）：`func f() -> void { retVal() }` → `x = f()` 得到 Void，`println(x)` 无输出（期望报错或返回 7）。
- **修复建议**：检测函数末尾是否有**指令** `OP_RETURN`，而非最后一个字节；或编译结束时无条件补 `OP_RETURN`。

### P0-3. `ret <表达式>` 在解释器路径永远返回 Void（-f）
- **位置**：`FoxCore/src/frontend/ast.cpp:548`（声明 `ast.hpp:219`）
  ```cpp
  RetStmt::RetStmt(std::unique_ptr<Expr> a) : arg(std::move(a)), hasArg(a != nullptr) {}
  ```
- **现象**：初始化列表按声明顺序执行：先 `arg = std::move(a)`，此时 `a` 已变空，`hasArg` 恒为 `false`。`ret 5` 与 `ret` 等价，返回值恒为 Void。任何带返回值的函数在 `-f` 下都报 `Function X expects to return int type, actually returned void`。
- **复现**：`func helper() -> int { ret 5 }`，`x = helper()` → 报错（`-fc` 正常输出 5）。
- **修复建议**：`: arg(std::move(a)), hasArg(arg != nullptr)` 或 `hasArg(static_cast<bool>(a))` 放在 move 之前求值。

### P0-4. `for` 循环完全无法使用（-c 与 -f 均失败）
- **位置**：`FoxCore/src/frontend/parser.cpp` — `parseSingleStatement` 第 285-319 行（if/while/for 分支）
- **现象**：该分支在 `insideBlock` 为假时把 `;` 当作语句分隔符截断。`for (i = 0; i < 5; i = i + 1) { ... }` 中第一个分号（位于 `{` 之前）导致捕获到 `for ( i = 0 ;` 就截断，随后编译报 `Expected token type 48, got 11`。
- **复现**：任意 `for` 循环（`test_for2.fox`）在 `-c` 阶段直接失败。while/if 无分号所以不受影响。
- **修复建议**：for 语句的分号应计入括号深度后再作分隔判断（仅 `!insideBlock && parenDepth==0` 时才截断），或直接按行读取。

---

## P1 高

### P1-1. 函数参数泄漏为全局变量（-fc）
- **位置**：`FoxCore/src/vm/bytecode.cpp` — `OP_CALL` 第 1172-1179 行；`OP_RETURN` 第 839-844 行
- **现象**：参数写入 `globals`，仅当调用前该名字已存在时恢复；全新名字在函数返回后残留。调用后 `println(a)` 能读到参数值。
- **复现**：`func add(a <- int, b <- int) -> int { ret a + b }` → `c = add(3,4)` 后 `println(a)` 打印 `3`（应为"Undefined variable: a"）。
- **修复建议**：进入函数时记录全部参数旧值（包括不存在的情况），返回时统一恢复/删除。

### P1-2. `math.sin/cos/tan` 传入 int 参数直接运行时报错
- **位置**：`FoxCore/libs/system/math/SystemFunctionsMath.cpp:14`（sin/cos/tan 相同）
- **现象**：类型检查明确接受 int/double，但随后调用 `v.asDouble()` —— 对 Int 类型 `asDouble()` 会抛 `Value is not a double type`。
- **复现**：`import fox.std.math` 后 `math.sin(0)` → `RuntimeError: Value is not a double type`。
- **修复建议**：`(v.getType() == Int) ? (double)v.asInt() : v.asDouble()`。

---

## P2 中

### P2-1. 字符串运算在解释器（-f）与字节码 VM（-fc）语义不一致
- **位置**：`FoxCore/src/frontend/ast.cpp` — `BinaryExpr::evaluate`（第 139-165 行，无 String 分支）、`CompareExpr::evaluate`（第 249-282 行，仅 int/double）
- **现象**：VM 的 `OP_ADD`/`OP_EQ` 支持字符串，解释器不支持。
  - `"a" + "b"` → -f 报 `Operation error: type mismatch`；-fc 正常拼接。
  - `s == "q"`（snake.fox 使用）→ -f 报 `Comparison error`；-fc 正常。
- **修复建议**：解释器路径补充 String 类型的 `+`/`==`/`!=` 分支，与 VM 对齐。

### P2-2. 解释器静默吞掉函数体内的语法错误
- **位置**：`FoxCore/src/frontend/parser.cpp` 第 438-443 行
  ```cpp
  try { func.compiledBody.push_back(std::move(parseLineToStmt(line))); }
  catch (...) { func.compiledBody.push_back(nullptr); }
  ```
- **现象**：函数体任意行的语法错误被转为 nullptr 静默跳过，-f 运行无任何报错继续执行；同样的代码 -c 会正常报 ParseError。类型声明行（`int x = 5`）也被静默丢弃，导致解释器与字节码行为不一致。
- **复现**：`y = = 6` 在 -f 下无任何提示，程序继续输出 `5` 并以 0 退出。
- **修复建议**：收集错误信息并在执行前报告，或与编译器路径统一抛出。

### P2-3. `-s` / `--str` 参数永远无法匹配
- **位置**：`FoxCore/src/main.cpp` 第 64-65 行
  ```cpp
  else if (argv[i] == "-s" || argv[i] == "--str"){          // 裸指针比较
      if (argv[i + 1] == "utf-8" || argv[i + 1] == "UTF-8"){ // 裸指针比较
  ```
- **现象**：`argv[i] == "-s"` 是比较指针地址而非字符串内容，恒为 false；内层同理。`-s utf-8` 直接报 `unknown parameter '-s'`。
- **复现**：`fox -s utf-8 -version` → `ERROR[ArgumentError]: unknown parameter '-s'`。
- **修复建议**：改用 `std::string(argv[i]) == "-s"` 或 strcmp；同时补 `i+1` 越界检查。

### P2-4. `for` 空条件语义不一致
- **位置**：`FoxCore/src/frontend/ast.cpp` — `ForStmt::execute`（第 703-731 行）vs `FoxCore/src/vm/bytecode.cpp` — `onFor`（第 528-562 行）
- **现象**：`for (i = 0; ; i++)` 解释器立即 break；字节码编译为 `OP_TRUE` 无限循环（C 语义）。
- **修复建议**：解释器与编译器统一为"空条件 = 无限循环"。

---

## P3 低

### P3-1. `Chunk::serialize` 序列化数组常量丢失元素
- **位置**：`FoxCore/src/vm/bytecode.cpp` 第 111-115 行（serialize）与第 172-180 行（deserialize）
- **现象**：Array 常量只写入 `size`，不写元素；反序列化得到全空 Value 数组。当前编译器不会把数组字面量放入常量表，暂不触发，但序列化格式存在隐患。

### P3-2. `.far` 解包路径穿越（Zip-slip）
- **位置**：`FoxCore/src/vm/far.cpp` — `extractAll` 第 283-308 行
- **现象**：`outputDir + "/" + entry.name` 未校验 entry 名，恶意 `../` 或绝对路径条目可写出输出目录。

### P3-3. `farRun` 对损坏/恶意 `.far` 缺少边界检查
- **位置**：`FoxCore/src/vm/far.cpp` 第 412-439 行
- **现象**：`local_off + 30` 与 `data_start + comp_size` 均未校验，构造的存档可造成越界读。

### P3-4. double 打印格式不一致
- **位置**：VM `OP_PRINT` 用 `Value::toString()`（`std::to_string`，输出 `3.000000`）；解释器 `std::cout << asDouble()`（输出 `3`）。
- **现象**：同一程序 -f 与 -fc 输出不一致。

### P3-5. `new()` 内存限制只在解释器 `onAssign` 生效
- **位置**：`FoxCore/src/frontend/parser.cpp` 第 723-726 行；`ast.cpp` `AssignStmt::execute` 第 626-629 行（已被注释掉）
- **现象**：`g_funcNewAllocBytes` 限制在字节码路径和 `AssignStmt` 中不生效，两条执行路径行为不一致。

### P3-6. `Interpreter::parse_failed` 一旦置位永不重置
- **位置**：`FoxCore/src/interpreter/interpreter.cpp` 第 14-24 行
- **现象**：单次解析失败后 `runMainFunc` 永远提前返回，复用实例无法恢复。

### P3-7. 死代码：解析器 dot 分支不可达
- **位置**：`FoxCore/src/frontend/lexer.cpp` 第 75 行（`readIdentifier` 允许 `.`）；`parser.cpp` 第 134-138、622-645 行
- **现象**：`a.b` 被词法合并为单个标识符，`TOKEN_DOT` 分支永远不会被触发（字符串后的 `.` 除外），库调用依赖合并行为。如后续要支持成员访问需先修复词法。

### P3-8. 解释器返回类型检查报错函数名误导
- **位置**：`FoxCore/src/interpreter/interpreter.cpp` 第 72-83 行
- **现象**：`ret` 失效（P0-3）时错误信息指向被调函数（如 `helper`）而非未返回值的函数本体；P0-3 修复后建议复核报错逻辑与"void 函数带值返回不检查"的问题。

---

## 验证环境

- 构建：`cmake --build cmake-build`（MinGW，Release）
- 复现用脚本位于 `%TEMP%\opencode\`（test_leak2/test_wrongret/test_param/test_math/test_for2/test_strcat/test_silent/test_simple 等）
- 基线：`test/smoke.fox` 与 `test/snake.fox`（-fc）通过
