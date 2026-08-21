# FoxVast VM 优化全景图

> 基准:字节码 VM 约 14M 指令/秒,单指令约 72ns;**fib(24) 初始约 290ms**(15 万次调用,每次约 1.9us);50M 循环 bench 约 55s。
> **第一梯队实施后:fib(24) 约 110ms**,单次调用约 0.73us。

## 一、第一梯队(已完成,约 2.6 倍)

### 1. Value 瘦身:190 字节 -> union Storage(已实施)

**现状**:`Value` 结构体约 190B,内含 `std::string`(32B)、`std::vector<Value>`(24B)、`std::unordered_map<std::string,GcHandle>`(约 64B)、`std::string objectClassName`(32B) 等成员,栈上每一句指令 push/pop 都在复制整个结构体,是单指令 72ns 的主要来源。

**方案**:
- 改为 tagged union `Storage`:标量(int/double/枚举/coroId)直接内嵌,容器(string/array/bytes)原地构造,dict 用 `unordered_map*` 指针(原 `std::string` 键值对已 bag-of-roots 托管),object 类名改 `std::string className` 内嵌。
- 拷贝/移动/析构由 `copyStorage/destroyStorage` 显式管理,`asDict()` 返回 `*st_.dict` 解引用,GC trace 与 toString 全部走 st_ 成员。

**实施结果**:fib(24) 从 290ms -> **179ms**(1.6x)。全量回归绿(coro_basic/coro_gc/gc_stress2/gc_class/gc_selfref/gc_selfref2/try_catch/features/smoke)。

### 2. 调用路径优化:参数全局槽预绑定,热路径零 hash(已实施)

**现状**:每次函数调用都要为每个参数做 `globals.find(name)` + `getOrCreateSlot` + `savedGlobals`/`newGlobals` 影子记录 + 全局槽写入 + 映射表写入;fib 单次调用 1.9us 大部分耗在调用里。协程 resume 段做同样的事。

**方案**:
- `paramSlots_` 按函数指针惰性缓存参数->全局槽位映射(首次调用时构建,此后零 hash)。
- 绑定/还原退化为槽位 vector 读写;`CallFrame` 内置 4 槽小数组,<=4 参数零堆分配;还原用帧内缓存的槽位指针,无查找。
- `names` 映射表热路径完全不碰,由 `flushGlobals()`(库调用/协程边界)同步。
- `dropGlobal` 改为保留槽位(值置 Void)而非删除,预绑定槽位与常量->槽位缓存全程有效;递归、协程多实例语义由逐帧 saved 值保证。

**实施结果**:fib(24) 从 179ms -> **119ms**(1.5x)。全量回归绿。

### 3. Threaded dispatch:superinstruction 折叠(部分实施)

**现状**:54 个 opcode 的巨型 switch,每句指令都要跑一轮分支;switch 本体 850 行,icache 压力大。

**方案**:
- ~~computed goto~~:评估后放弃——switch 体内有 123 处 `break;`,computed goto 改写需逐一改为 `goto dispatch`,风险/收益比不划算(跳转表 dispatch 本身已是 O(1),收益主要在分支预测与 icache)。
- **superinstruction(已实施)**:编译期把右操作数为 [-128,127] 小整数字面量的二元运算/比较内嵌为单指令:

| 新增 opcode | 语义 | 折叠条件 |
|---|---|---|
| OP_ADD_IMM ... OP_NE_IMM(共 11 个) | 栈顶值与 int8 操作数运算,结果压栈 | RHS 为 NumberExpr 且值在 int8 范围 |

- 免去 OP_CONSTANT 的取指 + 边界检查 + 入栈 + 出栈往返;语义与通用版本逐分支一致(含除零、类型不匹配报错、double 提升比较)。
- 反汇编器两遍扫描均已适配 IMM 指令长度。

**实施结果**:fib(24) 从 119ms -> **~110ms**(约 5-8%,指令数下降)。IMM 语义专项测试 + div0 + 全量回归绿。

**剩余**:computed goto / 栈指针缓存可作后续实验,预计再降 10-20%;bin 2 字节索引、`OP_GET_GLOBAL+OP_CONSTANT` 固定组合折叠可继续做。

## 二、第二梯队(预计 1.2-2 倍)

### 4. 字节码优化 pass(常量折叠 + 死代码删除)

**现状**:`s = s + i % 7` 等循环内每次重复取全局与取模;编译器没有任何优化 pass。

**方案**:编译结束后对每个函数的字节码跑一遍优化 pass:
- 常量折叠:`CONST 5 + CONST 3` -> `CONST 8`;`i % 1` -> `i`(注意:语义上要先定位,折叠要保守)。
- 条件跳转与无条件跳转合并;不可达代码(跳转目标之间的 dead block)删除。
- 循环不变量移动(IC):`while` 内的 `10 * id` 若 id 循环内不变,提到循环外。

**预期**:合成循环 2-5 倍,真实脚本 1.2-1.5 倍。**成本**:中(不改 VM 执行循环,只加 pass;需要反汇编 + 全量回归)。

### 5. 调用目标预解析

**现状**:`OP_CALL` 每次调用时对函数名做字符串 hash 查找,`callSystemFunction` 走的是宽泛字符串分发。

**方案**:load 时把函数名字符串索引编译为函数指针(或系统调用 id),`OP_CALL` 直接携带;找不到的才在运行时查找。`co.create("fn")` 同理。

**预期**:调用密集型脚本 10-20%。**成本**:低(可与 #2 合并实施)。

### 6. 修复 LTO

**现状**:MinGW g++ 当前版本搭 `-flto` 链接报错(collect2),`-pg` 与 gprof 也被 DWARF 炸掉;跨 TU 内联缺失,`Gc::instance()`、push/pop 每次都是真调用。

**方案**:换 clang-cl 或更新版 MinGW-w64 启用 LTO;或手动把调用热点链路挪进同一 TU(bytecode.cpp 已经很大,可做成 header-only inline)。

**预期**:5-15%。**成本**:低。

## 三、第三梯队(收益不确定)

### 7. Chunk 布局优化
把代码与常量池分开独立存放(数组分离,gcc 友好对齐),指令直接引用常量指针,减少取指/取常量的 cache miss。预计 5-10%。

### 8. GC 世代优化
nursery/survivor 分区:bump 分配 + 快速回收,替代 mark-sweep。当前 GC 压力占比实测 <5%,先堆更大的再考虑。收益:堆大时 2-3 倍分配速度。

### 9. 控制台输出合并
`println` 直写控制台;脚本密集输出时可攒一个/半个缓冲 flush(注意交互式程序需要及时输出,可做成可选开关)。收益:输出密集型脚本明显。

### 10. dict 实现优化
内部 `unordered_map<string, GcHandle>`:短键(<8 字符)字符串视图 + 哈希,改 flat hash;脚本字典频繁读写(GC 压力不大)值得做,收益中等,难度中。

### 11. 模块加载与反序列化
`.fc` 反序列化(含 v3 delta 编码)每次运行都做一次,可考虑缓存/惰性;`-far` 加载 + 多模块 loadProgram 可并行。收益:启动时间,通常 <50ms,除非工程很大。

## 四、结构性重构(为未来优化铺路)

- **淘汰 deprecated 解释器路径**(`-f`):旧树遍历执行器与 VM 并存,任何 VM 优化都要同步维护一份;建议冻期并逐步移除,至少标注 deprecated。注意:`co.*` 协程仅 VM 支持,`-f` 直接报 Undefined function,回归测试必须以 `-c` + `-fc` 方式跑。
- **性能回归基准**:建议用 `test/runtime_err/bench.fox` 建基准集:指令计数统计(VM 内置埋点,`-fc` 方式跑)、3 次取中位数、自动记账 fib(24) 基准值,每次优化前后对比,防止回退。
- **`-s gbk` 默认化**:项目源码已转 GBK,考虑 chcp 936 默认化,免去每次带 `-s gbk` 的摩擦。

## 五、考察过不做

- **每指令 checkpoint 频率**:GC tick 计数,开销可忽略,不需要动。
- **global 哈希池优化**:实测不是瓶颈,槽位优化后更不是。
- **删除 `-f` 路径**:维护成本高但与性能无关,暂保留。
- **多线程/并发 GC**:VM 单线程模型,投入产出比低。

## 六、执行顺序记录

1. ~~#1 Value 瘦身~~ 已完成:290ms -> 179ms。
2. ~~#2 调用路径~~ 已完成:179ms -> 119ms。
3. ~~#3 threaded dispatch(subset)~~ 已完成 IMM 折叠:119ms -> ~110ms;computed goto 放弃,栈指针缓存待议。
4. 下一步:#4 字节码优化 pass(常量折叠 + dead code)或 #5 调用目标预解析。
5. #6-#11 其余按收益取舍。