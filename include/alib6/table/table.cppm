/**
 * @file table.cppm
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 现代化表格生成与排版引擎模块接口 (alib6.table)
 * @version 6.0
 * @date 2026-08-26
 * 
 * @copyright Copyright (c) 2026
 * 
 */
export module alib6.table;

import std;
import alib6.core;
import alib6.log;
import alib6.perf;

namespace pmr = std::pmr;

export namespace alib6::table {

    /// @brief 列水平对齐模式
    enum class ColAlign : u8 {
        Left,
        Center,
        Right
    };

    /// @brief 行垂直对齐模式
    enum class RowAlign : u8 {
        Top,
        Center,
        Bottom
    };

    /// @brief 字符串行与列宽度计算信息
    struct StringCalcInfo {
        pmr::vector<std::string_view> lines;
        pmr::vector<usize> cols;
        usize max_cols{0};

        explicit StringCalcInfo(memory_resource* mem = get_default_resource())
            : lines(mem), cols(mem) {}
    };

    /// @brief 计算 UTF-8 字符串在终端中的真实视觉行数与列宽 (精确支持中英文、CJK 汉字、Emoji 与全角符号)
    [[nodiscard]] StringCalcInfo calc_string_dimensions(std::string_view sv, memory_resource* mem = get_default_resource());

    /**
     * @brief 表格边框与对齐排版配置
     */
    struct TableConfig {
        bool enable_lborder{true};
        bool enable_rborder{true};
        bool enable_top_border{true};
        bool enable_bottom_border{true};
        bool enable_mid_sep{true};
        bool enable_line_sep{true};

        std::string mid_column_sep{"│"};
        std::string line_sep{"─"};
        std::string mid_line_joint{"┼"};

        std::string lborder{"│"};
        std::string rborder{"│"};
        std::string lline_joint{"├"};
        std::string rline_joint{"┤"};

        std::string top_border{"─"};
        std::string top_joint{"┬"};
        std::string top_left{"╭"};
        std::string top_right{"╮"};

        std::string bottom_border{"─"};
        std::string bottom_joint{"┴"};
        std::string bottom_left{"╰"};
        std::string bottom_right{"╯"};

        usize base_padding{1};
        bool wrap_new_line{false};
        ColAlign col_align{ColAlign::Left};
        RowAlign row_align{RowAlign::Top};

        /// @brief 圆角边框风格 (默认)
        static TableConfig unicode_rounded();
        /// @brief 经典直角 Unicode 边框风格
        static TableConfig unicode_box();
        /// @brief 双线高亮边框风格
        static TableConfig double_line();
        /// @brief 虚线点缀风格
        static TableConfig modern_dot();
        /// @brief Markdown 表格兼容格式
        static TableConfig markdown();
        /// @brief 极简现代风格 (无外边框)
        static TableConfig minimal();
        /// @brief 纯 ASCII 跨平台终端兼容风格
        static TableConfig ascii_classic();
    };

    /// @brief 单元格行列坐标
    struct Pos {
        u32 row{0};
        u32 col{0};

        constexpr auto operator<=>(const Pos& other) const noexcept {
            return std::tie(row, col) <=> std::tie(other.row, other.col);
        }
        constexpr bool operator==(const Pos& other) const noexcept {
            return row == other.row && col == other.col;
        }
    };

    struct PosCompare {
        constexpr bool operator()(const Pos& a, const Pos& b) const noexcept {
            return std::tie(a.row, a.col) < std::tie(b.row, b.col);
        }
    };

    class Table;

    /**
     * @brief 表格单元格 (支持流式写入、标签注入、对齐覆盖、嵌套表格与多行排版)
     */
    struct Cell {
        pmr::string cache_str;
        pmr::vector<alib6::log::LogCustomTag> tags;
        StringCalcInfo info;
        bool calced{false};
        std::optional<ColAlign> col_alignment;
        std::optional<RowAlign> row_alignment;

        explicit Cell(memory_resource* mem = get_default_resource())
            : cache_str(mem)
            , tags(mem)
            , info(mem) {}

        void clear() {
            cache_str.clear();
            tags.clear();
            calced = false;
        }

        Cell& align(ColAlign ca) noexcept { col_alignment = ca; return *this; }
        Cell& align(RowAlign ra) noexcept { row_alignment = ra; return *this; }
        Cell& align(ColAlign ca, RowAlign ra) noexcept { col_alignment = ca; row_alignment = ra; return *this; }

        [[nodiscard]] std::string_view view() const noexcept { return cache_str; }

        Cell& operator<<(alib6::log::log_tag t) {
            auto& tag = tags.emplace_back(t.category, t.payload);
            tag.set_pos(cache_str.size());
            return *this;
        }

        Cell& operator<<(alib6::log::log_nop) noexcept {
            return *this;
        }

        Cell& operator<<(Table& inner_tbl);
        Cell& operator<<(const Table& inner_tbl);

        Cell& operator=(Table& inner_tbl) {
            clear();
            return (*this << inner_tbl);
        }
        Cell& operator=(const Table& inner_tbl) {
            clear();
            return (*this << inner_tbl);
        }

        template<class T>
            requires (!std::same_as<std::decay_t<T>, alib6::log::log_tag> &&
                      !std::same_as<std::decay_t<T>, alib6::log::log_nop> &&
                      !std::same_as<std::decay_t<T>, Table>)
        Cell& operator<<(T&& val) {
            if constexpr (std::is_convertible_v<T, std::string_view>) {
                cache_str.append(std::string_view(val));
            } else if constexpr (requires(T&& v, pmr::string& s) { v.write_to_log(s); }) {
                val.write_to_log(cache_str);
            } else {
                std::format_to(std::back_inserter(cache_str), "{}", std::forward<T>(val));
            }
            return *this;
        }
    };

    /**
     * @brief 现代化表格生成与排版器
     */
    class Table {
    public:
        using data_t = pmr::map<Pos, Cell, PosCompare>;

        TableConfig config;
        data_t cells;
        memory_resource* resource;
        std::optional<Cell> default_cell;

        usize top_row{std::numeric_limits<usize>::max()};
        usize left_col{std::numeric_limits<usize>::max()};
        usize bottom_row{0};
        usize right_col{0};
        bool view_fixed{false};

        pmr::vector<alib6::log::LogCustomTag> restore_tags;

        struct RowProxy {
            Table& tbl;
            u32 row;

            struct ColProxy {
                Table& tbl;
                u32 row;
                u32 col;

                [[nodiscard]] Cell& get() {
                    auto [it, _] = tbl.cells.try_emplace(
                        Pos{.row = row, .col = col},
                        tbl.resource
                    );
                    return it->second;
                }

                ColProxy& align(ColAlign ca) { get().align(ca); return *this; }
                ColProxy& align(RowAlign ra) { get().align(ra); return *this; }
                ColProxy& align(ColAlign ca, RowAlign ra) { get().align(ca, ra); return *this; }

                void clear() { get().clear(); }

                template<class T>
                ColProxy& operator<<(T&& val) {
                    get() << std::forward<T>(val);
                    return *this;
                }

                template<class T>
                ColProxy& operator=(T&& val) {
                    clear();
                    get() << std::forward<T>(val);
                    return *this;
                }
            };

            ColProxy operator[](u32 col) {
                if (!tbl.view_fixed) {
                    if (col < tbl.left_col) tbl.left_col = col;
                    if (col > tbl.right_col) tbl.right_col = col;
                }
                return ColProxy{tbl, row, col};
            }
        };

    public:
        explicit Table(
            TableConfig cfg = TableConfig::unicode_rounded(),
            memory_resource* mem = get_default_resource()
        ) : config(cfg)
          , cells(mem)
          , resource(mem)
          , restore_tags(mem) {
            add_restore_tag(alib6::log::color(alib6::log::Color::None));
        }

        template<class Fn>
            requires std::invocable<Fn, Table&>
        explicit Table(
            Fn&& fn,
            TableConfig cfg = TableConfig::unicode_rounded(),
            memory_resource* mem = get_default_resource()
        ) : Table(cfg, mem) {
            fn(*this);
        }

        RowProxy operator[](u32 row) {
            if (!view_fixed) {
                if (row < top_row) top_row = row;
                if (row > bottom_row) bottom_row = row;
            }
            return RowProxy{*this, row};
        }

        void set_view(u32 row, u32 col, u32 row_count, u32 col_count) noexcept {
            top_row = row;
            left_col = col;
            bottom_row = row + row_count - 1;
            right_col = col + col_count - 1;
            view_fixed = true;
        }

        void swap_rows(u32 r1, u32 r2);
        void swap_cols(u32 c1, u32 c2);

        void add_restore_tag(alib6::log::log_tag tag) {
            restore_tags.emplace_back(tag.category, tag.payload);
        }

        [[nodiscard]] pmr::string str(memory_resource* mem = nullptr) const {
            if (!mem) mem = resource;
            pmr::string buf(mem);
            struct StringContext {
                pmr::string& s;
                void append(std::string_view sv) { s.append(sv); }
            } ctx{buf};

            const_cast<Table*>(this)->render_into_context(ctx);
            return buf;
        }

        template<class Target>
        void write_to_log(Target& tg) const {
            tg.append(str());
        }

        template<class CTX>
        CTX&& self_forward(CTX&& ctx) {
            render_into_context(ctx);
            return std::move(ctx);
        }

        template<class CTX>
        void render_into_context(CTX& ctx) {
            if (cells.empty()) return;
            if (right_col < left_col || bottom_row < top_row) return;

            auto access_row = [this](usize r) -> usize { return r - top_row; };
            auto access_col = [this](usize c) -> usize { return c - left_col; };

            usize total_rows = access_row(bottom_row) + 1;
            usize total_cols = access_col(right_col) + 1;

            std::vector<usize> col_widths(total_cols, 0);
            std::vector<usize> row_heights(total_rows, 0);

            // 预处理所有单元格字符串尺寸
            for (auto& [pos, cell] : cells) {
                if (pos.row < top_row || pos.row > bottom_row) continue;
                if (pos.col < left_col || pos.col > right_col) continue;

                if (!cell.calced) {
                    cell.info = calc_string_dimensions(cell.view(), resource);
                    cell.calced = true;
                }

                usize c_idx = access_col(pos.col);
                usize r_idx = access_row(pos.row);

                if (cell.info.max_cols > col_widths[c_idx]) {
                    col_widths[c_idx] = cell.info.max_cols;
                }
                if (cell.info.lines.size() > row_heights[r_idx]) {
                    row_heights[r_idx] = cell.info.lines.size();
                }
            }

            // 保证每行至少有高度 1
            for (auto& h : row_heights) {
                if (h == 0) h = 1;
            }

            auto make_spaces = [](usize count) -> pmr::string {
                return pmr::string(count, ' ');
            };

            auto repeat_str = [](std::string_view pattern, usize count) -> pmr::string {
                pmr::string res;
                if (pattern.empty() || count == 0) return res;
                res.reserve(pattern.size() * count);
                for (usize i = 0; i < count; ++i) {
                    res.append(pattern);
                }
                return res;
            };

            auto append_chunk = [&](std::string_view s) {
                if constexpr (requires { ctx.cache_str.append(s); }) {
                    ctx.cache_str.append(s);
                } else if constexpr (requires { ctx.append(s); }) {
                    ctx.append(s);
                } else {
                    std::move(ctx) << s;
                }
            };

            // 1. 顶部边框
            if (config.enable_top_border) {
                if (config.enable_lborder) append_chunk(config.top_left);
                for (usize c = 0; c < total_cols; ++c) {
                    if (col_widths[c] == 0) continue;
                    usize seg_w = col_widths[c] + 2 * config.base_padding;
                    append_chunk(repeat_str(config.top_border, seg_w));
                    if (config.enable_mid_sep && c + 1 != total_cols) {
                        append_chunk(config.top_joint);
                    }
                }
                if (config.enable_rborder) append_chunk(config.top_right);
                append_chunk("\n");
            }

            // 2. 数据行生成
            for (usize r = 0; r < total_rows; ++r) {
                usize actual_row = top_row + r;
                usize line_count = row_heights[r];

                for (usize l = 0; l < line_count; ++l) {
                    if (config.enable_lborder) append_chunk(config.lborder);

                    for (usize c = 0; c < total_cols; ++c) {
                        usize actual_col = left_col + c;
                        usize col_w = col_widths[c];
                        if (col_w == 0) continue;

                        // 单元格 padding
                        append_chunk(make_spaces(config.base_padding));

                        auto it = cells.find(Pos{.row = static_cast<u32>(actual_row), .col = static_cast<u32>(actual_col)});
                        if (it != cells.end()) {
                            auto& cell = it->second;
                            RowAlign r_align = cell.row_alignment.value_or(config.row_align);
                            ColAlign c_align = cell.col_alignment.value_or(config.col_align);

                            usize start_l = 0;
                            usize end_l = cell.info.lines.size();
                            if (r_align == RowAlign::Bottom) {
                                start_l = line_count - cell.info.lines.size();
                                end_l = line_count;
                            } else if (r_align == RowAlign::Center) {
                                start_l = (line_count - cell.info.lines.size()) / 2;
                                end_l = start_l + cell.info.lines.size();
                            }

                            if (l >= start_l && l < end_l) {
                                usize line_idx = l - start_l;
                                std::string_view text_line = cell.info.lines[line_idx];
                                usize text_w = cell.info.cols[line_idx];

                                usize space_left = 0;
                                usize space_right = col_w >= text_w ? col_w - text_w : 0;

                                if (c_align == ColAlign::Center) {
                                    space_left = space_right / 2;
                                    space_right -= space_left;
                                } else if (c_align == ColAlign::Right) {
                                    space_left = space_right;
                                    space_right = 0;
                                }

                                if (space_left > 0) append_chunk(make_spaces(space_left));

                                // 映射并记录标签 (如颜色)
                                if constexpr (requires { ctx.tags; ctx.cache_str; }) {
                                    usize line_start_pos = 0;
                                    for (usize li = 0; li < line_idx; ++li) {
                                        line_start_pos += cell.info.lines[li].size() + 1;
                                    }
                                    usize line_end_pos = line_start_pos + text_line.size();
                                    usize base_offset = ctx.cache_str.size();

                                    for (const auto& tag : cell.tags) {
                                        usize tpos = tag.get_pos();
                                        if (tpos >= line_start_pos && tpos <= line_end_pos) {
                                            auto t = tag;
                                            t.set_pos(base_offset + (tpos - line_start_pos));
                                            ctx.tags.push_back(t);
                                        } else if (line_idx > 0 && tpos < line_start_pos) {
                                            // 前一行生效的标签，若本行前未被覆盖则延续
                                            bool overridden = false;
                                            for (const auto& next_t : cell.tags) {
                                                if (next_t.category == tag.category && next_t.get_pos() > tpos && next_t.get_pos() <= line_start_pos) {
                                                    overridden = true;
                                                    break;
                                                }
                                            }
                                            if (!overridden) {
                                                auto t = tag;
                                                t.set_pos(base_offset);
                                                ctx.tags.push_back(t);
                                            }
                                        }
                                    }
                                }

                                append_chunk(text_line);

                                // 如果注入了标签，在单元格末尾追加重置标签
                                if constexpr (requires { ctx.tags; ctx.cache_str; }) {
                                    if (!cell.tags.empty()) {
                                        usize reset_pos = ctx.cache_str.size();
                                        for (auto rtag : restore_tags) {
                                            rtag.set_pos(reset_pos);
                                            ctx.tags.push_back(rtag);
                                        }
                                    }
                                }

                                if (space_right > 0) append_chunk(make_spaces(space_right));
                            } else {
                                append_chunk(make_spaces(col_w));
                            }
                        } else {
                            append_chunk(make_spaces(col_w));
                        }

                        append_chunk(make_spaces(config.base_padding));

                        if (c + 1 < total_cols) {
                            if (config.enable_mid_sep) append_chunk(config.mid_column_sep);
                        } else {
                            if (config.enable_rborder) append_chunk(config.rborder);
                        }
                    }
                    append_chunk("\n");
                }

                // 行间分割线
                if (config.enable_line_sep && r + 1 < total_rows) {
                    if (config.enable_lborder) append_chunk(config.lline_joint);
                    for (usize c = 0; c < total_cols; ++c) {
                        if (col_widths[c] == 0) continue;
                        usize seg_w = col_widths[c] + 2 * config.base_padding;
                        append_chunk(repeat_str(config.line_sep, seg_w));
                        if (config.enable_mid_sep && c + 1 != total_cols) {
                            append_chunk(config.mid_line_joint);
                        }
                    }
                    if (config.enable_rborder) append_chunk(config.rline_joint);
                    append_chunk("\n");
                }
            }

            // 3. 底部边框
            if (config.enable_bottom_border) {
                if (config.enable_lborder) append_chunk(config.bottom_left);
                for (usize c = 0; c < total_cols; ++c) {
                    if (col_widths[c] == 0) continue;
                    usize seg_w = col_widths[c] + 2 * config.base_padding;
                    append_chunk(repeat_str(config.bottom_border, seg_w));
                    if (config.enable_mid_sep && c + 1 != total_cols) {
                        append_chunk(config.bottom_joint);
                    }
                }
                if (config.enable_rborder) append_chunk(config.bottom_right);
                if (config.wrap_new_line) append_chunk("\n");
            }
        }
    };

    inline Cell& Cell::operator<<(Table& inner_tbl) {
        struct CellContext {
            Cell& cell;
            void append(std::string_view sv) { cell.cache_str.append(sv); }
            pmr::string& cache_str;
            pmr::vector<alib6::log::LogCustomTag>& tags;
        } ctx{*this, cache_str, tags};
        inner_tbl.render_into_context(ctx);
        return *this;
    }

    inline Cell& Cell::operator<<(const Table& inner_tbl) {
        return (*this << const_cast<Table&>(inner_tbl));
    }

    // ==================== 基准测试结果表格生成助手 ====================

    /**
     * @brief 将多个 BenchmarkResults 生成结构化统计表格
     */
    template<class Fn = std::nullptr_t, class ForwardFn = std::nullptr_t>
    inline Table make_table(
        const std::vector<alib6::perf::BenchmarkResults>& results,
        Fn&& fn = nullptr,
        ForwardFn&& ifwd = nullptr,
        TableConfig cfg = TableConfig::unicode_rounded(),
        memory_resource* mem = get_default_resource()
    ) {
        Table table(cfg, mem);
        table.config.col_align = ColAlign::Center;
        table.config.row_align = RowAlign::Center;

        if (results.empty()) return table;

        // 表头
        table[0][0] << alib6::log::color(alib6::log::Color::Blue, alib6::log::Color::None, alib6::log::Style::Bold) << "Test";
        table[0][1] << "TimeCost";
        table[0][2] << "RunTimes";
        table[0][3] << "Average";
        table[0][4] << "ShortestAvg";
        table[0][5] << "LongestAvg";
        table[0][6] << "Stddev";
        table[0][7] << "CV";

        u32 current_row = 1;
        for (const auto& r : results) {
            if (r.results.empty()) continue;
            auto info = r.calculate();

            std::string fmt = std::format("{{:.{}f}}", r.m_precision);

            table[current_row][0] << alib6::log::color(alib6::log::Color::Blue) << r.m_name;
            table[current_row][1] << alib6::time::normalize_elapse(info.sum);
            table[current_row][2] << info.times;
            table[current_row][3] << alib6::time::normalize_elapse(info.global_aver);
            table[current_row][4] << alib6::time::normalize_elapse(info.shortest_avg);
            table[current_row][5] << alib6::time::normalize_elapse(info.longest_avg);
            table[current_row][6] << std::vformat(fmt, std::make_format_args(info.stddev));
            table[current_row][7] << std::format("{:.2f}%", info.cv);

            ++current_row;
        }

        if constexpr (std::invocable<ForwardFn, Table&>) {
            ifwd(table);
        }
        if constexpr (std::invocable<Fn, Table&>) {
            fn(table);
        }

        return table;
    }

    /**
     * @brief 便捷重载：单个 BenchmarkResults 生成表格
     */
    template<class Fn = std::nullptr_t>
    inline Table make_table(
        const alib6::perf::BenchmarkResults& result,
        Fn&& fn = nullptr,
        TableConfig cfg = TableConfig::unicode_rounded(),
        memory_resource* mem = get_default_resource()
    ) {
        std::vector<alib6::perf::BenchmarkResults> vec = {result};
        return make_table(vec, std::forward<Fn>(fn), nullptr, cfg, mem);
    }

    /**
     * @brief 两个 BenchmarkResults 对比表格生成
     */
    template<class Fn = std::nullptr_t>
    inline Table make_table_compare(
        const alib6::perf::BenchmarkResults& a,
        const alib6::perf::BenchmarkResults& b,
        usize precision = 4,
        Fn&& fn = nullptr,
        TableConfig cfg = TableConfig::unicode_rounded(),
        memory_resource* mem = get_default_resource()
    ) {
        std::vector<alib6::perf::BenchmarkResults> results = {a, b};
        return make_table(results, std::forward<Fn>(fn), [&](Table& tbl) {
            auto calc_a = a.calculate();
            auto calc_b = b.calculate();

            u32 diff_row = 3;
            tbl[diff_row][0] << alib6::log::color(alib6::log::Color::Cyan, alib6::log::Color::None, alib6::log::Style::Bold)
                             << "Relative Diff (A vs B)";
            tbl[diff_row][1] << "-";
            tbl[diff_row][2] << "-";

            auto format_diff = [](double val_a, double val_b, bool lower_is_better = true) -> std::string {
                if (val_b == 0.0) return "-";
                double rel = (1.0 - (val_a / val_b)) * 100.0;
                return std::format("{:+.2f}%", rel);
            };

            tbl[diff_row][3] << format_diff(calc_a.global_aver, calc_b.global_aver, true);
            tbl[diff_row][4] << format_diff(calc_a.shortest_avg, calc_b.shortest_avg, true);
            tbl[diff_row][5] << format_diff(calc_a.longest_avg, calc_b.longest_avg, true);
            tbl[diff_row][6] << "-";
            tbl[diff_row][7] << format_diff(calc_a.cv, calc_b.cv, true);
        }, cfg, mem);
    }

    /**
     * @brief 即时压测两个可调用对象并生成对比表格
     */
    template<usize Times = 1000, usize Turns = 10, class Fn1, class Fn2, class Fn = std::nullptr_t>
    inline Table make_table_compare_bench(
        std::string_view name1,
        Fn1&& fn1,
        std::string_view name2,
        Fn2&& fn2,
        usize precision = 4,
        Fn&& fn = nullptr,
        TableConfig cfg = TableConfig::unicode_rounded(),
        memory_resource* mem = get_default_resource()
    ) {
        auto res1 = alib6::perf::bench(name1, Times, Turns, std::forward<Fn1>(fn1));
        auto res2 = alib6::perf::bench(name2, Times, Turns, std::forward<Fn2>(fn2));
        return make_table_compare(res1, res2, precision, std::forward<Fn>(fn), cfg, mem);
    }

} // namespace alib6::table

export namespace alib6 {
    using Table = alib6::table::Table;
    using TableConfig = alib6::table::TableConfig;
    using ColAlign = alib6::table::ColAlign;
    using RowAlign = alib6::table::RowAlign;
    using alib6::table::make_table;
    using alib6::table::make_table_compare;
    using alib6::table::make_table_compare_bench;
}

export namespace std {
    template<>
    struct formatter<alib6::table::Table> : formatter<string_view> {
        auto format(const alib6::table::Table& tbl, format_context& ctx) const {
            auto s = tbl.str();
            return formatter<string_view>::format(s, ctx);
        }
    };
}
