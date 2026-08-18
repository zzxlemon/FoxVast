# flog 颜色与高亮使用指南

本文介绍 flog 的两个扩展功能:给每个日志级别设置**自定义颜色**,以及在消息里**高亮任意区间**的字符。完整 API 见 [README.md](README.md)。

## 快速开始

```fox
!import flog

func main() -> void {
    c = flog_default_config()
    c = flog_set_color(c, 1)              # 开关:1=开(默认 0 关)

    # 1) 每个级别改成自己喜欢的颜色
    c = flog_set_color_code(c, 0, 90)     # debug 亮灰
    c = flog_set_color_code(c, 1, 94)     # info  亮蓝
    c = flog_set_color_code(c, 2, 33)     # warn  黄
    c = flog_set_color_code(c, 3, 91)     # error 亮红
    c = flog_set_color_code(c, 4, 95)     # fatal 亮紫红
    flog_error(c, "connection lost")

    # 2) 高亮消息里的一个区间(0 起,含两端)
    m = flog_highlight(c, "request 10.0.0.1:8080 failed", 8, 20, 33)
    flog_warn(c, m)
}
```

运行(`fox -f` 或先 `-c` 再 `-fc` 均可)即可看到:error 亮红,`10.0.0.1:8080` 黄色。终端需支持 ANSI(Windows Terminal、VS Code 终端默认支持)。

## 默认配色

级别名 → 级别号 → 默认码:

| 级别 | level | 默认颜色码 | 效果 |
|---|---|---|---|
| debug | 0 | 90 | 亮灰 |
| info | 1 | 32 | 绿 |
| warn | 2 | 33 | 黄 |
| error | 3 | 31 | 红 |
| fatal | 4 | 35 | 紫红 |

默认值存在配置的 `color_map` 字段里:`{"0":90, "1":32, "2":33, "3":31, "4":35}`,可用 `flog_set_color_code` 覆盖。

## 自定义每级颜色

```
flog_set_color_code(c, level, code) -> dict    # level: 0..4(越界自动钳制)
```

常用 ANSI SGR 颜色码(`code` 是纯数字,不要带 `\e[` 和 `m`):

| 码 | 颜色 | 码 | 亮色 |
|---|---|---|---|
| 30 | 黑 | 90 | 亮灰 |
| 31 | 红 | 91 | 亮红 |
| 32 | 绿 | 92 | 亮绿 |
| 33 | 黄 | 93 | 亮黄 |
| 34 | 蓝 | 94 | 亮蓝 |
| 35 | 紫红 | 95 | 亮紫红 |
| 36 | 青 | 96 | 亮青 |
| 37 | 白 | 97 | 亮白 |
| 0 | 复位(恢复默认色) | 40-47 | 背景色 |

注意:配置是**值传递**,`flog_set_color_code` 和所有 `flog_set_*` 一样返回**新的** dict,必须接住返回值,否则不生效:

```fox
c = flog_set_color_code(c, 3, 91)     # 正确:接住
flog_set_color_code(c, 4, 95)         # 错误:改了等于没改
```

另外模板里要有 `{color}` / `{reset}` 占位符颜色才出现(默认模板已自带;如果自定义过 format 又不见颜色,把这两个占位符加回去)。

## 高亮消息区间

```
flog_highlight(c, msg, a, b, code) -> string
```

- 角色:把 `msg` 里下标 `a` 到 `b`(0 起,**含两端**)的字符用 `code` 颜色包起来,返回新字符串;
- `a < 0` 自动按 0 处理,`b` 超过末尾自动截到最后一个字符;`a > b`(空区间)原样返回原消息;
- `code` 取值同上表;
- 建议 `code` 用 31/33/36/91/96 这类和高亮底色(如 0/1/2/3/4 的行色)区分开的码,否则看不清。

示例(下标从 0 数):

```
"find 192.168.0.1 ok"              # 高亮 IP: (5, 15, 33)
"request 10.0.0.1:8080 failed"     # 高亮 IP+端口: (8, 20, 33)
"db: user=alice, retry=3/5, code=500"   # 高亮 alice:(10, 14, 96)  高亮 500:(32, 34, 91)
```

用法:

```fox
m = flog_highlight(c, "find 192.168.0.1 ok", 5, 15, 33)
flog_warn(c, m)                                  # 整行 warn 黄 + IP 也黄会看不清,
                                                 # 可把 IP 换成 96 这类亮色
```

### 同时高亮多个片段

`flog_highlight` 一次只能包一段。多段时**从右往左**依次做,左边的下标才不会被前面插入的颜色码顶乱:

```fox
m = flog_highlight(c, "db: user=alice, retry=3/5, code=500", 32, 34, 91)   # 先做右边的 500
m = flog_highlight(c, m, 10, 14, 96)                                       # 再做左边的 alice
flog_error(c, m)
```

为什么:第一次调用在字符串里插入了 `\e[91m...\e[0m` 共 10 个字符,位置在 32 之后,所以左侧 10..14 的下标不变;反过来先做左边,右边的下标就要 +10,容易数错。

## 与文件输出的关系

- 文件输出永远用不带颜色的渲染(`flog_render_plain`),模板里的 `{color}`/`{reset}` 不会进文件;
- 但 `flog_highlight` 是把颜色码**直接嵌进消息文本**的,所以被高亮的那一行写文件时会把 `\e[91m` 这类码一起写进去;
- 需要文件保持纯净时,把这条日志的输出设为 `"console"`,或高亮只用于控制台展示:

```fox
c = flog_set_output(c, "console")    # 该配置输出到控制台,文件不落盘
```

## API 速查

| API | 作用 |
|---|---|
| `flog_set_color(c, v)` | `v = 1` 开启颜色(仅控制台),`0` 关闭(默认) |
| `flog_set_color_code(c, l, code)` | 自定义级别 `l`(0..4)的颜色码,返回新配置 |
| `flog_highlight(c, msg, a, b, code)` | 高亮 `msg[a..b]`(含两端,0 起),返回新字符串 |
| `flog_set_output(c, o)` | `"console"` / `"file"` / `"both"` |
| `flog_set_format(c, fmt)` | 模板,颜色占位符为 `{color}` / `{reset}` |