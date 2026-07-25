# FoxLang 

[LemonStudio](https://lemdev.top) · [Language](https://lemdev.top/fox/)

> **FoxLang** 是一款由 [Lemon Studio](https://lemdev.top) 倾力打造的现代化、强类型命令式脚本编程语言。它抛弃了传统语言臃肿的变量声明与复杂的教条，引入了独创的管道式输入流与指令级终端控制，旨在为极客们提供最纯粹、最直观的开发体验。

**官方网站**：[lemdev.top](https://lemdev.top) | **标准库文档**：[标准库文档](https://lemdev.top/fox/docs/index.html)

---

## 语言核心特性

*  **自由的运行时赋值**：取消了 `var`、`let`、`const` 等传统关键字的束缚，变量直接赋值即可使用，类型由运行时动态决定。
*  **严谨的函数强类型**：在函数边界处理上毫不妥协，参数强制通过 `<-` 符号进行显式类型绑定，支持 `int`、`double`、`string`、`void` 四大核心原生类型。
*  **指令级终端交互**：独创的 `input() <- variable` 管道式输入流，配合专门用于输出换行的 `end` 指令，让命令行脚本编写顺畅自然。
*  **清爽的流程控制**：支持标准的 `if` 与 `while` 控制块，配合经典的 `and` 与 `or` 逻辑关键字，消除了易错的隐式分支，逻辑清晰见底。

---

## 语法速览 (Syntax Overview) 

下面展示了一个包含 FoxLang 核心语法要素（类型转换、复合运算、管道输入、指令级换行、函数定义与返回）的完整示例：

```fox
import sys
import math

// 定义一个带有显式类型绑定的函数
func calculate(val <- int, base <- double) -> double {
    if (val > 0) {
        ret double(val) + base
    }
    ret base
}

// 主程序入口
func main() -> void {
    print("欢迎使用 Lemon Studio FoxLang!")
    end // 精准控制输出换行
    
    print("请输入您的验证基数: ")
    input() <- userNum // 管道式接收终端流数据
    
    // 显式类型转换与函数调用
    res = calculate(int(userNum), 3.14)
    
    print("计算结果: ")
    print(res)
    end
    
    exit(0) // 进程安全退出
}
```

---

## 快速入门 (Quick Start)

### 1. 构建编译器
FoxLang 的核心解释器与编译器后端基于 C++ 开发。请确保您的系统已安装支持 `C++17` 标准的编译器（如 GCC 9+、Clang 或 MSVC）以及 `CMake`。

```bash
# 克隆仓库
git clone https://github.com
cd FoxLang

# 创建构建目录并编译
mkdir build && cd build
cmake ..
cmake --build .
```

### 2. 运行您的第一个 FoxLang 脚本
在可执行文件同级目录下新建一个 `hello.fox` 文件：

```fox
print("Hello, FoxLang!")
end
```

使用编译生成的编译器运行它：
```bash
./fox.exe -c hello.fox
./fox.exe -fc hello.fox
```

---

## 语法规则简表 (Language Specifications)

为了方便您快速了解 FoxLang 的设计边界，以下是基于核心词法/语法树分析器整理的规则简表：

| 语法要素 | 表达式/关键字示例 | 说明 |
| :--- | :--- | :--- |
| **原生类型** | `int`, `double`, `string`, `void` | 仅支持这四种核心类型 |
| **基础赋值** | `x = 10`, `name = "fox"` | 无需显式声明，直接赋值使用 |
| **类型转换** | `int(expr)`, `double(expr)` | 内置强类型相互显式转换函数 |
| **算术与逻辑**| `+`, `-`, `and`, `or` | 目前暂未引入乘法、除法与取模运算 |
| **关系比较** | `==`, `!=`, `>`, `<`, `>=`, `<=` | 支持复合布尔表达式求值 |
| **控制流** | `if (cond) { ... }` | ?? 目前语法树中无 `else` 分支结构 |
| **循环流** | `while (cond) { ... }` | 条件必须包裹在括号 `()` 内 |
| **返回与退出**| `ret expr`, `exit(code)` | `ret` 用于函数返回；`exit` 终止程序 |

---

## 仓库目录结构

```text
FoxLang/
├── src/
│   ├── lexer.hpp / .cpp     # 词法分析器（Token 提取与识别）
│   ├── parser.hpp / .cpp    # 语法分析器（AST 抽象语法树构建）
│   ├── token.hpp            # Token 类型定义与关键字映射
│   ├── ast.hpp              # 语法树节点结构定义
|   └── ...
├── libs/                    # 官方标准库代码
└── README.md                # 本说明文件
```

---

## 参与贡献 (Contributing)

FoxLang 是一个完全由开源极客驱动、社区共建的项目。如果您对编译原理、词法解析优化或者标准库扩展感兴趣，我们由衷欢迎您的加入！

1. Fork 本仓库
2. 创建您的特性分支 (`git checkout -b feature/AmazingFeature`)
3. 提交您的修改 (`git commit -m 'Add some AmazingFeature'`)
4. 推送到分支 (`git push origin feature/AmazingFeature`)
5. 在本仓库提交一个 **Pull Request**

---

## 开源许可证

本项目基于 **Apache-2.0** 许可证开源 - 详情请参阅 [LICENSE](LICENSE) 文件。

 2026 **[Lemon Studio](https://lemdev.top)**. 保留所有权利。

**(此信息在未来的版本变更可能会有所变化)**
