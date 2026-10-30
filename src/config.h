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
#pragma once

// A fix's block of game_fixes.json, as ReadConfig() gets it.
//
// A thin, copyable view over Source2Mod's JSON node (ISource2ModJSONValue) --
// nothing here owns anything. Every getter is total: a missing key or a value
// of the wrong type gives the default you pass, never an error. (The core has
// already merged the admin's file over the defaults the fix declared in
// GetConfigDefaults(), so a key the fix declared is always there; the default
// argument only matters for keys a fix reads without declaring.)
//
// The nodes are valid only while ReadConfig() runs -- Source2Mod frees them a
// frame after a config change. COPY what you need into your own members
// (std::string, std::vector, ...); never keep a FixConfig, a Section() or a
// const char* from it.

#include <source2mod/ISource2ModJSON.h>

#include <cstdint>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

class FixConfig
{
public:
    FixConfig() = default;
    explicit FixConfig(const ISource2ModJSONValue* pNode) : m_pNode(pNode) {}

    /// Whether there is a node at all (false for the Section() of a missing key).
    bool Valid() const { return m_pNode != nullptr; }
    bool IsObject() const { return m_pNode && m_pNode->IsObject(); }
    bool IsArray() const { return m_pNode && m_pNode->IsArray(); }

    /// This object has the key.
    bool Has(const char* pszKey) const { return m_pNode && m_pNode->Has(pszKey); }

    /* ---- scalars by key: "reload_interval": 1800 ---- */

    bool GetBool(const char* pszKey, bool bDefault = false) const
    {
        return m_pNode ? m_pNode->GetBool(pszKey, bDefault) : bDefault;
    }

    int GetInt(const char* pszKey, int nDefault = 0) const
    {
        return m_pNode ? m_pNode->GetInt(pszKey, nDefault) : nDefault;
    }

    int64_t GetInt64(const char* pszKey, int64_t nDefault = 0) const
    {
        return m_pNode ? m_pNode->GetInt64(pszKey, nDefault) : nDefault;
    }

    float GetFloat(const char* pszKey, float flDefault = 0.0f) const
    {
        return m_pNode ? m_pNode->GetFloat(pszKey, flDefault) : flDefault;
    }

    double GetDouble(const char* pszKey, double flDefault = 0.0) const
    {
        return m_pNode ? m_pNode->GetDouble(pszKey, flDefault) : flDefault;
    }

    std::string GetString(const char* pszKey, const char* pszDefault = "") const
    {
        return m_pNode ? std::string(m_pNode->GetString(pszKey, pszDefault)) : std::string(pszDefault);
    }

    /// A 64-bit unsigned that may be written as a JSON number or as a string
    /// ("76561198000000000" -- a SteamID64 is safest quoted).
    uint64_t GetUInt64(const char* pszKey, uint64_t nDefault = 0) const
    {
        return At(pszKey).AsUInt64(nDefault);
    }

    /* ---- nested objects and lists ---- */

    /// The object (or array) under a key; !Valid() when absent.
    FixConfig Section(const char* pszKey) const
    {
        return FixConfig(m_pNode ? m_pNode->Get(pszKey) : nullptr);
    }

    /// ["a", "b"] -> {"a", "b"}; elements that are not strings are skipped.
    std::vector<std::string> GetStringList(const char* pszKey) const
    {
        return Section(pszKey).AsStringList();
    }

    /// { "Slynx": "7656..." } -> {{"Slynx", "7656..."}} in file order, which is what
    /// "label -> value" blocks like steam_ban's whitelist are. Numbers and bools
    /// come back as their text.
    std::vector<std::pair<std::string, std::string>> GetStringMap(const char* pszKey) const
    {
        return Section(pszKey).AsStringMap();
    }

    /* ---- walking a node of any shape (for anything the getters above do not cover) ---- */

    /// Members of an object, elements of an array; 0 for anything else.
    int Count() const { return m_pNode ? m_pNode->Count() : 0; }
    /// Key of member @p nIndex of an object ("" for an array).
    std::string KeyAt(int nIndex) const
    {
        const char* pszKey = m_pNode ? m_pNode->KeyAt(nIndex) : nullptr;
        return pszKey ? pszKey : "";
    }
    /// Member @p nIndex of an object (its value) or element @p nIndex of an array.
    FixConfig At(int nIndex) const
    {
        return FixConfig(m_pNode ? m_pNode->At(nIndex) : nullptr);
    }
    FixConfig At(const char* pszKey) const { return Section(pszKey); }

    /* ---- this node's own value ---- */

    bool AsBool(bool bDefault = false) const { return m_pNode ? m_pNode->AsBool(bDefault) : bDefault; }
    int AsInt(int nDefault = 0) const { return m_pNode ? m_pNode->AsInt(nDefault) : nDefault; }
    int64_t AsInt64(int64_t nDefault = 0) const { return m_pNode ? m_pNode->AsInt64(nDefault) : nDefault; }
    double AsDouble(double flDefault = 0.0) const { return m_pNode ? m_pNode->AsDouble(flDefault) : flDefault; }
    float AsFloat(float flDefault = 0.0f) const { return m_pNode ? m_pNode->AsFloat(flDefault) : flDefault; }

    /// A string node's text; a number or bool node's text too ("1800", "true").
    std::string AsString(const char* pszDefault = "") const
    {
        if (!m_pNode)
            return pszDefault;
        if (m_pNode->IsString())
            return m_pNode->AsString(pszDefault);
        if (m_pNode->IsNumber() || m_pNode->IsBool())
            return m_pNode->Dump(-1);
        return pszDefault;
    }

    uint64_t AsUInt64(uint64_t nDefault = 0) const
    {
        if (!m_pNode)
            return nDefault;
        if (m_pNode->IsString())
        {
            const char* pszText = m_pNode->AsString("");
            char* pEnd = nullptr;
            const unsigned long long nValue = std::strtoull(pszText, &pEnd, 10);
            return (pEnd && pEnd != pszText && *pEnd == '\0') ? static_cast<uint64_t>(nValue) : nDefault;
        }
        if (m_pNode->IsNumber())
            return static_cast<uint64_t>(m_pNode->AsInt64(static_cast<int64_t>(nDefault)));
        return nDefault;
    }

    std::vector<std::string> AsStringList() const
    {
        std::vector<std::string> list;
        if (!IsArray())
            return list;
        for (int i = 0; i < m_pNode->Count(); i++)
        {
            const ISource2ModJSONValue* pElement = m_pNode->At(i);
            if (pElement && pElement->IsString())
                list.emplace_back(pElement->AsString(""));
        }
        return list;
    }

    std::vector<std::pair<std::string, std::string>> AsStringMap() const
    {
        std::vector<std::pair<std::string, std::string>> map;
        if (!IsObject())
            return map;
        for (int i = 0; i < m_pNode->Count(); i++)
            map.emplace_back(KeyAt(i), At(i).AsString());
        return map;
    }

private:
    const ISource2ModJSONValue* m_pNode = nullptr;
};
