/**
 * vim: set ts=4 sw=4 tw=99 noet:
 * =============================================================================
 * GameFixes_s2m
 * Copyright (C) 2026 Michal "Slynx (˙·٠● S l y n x ●٠·˙)" Přikryl.
 * =============================================================================
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License, version 3.0, as published by the
 * Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * As a special exception, Michal "Slynx (˙·٠● S l y n x ●٠·˙)" Přikryl gives
 * you permission to link the code of this program (as well as its derivative
 * works) to "Counter-Strike 2," "Source 2," "Steam," and any Game MODs or
 * server software running on software by Valve Corporation. You must obey the
 * GNU General Public License in all respects for all other code used.
 *
 * Additionally, this exception applies to all derivative works unless
 * otherwise stated in LICENSE.txt.
 *
 * Authors:
 *   - Michal "Slynx (˙·٠● S l y n x ●٠·˙)" Přikryl
 *
 * Project: GameFixes_s2m
 */

#include "voice_flood.h"

#include <algorithm>
#include <cstdio>

// One message: voice_data, the serialized message, and its cost (the
// serialized size plus 4 bytes per packet) are each held to 16 KiB, and it
// may carry at most 64 packets.
static constexpr size_t kMaxVoiceDataBytes = 16384;
static constexpr size_t kMaxMessageBytes = 16384;
static constexpr uint32_t kMaxPacketsPerMessage = 64;
static constexpr size_t kBytesPerPacket = 4;

// The rolling budget, per client, over the last second (of game time).
static constexpr double kBudgetWindow = 1.0;
static constexpr size_t kMaxMessagesPerWindow = 192;
static constexpr size_t kMaxCostPerWindow = 256 * 1024;
static constexpr uint32_t kMaxPacketsPerWindow = 1024;

const char* CVoiceFloodFix::GetDescription() const
{
    return "Drops malformed or oversized clc_VoiceData before the engine handles it (16 KiB / 64 packets per\n"
           "message) and holds every client to 192 messages, 256 KiB and 1024 packets per second. Never kicks anyone.";
}

bool CVoiceFloodFix::Load(char* error, size_t maxlen)
{
    if (!g_pSource2ModNetworkMessages || !g_pSource2ModClients)
    {
        std::snprintf(error, maxlen, "the core has no network messages / clients interface: update Source2Mod");
        return false;
    }

    if (!HookClientMessage({ this, &CVoiceFloodFix::OnClientMessage }))
    {
        std::snprintf(error, maxlen, "could not hook the clients' messages");
        return false;
    }

    Log("watching clc_VoiceData through the core's client message hook");
    return true;
}

void CVoiceFloodFix::Unload()
{
    // The hook and its function are taken down by the plugin.
    for (Budget& budget : m_Budgets)
        budget = {};
}

void CVoiceFloodFix::OnStartupServer(StartupServerContext& ctx)
{
    for (Budget& budget : m_Budgets)
        budget = {};

    if (m_nDropped)
        Log("%llu voice message(s) dropped on the last map", static_cast<unsigned long long>(m_nDropped));

    m_nDropped = 0;
    m_bLoggedFirstDrop = false;
}

Action CVoiceFloodFix::OnClientMessage(CPlayerSlot slot, int nMessageId, void* pMessage)
{
    if (nMessageId != clc_VoiceData || !pMessage)
        return Action::Ignore;

    GF_VPROF("GameFixes::voice_flood::ClientMessage");

    // The handle is the protobuf message; the voice message is its derived type.
    const auto& msg = *static_cast<const CCLCMsg_VoiceData*>(static_cast<const google::protobuf::Message*>(pMessage));
    auto* pClient = reinterpret_cast<CServerSideClientBase*>(g_pSource2ModClients->GetServerSideClient(slot));

    // Nothing to check without a client: let the engine have it as it is.
    if (!ShouldDrop(pClient, msg))
        return Action::Ignore;

    m_nDropped++;
    if (!m_bLoggedFirstDrop)
    {
        m_bLoggedFirstDrop = true;
        Log("dropped a voice message from slot %d", slot.Get());
    }

    // The message just never reaches the voice system; nobody is kicked.
    return Action::Supersede;
}

bool CVoiceFloodFix::ShouldDrop(CServerSideClientBase* pClient, const CCLCMsg_VoiceData& msg)
{
    // Bots and clients still connecting are left to the engine.
    if (!pClient || pClient->IsFakeClient() || pClient->m_nSignonState != SIGNONSTATE_FULL)
        return false;

    const int nSlot = pClient->m_nClientSlot.Get();
    if (nSlot < 0 || nSlot >= ABSOLUTE_PLAYER_LIMIT)
        return true;

    // Malformed: no audio, no sender, or a sender that isn't this client.
    if (!msg.has_audio() || !msg.has_xuid())
        return true;
    if (msg.xuid() == 0 || msg.xuid() != pClient->m_SteamID.ConvertToUint64())
        return true;

    const CMsgVoiceAudio& audio = msg.audio();
    if (!VoiceDataFormat_t_IsValid(audio.format()))
        return true;

    const uint32_t nOffsets = static_cast<uint32_t>(audio.packet_offsets_size());
    const uint32_t nPackets = audio.num_packets();
    const size_t nVoiceBytes = audio.voice_data().size();

    // Packets announced with no data to go with them.
    if ((nOffsets != 0 || nPackets != 0) && nVoiceBytes == 0)
        return true;

    if (nOffsets > kMaxPacketsPerMessage || nPackets > kMaxPacketsPerMessage || nVoiceBytes > kMaxVoiceDataBytes)
        return true;

    const size_t nMessageBytes = msg.ByteSizeLong();
    if (nMessageBytes > kMaxMessageBytes)
        return true;

    const uint32_t packets = (std::max)(nOffsets, nPackets);
    const size_t cost = nMessageBytes + kBytesPerPacket * packets;
    if (cost > kMaxMessageBytes)
        return true;

    // Forget what fell out of the window, then take this message only if it
    // still fits.
    Budget& budget = m_Budgets[nSlot];
    const double now = GameTime();
    while (!budget.recent.empty() && now - budget.recent.front().time >= kBudgetWindow)
    {
        budget.cost -= budget.recent.front().cost;
        budget.packets -= budget.recent.front().packets;
        budget.recent.pop_front();
    }

    if (budget.recent.size() >= kMaxMessagesPerWindow)
        return true;
    if (budget.cost + cost > kMaxCostPerWindow)
        return true;
    if (budget.packets + packets > kMaxPacketsPerWindow)
        return true;

    budget.recent.push_back({ now, cost, packets });
    budget.cost += cost;
    budget.packets += packets;
    return false;
}
