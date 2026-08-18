# FoxVast

[LemonStudio](https://lemdev.top) · [Language](https://lemdev.top/fox/)

> **FoxVast** 是一款由 [Lemon Studio](https://lemdev.top) 倾力打造的现代化、强类型命令式脚本编程语言。它抛弃了传统语言臃肿的变量声明与复杂的教条，引入了独创的管道式输入流与指令级终端控制，旨在为极客们提供最纯粹、最直观的开发体验。

**官方网站**：[lemdev.top](https://lemdev.top/fox/) | **标准库文档**：[标准库文档](https://lemdev.top/fox/docs/index.html)

> 状态:活跃开发中。本 README 与代码现状同步;已实现而未文档化的设计取舍见 [docs/ROADMAP.md](docs/ROADMAP.md)。

---

## 语言核心特性

* **自由的运行时赋值**:取消了 `var`、`let`、`const` 等传统关键字的束缚,变量直接赋值即可使用,类型由运行时动态决定。
* **严谨的函数强类型**:函数边界处参数强制通过 `<-` 符号显式类型绑定,返回值强校验(返回类型不符直接报错)。
* **指令级终端交互**:`input() <- variable` 管道式输入流,配合专门用于输出换行的 `end` 指令。
* **完整的流程控制**:`if / else`、`while / for`、`break / continue`,逻辑关键字 `and / or / not`。
* **复合运算**:`+ - * / %` 与全部关系比较(`== != > < >= <=`)齐备。
* **丰富的容器**:数组、dict(字典)、bytes,支持下标读写、数组拼接/排序等原生库能力。
* **class / struct**:字段、方法(隐式 `this`)、构造 `init`、`new ClassName(...)` 实例化。
* **异常处理**:`try { } catch (e) { }` + `error("msg")` 抛出,VM 与解释器双路支持。
* **模块与插件**:`import "file.fox"` 文件合并、`import fox.std.util -> alias` 库别名、`!import fjson` 等插件导入(查找顺序:脚本目录 → `C:\FoxLibs` → 工作目录)。
* **字节码工具链**:`.fox` → `.fc`(varint 压缩、常量去重、XOR 混淆),支持多模块合并 `-fc`、反汇编 `-d`、`.far`(ZIP)打包分发。

---

## 语法速览 (Syntax Overview)

```fox
// 定义一个带有显式类型绑定的函数
func calculate(val <- int, base <- double) -> double {
    if (val > 0) {
        ret double(val) + base
    }
    ret base
}

// dict 与数组
func describe(p <- dict) -> void {
    println("name: " + p["name"])
    println("tags: " + util.IntChangeString(util.arr_length(p["tags"])) + " items")
}

// 主程序入口
func main() -> void {
    print("欢迎使用 Lemon Studio FoxVast!")
    end // 精准控制输出换行

    print("请输入您的验证基数: ")
    input() <- userNum // 管道式接收终端流数据

    res = calculate(int(userNum), 3.14)
    println("计算结果: ")

    // 异常
    try {
        error("boom")
    } catch (e) {
        println("caught: " + e)
    }

    exit(0) // 进程安全退出
}
```

> 注:字符串拼接 `+` 只连接字符串,不自动做 int → string,需要时用 `util.IntChangeString(...)`。

---

## 快速入门 (Quick Start)

### 1. 构建编译器

FoxVast 的解释器与字节码编译器基于 C++ 实现,需要支持 `C++17` 的编译器(如 MinGW-w64 GCC 9+)与 `CMake`。

```bash
git clone https://github.com/zzxlemon/FoxVast.git
cd FoxVast

# 配置并编译(MinGW 环境)
cmake -S FoxCore -B cmake-build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build cmake-build
```

构建产物为 `cmake-build/core/fox.exe` 与 `cmake-build/core/fox.*.dll`(运行时库;库缺失时对应 `fox.<lib>` 命名空间不可用)。

### 2. 运行您的第一个 FoxVast 脚本

`hello.fox`:

```fox
func main() -> void {
    println("Hello, FoxVast!")
    exit(0)
}
```

```bash
./fox.exe -f hello.fox   # 解释运行(旧路径,主要为了调试)
./fox.exe -c hello.fox   # 编译为 hello.fc
./fox.exe -fc hello.fox  # 编译 + 运行(已存在 .fc 则直接运行;改过源码须重新 -c)
```

### 3. 插件与标准库

官方插件 `fjson`(JSON 解析/生成)与 `flog`(日志,支持级别过滤、模板、文件输出、ANSI 颜色与区间高亮)位于 `package_plugin/`。放入 `C:\FoxLibs\` 后即可 `!import fjson` / `!import flog` 使用,详见各插件目录的 README。

---

## 语法规则简表

| 语法要素 | 表达式/关键字示例 | 说明 |
| :--- | :--- | :--- |
| **原生类型** | `int`, `double`, `string`, `void` | 另有 `array`、`dict`、`bytes`、`object`(class 实例) |
| **基础赋值** | `x = 10`, `name = "fox"` | 无需显式声明,直接赋值使用;`c <- dict` 是函数参数的强类型绑定 |
| **类型转换** | `int(expr)`, `double(expr)` | 内置转换函数,另有 util 库 6 个转换函数 |
| **算术与逻辑** | `+ - * / %`, `and`, `or`, `not` | 乘、除、取模已实现;字符串不支持条件求值 |
| **关系比较** | `==`, `!=`, `>`, `<`, `>=`, `<=` | 支持复合布尔表达式求值 |
| **控制流** | `if (cond) { ... } else { ... }` | `else` 已实现 |
| **循环流** | `while (cond) { ... }`, `for (;;) { ... }` | `break` / `continue` 已实现 |
| **函数** | `func f(p <- int) -> int { ret p }` | 参数显式类型绑定,返回类型强校验,void 函数不可返回值 |
| **类与结构** | `class Point { x <- int; init(...) {...} }` | 字段类型限 `int/double/string/dict/bytes`;方法首参为隐式 `this` |
| **异常** | `try { } catch (e) { }`, `error("msg")` | `error()` 抛 LangError,由最近的 catch 捕获 |
| **内存管理** | `free name`, `free_all` | 清除全局变量(无 GC;详见 ROADMAP) |
| **导入** | `import "file.fox"`, `import fox.std.util -> alias`, `!import fjson` | 文件合并(带环检测)/ 库别名 / 插件 |
| **返回与退出** | `ret expr`, `exit(code)` | `ret` 函数返回;`exit` 终止程序,非 0 为错误码 |

---

## 命令行工具

| 参数 | 作用 |
|---|---|
| `-f <file.fox>` | 解释运行(旧路径)。官方推荐编译运行 |
| `-c <file.fox>` | 编译为 `.fc`;对入口文件会内联展开 `!import`/`import`,生成自包含字节码 |
| `-fc <file.fox>` | 编译 + 运行;若同目录 `.fc` 已存在直接加载运行(改过源码须重新 `-c`) |
| `-d <file.fc>` | 反汇编字节码 |
| `-p` / `-u` / `-far` | `.far` 打包 / 解包 / 运行(ZIP 格式单文件分发) |
| `pack=true` / `pack=false` | `.fx`/`.fz` 混淆打包开关(XOR 混淆,非加密,见 ROADMAP) |
| `-s utf-8 / gbk` | 切换控制台代码页 |
| `-version` | 版本信息 |

---

## 标准库与插件

| 库 | 命名空间 | 用途 |
|---|---|---|
| `fox.std.util` | `util.` | 数组(length/append/pop/contains/slice/sort)、字符串(contains/replace/split/trim/lower/upper/substring/length/拼接转换) |
| `fox.sys.io.fs` | `file.` | `file_open/read/write/close/remove` |
| `fox.sys.time` | `time.` | `now` / `format`(strftime)/ `field`(秒级) |
| `fox.std.math` | `math.` | `sin / cos / tan` |
| `fox.std.random` | `random.` | `random("int", min, max)` |
| `fox.std.socket` | `socket.` | TCP socket:create/connect/send/recv/close |
| `fox.gl.fg` | `fg.` | GLFW 窗口与 2D 绘制:window/clear/draw_triangle/draw_rect/draw_text/draw_line/draw_circle/mouse |
| `package_plugin/fjson` | `fjson` | JSON 解析/生成(纯 Fox 插件,见其 README) |
| `package_plugin/flog` | `flog` | 日志(纯 Fox 插件,见其 README / colors.md) |

运行时库按需动态加载:`fox.exe` 旁有 `fox.<name>.dll` 即注册对应命名空间;库缺失不影响其他功能。

---

## 仓库目录结构

```text
FoxVast/
├── FoxCore/                # 语言实现本体
│   ├── src/                # frontend(lexer/parser/ast) / interpreter / vm / util
│   ├── libs/               # 原生库(system: io/fs/time/math/random/socket + graphics)
│   ├── native/             # 第三方依赖(glfw / glad / stb_truetype / 字体)
│   └── CMakeLists.txt      # CMake 构建配置
├── dlls/                   # 运行时库 DLL 薄壳入口(fox.<name>.dll)
├── Package/                # 包管理器 foxpkg(install/uninstall;update/list/from 待实现)
├── package_plugin/         # 纯 Fox 插件:fjson、flog
├── test/                   # 示例与回归(main / smoke / features / snake / hangman)
├── docs/                   # 设计决策与路线图
└── README.md
```

---

## 参与贡献 (Contributing)

1. Fork 本仓库
2. 创建您的特性分支 (`git checkout -b feature/AmazingFeature`)
3. 提交您的修改 (`git commit -m 'Add some AmazingFeature'`)
4. 推送到分支 (`git push origin feature/AmazingFeature`)
5. 在本仓库提交一个 **Pull Request**

计划中的工作(CI 落地、foxpkg 补全、bool/int64、UTF-8 等)见 [docs/ROADMAP.md](docs/ROADMAP.md),欢迎认领。

---

## 开源许可证

本项目基于 **MIT** 许可证开源 - 详情请参阅 [LICENSE](LICENSE) 文件。

© 2026 **[Lemon Studio](https://lemdev.top)**. 保留所有权利。

**(此信息在未来的版本变更可能会有所变化)**