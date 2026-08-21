# flog — FoxVast 的日志插件模块

纯 FoxVast 实现的日志库,支持级别过滤、自定义输出模板(strftime 时间格式)、console / file / both 输出与任意目标文件落盘。仅依赖三个核心原生库:`fox.std.util`、`fox.sys.time`(fox.time.dll)、`fox.sys.io.fs`。

## 模块结构

| 文件 | 职责 |
|---|---|
| `flog.fox` | 聚合入口,仅 import 其余四模块 |
| `flog_cfg.fox` | 默认配置、`flog_level_num` / `flog_level_name`、全部 `flog_set_*` setter |
| `flog_render.fox` | `flog_render`(配置 + 级别 + 消息 → 格式化一行)、占位符替换 |
| `flog_output.fox` | 底层落盘与分发:`_flog_file_write` / `_flog_emit` / `_flog_emit_to`、文件工具函数 |
| `flog_api.fox` | 公共 API:`flog_log` / `flog_render_line` / `flog_log_to` / 级别便捷函数 |

## 使用

脚本里 `!import flog`(`!import` 查找顺序:脚本所在目录 → `C:\FoxLibs` → 当前工作目录,裸名自动补 `.fox`)。运行期需要 `fox.time.dll` 在 fox.exe 旁(或库路径)可用。

```fox
!import flog

func main() -> void {
    c = flog_default_config()              # level=0(全量), console 输出
    flog_debug(c, "starting up")
    flog_info(c, "config loaded")

    c2 = flog_set_level(c, 2)              # 升到 warn,debug/info 被过滤
    flog_warn(c2, "this one shows")

    c3 = flog_set_format(c2, "[{time}] [{level}] <{name}> {msg}")
    c3 = flog_set_name(c3, "game")
    c3 = flog_set_path(c3, "game.log")
    c3 = flog_set_output(c3, "both")       # 控制台 + 文件
    flog_error(c3, "connection lost")

    flog_log_to(c, 1, "audit trail", "audit.log")   # 直接写文件,不改配置
    line = flog_render_line(c, 3, "custom")          # 自己渲染,自用
    println("DIY: " + line)
}
```

运行 `fox -f demo.fox` 即可看到完整演示(级别过滤、模板、文件输出、log_to、render_line)。

颜色与高亮的详细用法(自定义每级颜色、区间高亮、多段高亮、文件输出注意事项)见 [colors.md](colors.md)。

## API

| 函数 | 说明 |
|---|---|
| `flog_default_config()` | 默认配置:`level 0`、`{color}{time} [{level}] {msg}{reset}`、`%Y-%m-%d %H:%M:%S`、`name ""`、`output "console"`、`file_path "log.txt"`、`append 1`、`color 0`、`color_map {"0":90,"1":32,"2":33,"3":31,"4":35}` |
| `flog_log(c, l, msg)` | 按数字级别(0..4)记一条日志 |
| `flog_debug/info/warn/error/fatal(c, msg)` | 级别便捷函数 |
| `flog_log_to(c, l, msg, path)` | 渲染后直接写入指定文件,配置不变 |
| `flog_debug_to/.../fatal_to(c, msg, path)` | 对应级别的 `_to` 变体 |
| `flog_render_line(c, l, msg)` | 只渲染不输出,供自定义消费 |
| `flog_set_level(c, l)` / `flog_set_level_name(c, "warn")` | 设最低级别(越界自动钳制到 0..4) |
| `flog_set_format(c, fmt)` / `flog_set_time_fmt(c, fmt)` / `flog_set_name(c, n)` | 模板 / strftime / 模块名 |
| `flog_set_output(c, o)` | `"console"` / `"file"` / `"both"`(非法值回退 console) |
| `flog_set_path(c, p)` / `flog_set_append(c, a)` | 日志文件路径 / 1 追加(默认)、0 覆盖 |
| `flog_set_color(c, v)` | 1 开启控制台 ANSI 颜色,0 关闭(默认);文件输出始终无颜色 |
| `flog_set_color_code(c, l, code)` | 自定义某个级别(0..4)的 ANSI 颜色码(如 `flog_set_color_code(c, 3, 91)` 让 error 变亮红);常用码 30-37 基础色、90-97 亮色 |
| `flog_highlight(c, msg, a, b, code)` | 把 msg 从 a 到 b(**含两端**,0 起)的字符包上指定颜色;区间越界/为空时原样返回,如 `flog_highlight(c, "disk 92% full", 6, 7, 33)` |
| `flog_append_file(path, line)` / `flog_write_file(path, line)` | 直接向任意文件追加 / 覆盖一行 |
| `flog_clear_file(c)` | 删除配置里的日志文件 |

所有 `flog_set_*` 返回**新的**配置 dict,**必须接住返回值**,否则修改不生效。

## 模板占位符

| 占位符 | 含义 |
|---|---|
| `{time}` | 当前时间,按 `time_fmt`(strftime)格式化,默认 `%Y-%m-%d %H:%M:%S` |
| `{level}` | 级别名大写:`DEBUG/INFO/WARN/ERROR/FATAL` |
| `{level_num}` | 级别数字 `0..4` |
| `{name}` | 模块名(可为空串) |
| `{msg}` | 消息文本 |
| `{nl}` | 换行符 |
| `{esc_q}` | 双引号字符(便于引号包裹日志) |
| `{color}` | 当前级别的 ANSI 颜色码(`color=0` 时为空串):debug 灰 / info 绿 / warn 黄 / error 红 / fatal 紫 |
| `{reset}` | ANSI 复位码(`color=0` 时为空串) |

未知占位符原样保留,可混入自定义 token 后用 `util.str_replace` 自行替换。替换按最长 token 优先,避免部分匹配污染。

## 注意

1. **纯函数式**:函数内看不到全局变量、dict 是值传递,配置必须作为参数传递、setter 返回值必须接住;`append` 等字段语义见上表。
2. **颜色只在控制台**:`color=1` 时模板里的 `{color}/{reset}` 变为 ANSI 码,文件输出永远用 `flog_render_plain` 渲染(无 ANSI)。终端需支持 ANSI(Windows Terminal、VS Code 终端默认支持;老式 cmd 可能需开启 VT 处理)。
3. **highlight 会把 ANSI 带进文件**:`flog_highlight` 把颜色码直接嵌进消息文本,因此该行在文件输出里也会带 ANSI 码;需要文件保持纯净时,把这条日志的 `output` 设为 `"console"`,或只用 `flog_highlight` 做控制台展示。
4. **变量名隔离(重要)**:模块内部使用了 `c` / `l` / `msg` / `path` / `line` / `h` / `mode` 等通用名,与调用方局部变量重名时会覆盖且不恢复;建议你的局部变量用 `c1/c2/log_cfg` 这类前缀命名(如 demo.fox)。
5. **文件按行开关**:无全局句柄,每行 open→write→close,可靠但高频场景有开销。
6. **时间精度**:`fox.sys.time` 提供秒级时间(Value 无 int64),毫秒级不保证。
7. **编译流程**:`-fc` 合并编译需要把全部 6 个文件一起传入(`demo.fox flog.fox flog_cfg.fox flog_render.fox flog_output.fox flog_api.fox`),或先各自 `-c` 生成 `.fc`;改过源文件后要重新编译。
8. **dict 字面量单行**:配置 dict 必须写成一行(解析器在换行处截断语句),参见 demo。
