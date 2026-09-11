/**
 * @file data.cppm
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief alib6 动态数据结构子系统聚合导出模块 (alib6.data)
 * @version 6.0
 * @date 2026-08-26
 * 
 * @copyright Copyright (c) 2026
 * 
 */
module;
#include <alib6/config.h>

export module alib6.data;

export import :concepts;
export import :kernel;
export import :json;
export import :toml;
export import :validator;
export import :reflect;
export import :translator;

export namespace alib6 {
    namespace data = alib6::data;

    using Value = alib6::data::Value;
    using CacheValue = alib6::data::Value;
    using AData = alib6::data::AData;
    template<class V = alib6::data::Value>
    using BasicAData = alib6::data::BasicAData<V>;
    using JSON = alib6::data::JSON;
    using TOML = alib6::data::TOML;
    using JSONConfig = alib6::data::JSONConfig;
    using TOMLConfig = alib6::data::TOMLConfig;
    using Validator = alib6::data::Validator;
    using CompareStrategy = alib6::data::CompareStrategy;
    using MergeOperation = alib6::data::MergeOperation;
    using alib6::data::migrate;
    using alib6::data::adata_from_memory;

    using GenericTranslator = alib6::data::GenericTranslator;
    using FlattenTranslator = alib6::data::FlattenTranslator;
    using Translator = alib6::data::Translator;

    namespace attr = alib6::attr;
}

export namespace alib6::data {
    using alib6::to_adata;
    using alib6::from_adata;
    using alib6::fill_matching;
    using alib6::gen_schema;
    using alib6::generate_schema;
}
