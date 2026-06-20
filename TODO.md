# alib5 TODO

## data / reflect / schema

### [P0] gen_schema: class 路径补 magic key 承接类级约束
**位置：** `include/alib5/data/reflect/gen_schema.h:98-99`

**现状：**
class 路径直接设 `add_constaint = false`，导致挂在类型 `T` 上的所有以下注解被静默丢弃：

- `attr::Optional`
- `attr::OverrideIfConflict`
- `attr::Extra`
- `attr::SchemaValidate` / `attr::ValidateWithArgs`
- `attr::SchemaRange`
- `attr::DefaultValue`

**目标：**
class 路径下，把约束串写到 `root[magic_key_for_schema_restr]`（即 `"[ALIB5_OBJ]"`）这一项，
让 `if(add_constaint)` 分支去往 magic key 而不是 `root[0]`。validator.cpp 已支持读取
magic key（`from_adata` line 678-680 找不到 magic key 时默认 `RObject`，找到则解析约束）。

**约束：**
- 不要影响 enum / int / double / string / array / leaf 路径的 `root[0]` 行为。
- `DefaultValue` 在 object 上的语义需要重新定义（目前 `root[1]` 只适用于 array 形态）。

---

## data / toml

### [P1] TOML policy: dump 未实现
**位置：** `include/alib5/data/data_toml.h` + 对应 cpp

`__internal_dump` 仅标 `[[deprecated]]`，调用方编译会通过、运行时炸。
要么实现，要么改成 `static_assert(false, ...)` 或完全不声明，让
`IsDataPolicy<TOML, AData>` 在 concept 层就拒绝。

---

## data / validator

### [P1] Node::operator= 拷贝 default_value 不带 allocator
**位置：** `include/alib5/data/validator.h:223`

`Node& operator=(const Node& other)` 直接 `default_value = other.default_value;` —— 没有
走 alloc-extended 构造路径，与 `Node(const Node&, std::pmr::memory_resource*)`（line 185-192）
的语义不一致。跨 pmr 分配器拷贝时可能引入未预期的分配器传播。

### [P2] panic on user data
**位置：** `src/data/validator.cpp:264-266`

TUPLE 分支用户数据元素数 > `array_subs.size()` 时直接 `panic`。生产路径应改成
`record_error` + skip。

### [P2] 死变量清理
- `src/data/validator.cpp:84` —— `int depth = 0;` 只增不读
- `src/data/validator.cpp:236` —— `bool fail = false;` 现在已写但仍未读取（控制流靠 `break` 退出）

### [P2] reset() 是否应清 enable_string_errors / enable_missing
**位置：** `include/alib5/data/validator.h:232`

需要确认是有意保留用户配置还是疏漏。补一行注释即可。

### [P2] typo
- `gen_schema.h:43,147` —— `add_constaint` → `add_constraint`
- `validator.h:309` —— 注释 `ignore_mussing` → `ignore_missing`

---

## data / reflect

### [P2] from_adata.h:124-134 缩进
多 4 空格，仅排版。

### [P2] gen_schema.h:173 / 198 风格统一
`SchemaValidate` 用 `value_mapping.validate_function()`（实例访问 static），
`ValidateWithArgs` 用 `AnnotationBaseType::validate_function()`（类型访问）。统一选其一。

### [P2] -Wreorder 初始化列表顺序
**位置：** `include/alib5/data/validator.h:174-189`

`Node` 构造函数初始化列表顺序与声明顺序不一致，GCC `-Wreorder` 会报。
功能无害但产生 warning。

---

## 文档

### [P2] schema 串语法 BNF/EBNF 文档
schema mini-language 的唯一权威来源是 `src/data/validator.cpp::parse_restriction`。
建议在 `validator.h` 顶部或独立 doc 写出 EBNF，方便 gen_schema 与 parser 保持同步。
