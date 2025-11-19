#pragma once

#include <cstdint>
#include <string_view>
#include <string>

namespace Core {

    using HashValue = uint32_t;

    constexpr HashValue FNV1a_32(std::string_view str) {
        constexpr HashValue offset_basis = 0x811c9dc5;
        constexpr HashValue prime = 0x01000193;

        HashValue hash = offset_basis;
        for (char c : str) {
            hash ^= static_cast<HashValue>(c);
            hash *= prime;
        }
        return hash;
    }

    class HashedString {
    public:
        constexpr HashedString() : m_Hash(0) {}
        constexpr HashedString(HashValue hash) : m_Hash(hash) {}
        
        // Compile-time constructor
        consteval HashedString(const char* str) : m_Hash(FNV1a_32(str)) {
#ifdef _DEBUG
            m_DebugString = str;
#endif
        }

        // Runtime constructor
        explicit HashedString(std::string_view str) : m_Hash(FNV1a_32(str)) {
#ifdef _DEBUG
            m_DebugString = std::string(str);
#endif
        }

        constexpr HashValue GetHash() const { return m_Hash; }
        
        constexpr bool operator==(const HashedString& other) const {
            return m_Hash == other.m_Hash;
        }
        
        constexpr bool operator!=(const HashedString& other) const {
            return m_Hash != other.m_Hash;
        }

        constexpr bool operator<(const HashedString& other) const {
            return m_Hash < other.m_Hash;
        }

#ifdef _DEBUG
        const std::string& GetDebugString() const { return m_DebugString; }
#endif

    private:
        HashValue m_Hash;
#ifdef _DEBUG
        std::string m_DebugString;
#endif
    };

} // namespace Core

constexpr Core::HashedString operator""_hs(const char* str, size_t) {
    return Core::HashedString(Core::FNV1a_32(str));
}

// std::hash specialization
namespace std {
    template<>
    struct hash<Core::HashedString> {
        size_t operator()(const Core::HashedString& hs) const {
            return static_cast<size_t>(hs.GetHash());
        }
    };
}
