# fjson — FoxVast 的 JSON 插件模块

用 FoxVast 实现的 JSON 解析 / 序列化插件,不依赖任何核心语言扩展。提供解析、节点查询、字符串化(roundtrip)与从零构建 JSON 的能力。

## 模块结构

| 文件 | 职责 |
|---|---|
| `fjson.fox` | 聚合入口,仅 import 其余四模块 |
| `json_parse.fox` | `parse`(文本 → 节点树)、错误校验 |
| `json_get.fox` | `node_type` / `str_val` / `int_val` / `num_val` / `bool_val` / `is_null` / `arr_len` / `get` / `get_at` / `has` |
| `json_stringify.fox` | `stringify` / `escape_json` / `unescape_json` 等 |
| `json_build.fox` | `mk_obj` / `mk_arr` / `mk_str` / `mk_num` / `mk_bool` / `mk_null` / `obj_set` / `arr_push`、`is_obj` / `is_arr` 等 |

## 使用

你的脚本里 `!import fjson`(`!import` 会按以下顺序查找:脚本所在目录 → `C:\FoxLibs` → 当前工作目录,裸名自动补 `.fox`)。也可以把四个 json_*.fox 丢进 `C:\FoxLibs`,然后 `!import fjson`。

```fox
!import fjson

txt = "{\"name\": \"FoxVast\", \"version\": 3, \"tags\": [\"lang\", \"json\"], \"meta\": {\"flags\": [1, 2]}}"

n = parse(txt)
println(node_type(n))                       // obj
println(str_val(get(n, "name")))            // FoxVast
println(int_val(get(n, "version")))         // 3
println(arr_len(get(n, "tags")))            // 2
println(int_val(get_at(get(n, "tags"), 0))) // 1
println(stringify(n))                       // 原文 roundtrip
```

## 注意

1. **不可变构建**:`obj_set` / `arr_push` 返回新节点,必须把返回值重新赋给原变量,否则修改不生效。
2. **类型转换**:`mk_num` 的参数是 double,整数请用 `util.IntChangeDouble(5)`;`mk_bool` 的参数是 int(1/0)。
3. **数字保留原文**:解析出的数字节点的 `"s"` 字段保留源文字面量,`stringify` 按原文输出,`3.14159`、`1.5e3` 不会被改写;`mk_num(5.0)` 输出 `5`,小数会去掉尾零。
4. **转义**:解析与序列化双向支持 `\"`、`\\`、`\n`、`\r`、`\t`、`\b`、`\f`。
5. **严格校验**:未闭合字符串、bad number、缺少冒号、对象键非字符串、尾部多余内容等都会 `error()` 终止,不会静默容忍。
6. **变量名隔离(重要)**:FoxVast 的变量是全局的,被调函数(包括库函数内部)的参数/局部名与调用方局部同名时会覆盖调用方且不恢复。`fjson.fox` 内部所有参数与局部已做后缀唯一化(`t_ws`、`nd_sf`、`s_s`、`ky_h`、`s_ms` 等),正常用法不会冲突;若你在调用后继续读取某变量却发现值变了,检查是否与模块内部名字重名。
7. **编译流程**:用 `-fc` 跑之前必须先 `-c`;改过 `fjson.fox` 或脚本后要重新 `-c`,否则执行的是旧字节码。
8. **能力边界**:完整支持嵌套对象/数组、空对象 `{}`、空数组 `[]`、`true`/`false`/`null`;不做 `\uXXXX` 转义解析。