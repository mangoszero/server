#include "PacketCodec.h"

#include <algorithm>
#include <cstring>

namespace proto
{
PacketCodec::PacketCodec(HeaderDecryptor decryptor)
    : m_decryptor(std::move(decryptor))
{
}

DecodeStatus PacketCodec::Feed(uint8 const* data, std::size_t len,
    std::vector<WorldPacket>& out)
{
    if (!data && len != 0)
    {
        return DecodeStatus::Malformed;
    }

    std::size_t offset = 0;
    bool producedPacket = false;
    while (offset < len)
    {
        std::size_t consumed = 0;
        DecodeStatus const status = FeedOne(data + offset, len - offset, consumed, out);
        offset += consumed;

        if (status == DecodeStatus::Malformed)
        {
            return status;
        }
        if (status == DecodeStatus::Ready)
        {
            producedPacket = true;
        }
        if (consumed == 0 || status == DecodeStatus::NeedMore)
        {
            break;
        }
    }

    return producedPacket ? DecodeStatus::Ready : DecodeStatus::NeedMore;
}

DecodeStatus PacketCodec::FeedOne(uint8 const* data, std::size_t len,
    std::size_t& consumed, std::vector<WorldPacket>& out)
{
    consumed = 0;
    if (!data && len != 0)
    {
        return DecodeStatus::Malformed;
    }

    while (consumed < len)
    {
        if (!m_haveHeader)
        {
            std::size_t const wanted = CLIENT_HEADER_SIZE - m_headerFill;
            std::size_t const taken = std::min(wanted, len - consumed);
            std::memcpy(m_header + m_headerFill, data + consumed, taken);
            m_headerFill += taken;
            consumed += taken;

            if (m_headerFill < CLIENT_HEADER_SIZE)
            {
                return DecodeStatus::NeedMore;
            }

            if (m_decryptor)
            {
                m_decryptor(m_header, CLIENT_HEADER_SIZE);
            }

            uint32 const wireSize = (uint32(m_header[0]) << 8) | uint32(m_header[1]);
            uint32 const opcode = uint32(m_header[2])
                | (uint32(m_header[3]) << 8)
                | (uint32(m_header[4]) << 16)
                | (uint32(m_header[5]) << 24);

            if (wireSize < 4 || wireSize > MAX_CLIENT_PACKET_SIZE
                || opcode > MAX_CLIENT_PACKET_SIZE)
            {
                return DecodeStatus::Malformed;
            }

            m_opcode = uint16(opcode);
            m_payloadNeeded = wireSize - 4;
            m_haveHeader = true;
            m_payload.clear();
            m_payload.reserve(m_payloadNeeded);
        }

        if (m_payloadNeeded != 0)
        {
            std::size_t const taken =
                std::min<std::size_t>(m_payloadNeeded, len - consumed);
            m_payload.insert(m_payload.end(), data + consumed, data + consumed + taken);
            consumed += taken;
            m_payloadNeeded -= uint32(taken);

            if (m_payloadNeeded != 0)
            {
                return DecodeStatus::NeedMore;
            }
        }

        WorldPacket packet(m_opcode, m_payload.size());
        if (!m_payload.empty())
        {
            packet.append(m_payload.data(), m_payload.size());
        }
        out.push_back(packet);

        m_haveHeader = false;
        m_headerFill = 0;
        m_payload.clear();
        return DecodeStatus::Ready;
    }

    return DecodeStatus::NeedMore;
}

std::vector<uint8> PacketCodec::Encode(WorldPacket const& packet,
    HeaderEncryptor const& encryptor)
{
    uint16 const wireSize = uint16(packet.size() + 2);
    uint16 const opcode = packet.GetOpcode();
    uint8 header[SERVER_HEADER_SIZE] = {
        uint8(wireSize >> 8), uint8(wireSize),
        uint8(opcode), uint8(opcode >> 8)
    };

    if (encryptor)
    {
        encryptor(header, SERVER_HEADER_SIZE);
    }

    std::vector<uint8> wire;
    wire.reserve(SERVER_HEADER_SIZE + packet.size());
    wire.insert(wire.end(), header, header + SERVER_HEADER_SIZE);
    if (!packet.empty())
    {
        wire.insert(wire.end(), packet.contents(), packet.contents() + packet.size());
    }
    return wire;
}
}
