#pragma once

#include <cstdint>

namespace OsborneX::Net::wire {

enum class MessageType : std::uint8_t
{
    OrderEntry = 1,
    TopOfBook = 2,
    TradeExecution = 3,
};

inline constexpr std::uint32_t kFrameMagic = 0x4F584E31; // "OXN1"
inline constexpr std::uint8_t kFrameVersion = 1;

/// @brief Shared framing header for both the TCP order-entry stream and UDP market-data
///        datagrams: one frame, one message.
/// @details v1 limitation, documented deliberately (same spirit as Orderbook's
///          GetNextMarketClose weekend-gap caveat): no network-byte-order conversion is
///          applied anywhere in this wire layer. Both build targets (windows-msvc-*,
///          linux-gcc-*) are little-endian x86-64 today, so this holds in practice, but
///          would need htons/htonl-style conversion added before targeting a big-endian
///          or otherwise differently-ordered platform.
#pragma pack(push, 1)
struct FrameHeader
{
    std::uint32_t magic{ kFrameMagic };
    std::uint8_t version{ kFrameVersion };
    MessageType message_type{};
    std::uint16_t payload_length{};
};
#pragma pack(pop)
static_assert(sizeof(FrameHeader) == 8);

} // namespace OsborneX::Net::wire
