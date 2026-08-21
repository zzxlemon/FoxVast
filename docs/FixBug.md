# FixBug 记录

> 记录发现与修复的 Bug。已修复的保留现场描述供回归参考;未修复的标记为 [未修复]。

## 1. [已修复] 同一库双导入时别名丢失

- **位置**:`FoxCore/src/interpreter/interpreter.cpp` 与 `FoxCore/src/vm/bytecode.cpp` 的导入循环
  (`if (!visited.insert(path).second) continue;` 按路径去重)
- **现象**:同一脚本里同时写 `!import flog` 和 `!import flog -> f`,`f.*` 别名函数不注册:
  - 解释器 `-f`:`Undefined function: f.flog_info`
  - 字节码 `-fc`:`Undefined variable: f`
- **原因**:`visited` 集合只按文件路径去重,第二次导入(带别名)被整条跳过,别名副本从未注册
- **复现**:
  ```
  !import flog
  !import flog -> f
  func main() -> void {
      c = flog.flog_default_config()
      f.flog_info(c, "dual import test")   // 修复前报错
  }
  ```
- **修复**(2026-08-21):
  - parseInto 记录每个文件的裸函数名表(`bareNamesByPath`)与嵌套导入范围(`nestedByPath`)
  - 循环对重复路径不再直接跳过:记入 `pendingAliases`,等**全部文件解析完**后统一注册
    `alias.name` 副本(重复条目通常排在嵌套导入之前,若立即注册会因嵌套文件未解析而失败)
  - 别名沿嵌套导入递归传播(`regAlias`),带 `seen` 集合防环
  - 已验证:`!import flog` + `!import flog -> f` 双向顺序在 `-f` 与 `-fc` 下均通过

## 2. [已修复] far 打包 Linux 分支 DOS 日期差 20 年

- **位置**:`FoxCore/src/vm/far.cpp` `dos_datetime()` 的 `#else`(Linux)分支
- **原因**:DOS 日期用「距 1980 的年数」,`tm_year` 是「距 1900 的年数」,原代码直接 `(t->tm_year) << 9`,
  比真实日期偏大 20 年
- **修复**:改为 `(t->tm_year - 80) << 9`

## 3. [已修复] far 打包 Linux 分支 mkdir 参数不足

- **位置**:`FoxCore/src/vm/far.cpp`(Linux 编译报错)
- **原因**:POSIX `mkdir()` 需要权限参数,Windows `_mkdir()` 只要 1 参;原 `#define mkdir _mkdir`
  在 Linux 下展开后调用 `mkdir(path)` 只有 1 参,编译失败
- **修复**:引入 `FOX_MKDIR(p)` 宏,Windows 展开 `_mkdir(p)`,Linux 展开 `mkdir(p, 0755)`

## 4. [已修复] 别名注册在遍历 unordered_map 时插入(未定义行为)

- **位置**:`interpreter.cpp` / `bytecode.cpp` 的 parseInto 别名复制循环
- **原因**:`for (const auto& [funcName, func] : fileFuncs)` 循环体内直接
  `fileFuncs[alias + "." + ...] = std::move(copy)` 插入新元素,rehash 使迭代器失效,
  产生脏键(实测出现过 `f.g_error`、重复键)
- **修复**:先收集到独立的 `aliases` map,循环结束后统一 `insert`

## 5. [已修复] flog 库函数定义误加命名空间前缀

- **位置**:`package_plugin/flog/flog_output.fox`(与安装副本 `C:\FoxLibs\flog_output.fox`)
- **原因**:`func flog._flog_file_write(...)` 定义行带了 `flog.` 前缀(其余文件都是裸名定义),
  经 parseInto 前缀化后注册成 `flog.flog._flog_file_write`,调用 `flog._flog_file_write` 找不到
- **现象**:demo.fox 第 5 步「写入文件」报 `Undefined function: flog._flog_file_write`
- **修复**:定义行改回裸名 `func _flog_file_write(...)`

## 6. [已修复] CMake:Linux 下 .so 输出目录错误

- **位置**:`dlls/CMakeLists.txt`
- **原因**:Windows DLL 由 `RUNTIME_OUTPUT_DIRECTORY` 决定,Linux .so 走 `LIBRARY_OUTPUT_DIRECTORY`,
  未设置时产物落在 `build/dlls/` 而不是 `build/core/`,导致 fox.exe 启动时找不到库
- **修复**:三个目标(socket / graphics / fox_dll 宏)都补上 `LIBRARY_OUTPUT_DIRECTORY core`

## 7. [已修复] Windows 构建默认不是 Release

- **位置**:`FoxCore/build.bat`(Windows)
- **原因**:CMakeCache 中 `CMAKE_BUILD_TYPE` 为空字符串,缓存值覆盖 CMakeLists 里的默认 Release,
  实际编译无优化
- **修复**:build.bat 显式传 `-DCMAKE_BUILD_TYPE=Release`

## 8. [已知限制] 编码差异:Windows GBK vs Linux UTF-8

- Windows 树(`windows/`)源码与 `.fox` 脚本为 **GBK**,Linux 树(`linux/`)为 **UTF-8**
- `.fc` 字节码里字符串是原始字节,在另一平台运行会出现中文乱码;
  直接跨平台拷贝 `.fox` 源码同样会乱
- **约定**:保持两树独立,Windows 脚本用 GBK 保存,Linux 脚本用 UTF-8 保存;
  不要跨平台复用 `.fc`/`.fox`

## 9. [已知限制] Linux 图形库缺失平台功能

- `register_hotkeys`(全局热键)与 `simulate_real_click`(OS 级真实点击)仅 Windows 实现,
  Linux 上调用会抛 `RuntimeError: not supported on this platform`(设计如此,非崩溃)
- Linux 图形需要系统安装 GLFW(`libglfw3-dev`),未安装时 `fox.graphics.so` 不生成

## 10. [已知限制] 解释器模式(-f)已废弃

- `-f` 走 AST 解释器,功能滞后于字节码 VM(协程仅 `-fc` 支持,GC 依赖 checkpoint),
  官方推荐 `-c` + `-fc` 流程
