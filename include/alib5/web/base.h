#ifndef ALIB5_AWEB_BASE
#define ALIB5_AWEB_BASE
#include <alib5/autil.h>
#include <cstring>

namespace alib5::web{
    constexpr uint32_t conf_web_mtu_size = 1000;

    template <typename T>
    constexpr T to_network_order(T value) noexcept {
        if constexpr(std::endian::native == std::endian::little){
            return std::byteswap(value); 
        }
        return value;
    }

    /// 传输类型
    enum class ProtocolType{
        TCP,
        UDP
    };

    #pragma pack(push, 1)
    /// 这个用来计算大小的
    struct PackageHeader{
        /// 包的版本号,用于区分是否合并包
        uint16_t version_code;
        uint8_t category;
        uint8_t reserved0;
        /// 默认为0,如果category为InternalPartial那么length为总共的index
        uint32_t internal_partial_index;
        /// 数据大小
        uint64_t length;

        void from_web(){
            if constexpr(std::endian::native == std::endian::little){
                version_code = std::byteswap(version_code);
                internal_partial_index    = std::byteswap(internal_partial_index);
                length       = std::byteswap(length);
            }
        }

        /// 准备发送：主机序 -> 网络序
        void to_web(){
            if constexpr(std::endian::native == std::endian::little){
                version_code = std::byteswap(version_code);
                internal_partial_index    = std::byteswap(internal_partial_index);
                length       = std::byteswap(length);
            }
        }
    };
    #pragma pack(pop)
    constexpr uint32_t conf_header_size = sizeof(PackageHeader);

    /// 包类型,64bits对齐
    /// 虽然这个不是直接序列化的,但是标注一下方便后面转换
    struct ALIB5_API Package{
        /// 头
        PackageHeader header;
        /// 数据
        std::span<std::byte> bytes;
    };

    enum class PackCategory : uint8_t {
        Ping = 0,
        Pong = 1,
        Data = 2,
        PartialData = 3, // 业务上的部分发送
        PartialInternal = 4, // 内部的部分发送,用于UDP业务上MTU切分
    };

    /// pop出的包
    struct ALIB5_API DataPack{
        /// 版本号
        uint16_t version_code;
        /// 类型
        PackCategory category;
        /// 默认为0,在InternalPartial的时候起作用
        size_t partial_index;
        /// 数据从大小
        size_t expected_lentgh;
        /// 数据
        std::pmr::vector<std::byte> bytes;
        /// 是否准备完毕
        bool prepared { false };
        /// 回调函数,内部尽量做到move only,外部需要copy
        std::function<void(std::error_code)> callback;

        DataPack(std::pmr::memory_resource * __a) noexcept
        :bytes(__a){}

        DataPack(DataPack && pack) noexcept
        :version_code(pack.version_code)
        ,category(pack.category)
        ,bytes(std::move(pack.bytes))
        ,expected_lentgh(pack.expected_lentgh)
        ,prepared(pack.prepared)
        ,callback(std::move(pack.callback))
        ,partial_index(pack.partial_index){}
        DataPack(const DataPack & p) = default;

        DataPack& operator=(DataPack && d){
            version_code = d.version_code;
            category = d.category;
            bytes = std::move(d.bytes);
            expected_lentgh = d.expected_lentgh;
            callback = std::move(callback);
            prepared = d.prepared;
            partial_index = d.partial_index;
            return *this;
        }
    };

    /// 大端序需要注意了!!
    struct ALIB5_API Address{
        enum Type : uint8_t {
            IPv4,
            IPv6
        };

        Type type { IPv4 };
        uint16_t port { 0 };

        using v4_t = uint32_t;
        using v6_t = std::array<unsigned char,16>;
        
        std::variant<
            v4_t,
            v6_t
        > data { (v4_t)0 };

        void from_web(){
            if constexpr(std::endian::native == std::endian::little){
                port = std::byteswap(port);
                if(type == IPv4){
                    uint32_t& v4 = std::get<v4_t>(data);
                    v4 = std::byteswap(v4);
                }
            }
            // IPv6 (std::array) 不需要翻转字节顺序
        }

        void to_web(){
            from_web(); // 翻转逻辑是对称的
        }
    };

    /// 返回的地址为本地端序
    Address ALIB5_API resolve(std::string_view address,uint16_t port);

    inline DataPack make_pack(std::pmr::memory_resource & res,std::string_view s,uint32_t version = 0,PackCategory cat = PackCategory::Data){
        DataPack pack(&res);

        pack.category = cat;
        pack.version_code = version;
        pack.expected_lentgh = s.size();
        pack.bytes.resize(s.size());
        if(!s.empty())std::memcpy(pack.bytes.data(),s.data(),s.size());
        return pack;
    }
}

#endif