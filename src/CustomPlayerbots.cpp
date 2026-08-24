#include "CustomPlayerbots.h"

#include "AccountMgr.h"
#include "Chat.h"
#include "Config.h"
#include "DatabaseEnv.h"
#include "DBCStores.h"
#include "GameTime.h"
#include "Log.h"
#include "MotionMaster.h"
#include "ObjectMgr.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "PlayerbotMgr.h"
#include "RandomPlayerbotMgr.h"
#include "World.h"
#include "WorldSession.h"

#include <algorithm>
#include <deque>
#include <memory>
#include <set>
#include <unordered_set>

namespace
{
struct Appearance { uint8 skin; uint8 face; uint8 hairStyle; uint8 hairColor; uint8 facialHair; };
std::deque<ObjectGuid> startupQueue;
std::unordered_set<ObjectGuid> startupPending;
uint32 startupTimer = 0;
uint32 startupTotal = 0;
uint32 startupLogged = 0;

bool ValidRaceClass(uint8 race, uint8 playerClass)
{
    return sObjectMgr->GetPlayerInfo(race, playerClass) != nullptr;
}

template <typename T> bool Contains(std::vector<T> const& values, T value)
{
    return std::find(values.begin(), values.end(), value) != values.end();
}

bool SelectAppearance(uint8 race, uint8 gender, CustomPlayerbotAppearance const& wanted, Appearance& output, std::string& error)
{
    // CharSections.dbc is the authoritative client-valid appearance catalog.
    // FACE Type + Color is the skin/face pair used by CharacterCreateInfo.
    std::vector<std::pair<uint8, uint8>> facePairs, hairPairs;
    std::vector<uint8> facialHair;
    for (CharSectionsEntry const* section : sCharSectionsStore)
    {
        if (section->Race != race || section->Gender != gender)
            continue;
        if (section->GenType == SECTION_TYPE_FACE)
            facePairs.emplace_back(section->Type, section->Color);
        else if (section->GenType == SECTION_TYPE_HAIR)
            hairPairs.emplace_back(section->Type, section->Color);
        else if (section->GenType == SECTION_TYPE_FACIAL_HAIR)
            facialHair.push_back(section->Type);
    }
    if (facePairs.empty() || hairPairs.empty()) { error = "No appearance data exists for this race/gender."; return false; }

    auto faceIt = std::find_if(facePairs.begin(), facePairs.end(), [&](auto p) {
        return (!wanted.face || p.first == *wanted.face) && (!wanted.skinColor || p.second == *wanted.skinColor);
    });
    if (faceIt == facePairs.end()) { error = "SkinColor/Face is not a valid pair for this race and gender."; return false; }
    auto hairIt = std::find_if(hairPairs.begin(), hairPairs.end(), [&](auto p) {
        return (!wanted.hairStyle || p.first == *wanted.hairStyle) && (!wanted.hairColor || p.second == *wanted.hairColor);
    });
    if (hairIt == hairPairs.end()) { error = "HairStyle/HairColor is not a valid pair for this race and gender."; return false; }

    uint8 facial = 0;
    if (wanted.facialHair)
    {
        if (!Contains(facialHair, *wanted.facialHair)) { error = "FacialHair is not valid for this race and gender."; return false; }
        facial = *wanted.facialHair;
    }
    else if (!facialHair.empty())
        facial = facialHair[urand(0, facialHair.size() - 1)];

    output = { faceIt->second, faceIt->first, hairIt->first, hairIt->second, facial };
    return true;
}

std::optional<ObjectGuid> FindGuid(std::string const& name)
{
    ObjectGuid guid = sCharacterCache->GetCharacterGuidByName(name);
    if (guid.IsEmpty())
        return std::nullopt;
    QueryResult roster = CharacterDatabase.Query("SELECT 1 FROM custom_playerbots WHERE guid = {}", guid.GetCounter());
    return roster ? std::optional<ObjectGuid>(guid) : std::nullopt;
}
}

namespace CustomPlayerbots
{
bool Create(ChatHandler* handler, CustomPlayerbotRequest const& request)
{
    if (ObjectMgr::CheckPlayerName(request.name) != CHAR_NAME_SUCCESS) { handler->PSendSysMessage("Invalid or reserved character name."); return false; }
    if (!request.accountId || AccountMgr::GetCharactersCount(request.accountId) >= 10) { handler->PSendSysMessage("Account does not exist or already has 10 characters."); return false; }
    if (request.gender > GENDER_FEMALE || request.level < 1 || request.level > sWorld->getIntConfig(CONFIG_MAX_PLAYER_LEVEL) || !ValidRaceClass(request.race, request.playerClass))
    { handler->PSendSysMessage("Invalid race, gender, class, or level."); return false; }
    if (sCharacterCache->GetCharacterGuidByName(request.name)) { handler->PSendSysMessage("That character name is already in use."); return false; }

    Appearance appearance; std::string error;
    if (!SelectAppearance(request.race, request.gender, request.appearance, appearance, error)) { handler->PSendSysMessage(error); return false; }

    WorldSession session(request.accountId, "", 0, nullptr, SEC_PLAYER, EXPANSION_WRATH_OF_THE_LICH_KING, time_t(0), LOCALE_enUS, 0, false, false, 0, true);
    CharacterCreateInfo info(request.name, request.race, request.playerClass, request.gender, appearance.skin, appearance.face, appearance.hairStyle, appearance.hairColor, appearance.facialHair);
    std::unique_ptr<Player> bot(new Player(&session));
    bot->GetMotionMaster()->Initialize();
    if (!bot->Create(sObjectMgr->GetGenerator<HighGuid::Player>().Generate(), &info))
    {
        bot->CleanupsBeforeDelete();
        handler->PSendSysMessage("Core rejected character creation.");
        return false;
    }
    if (request.level > 1) bot->GiveLevel(request.level);
    bot->setCinematic(2);
    bot->SetAtLoginFlag(AT_LOGIN_NONE);
    bot->SaveToDB(true, false);
    sCharacterCache->AddCharacterCacheEntry(bot->GetGUID(), request.accountId, bot->GetName(), bot->getGender(), bot->getRace(), bot->getClass(), bot->GetLevel());
    CharacterDatabase.Execute("REPLACE INTO custom_playerbots (guid, account_id, autologin) VALUES ({}, {}, {})", bot->GetGUID().GetCounter(), request.accountId, request.autologin ? 1 : 0);
    ObjectGuid guid = bot->GetGUID();
    bot->CleanupsBeforeDelete();
    handler->PSendSysMessage("Custom playerbot {} created (guid {}).", request.name, guid.GetCounter());
    return true;
}

bool SetAutologin(ChatHandler* handler, std::string const& name, bool enabled)
{
    auto guid = FindGuid(name); if (!guid) { handler->PSendSysMessage("Character not found."); return false; }
    CharacterDatabase.Execute("UPDATE custom_playerbots SET autologin = {} WHERE guid = {}", enabled ? 1 : 0, guid->GetCounter());
    handler->PSendSysMessage("{} autologin {}.", name, enabled ? "enabled" : "disabled"); return true;
}

bool Unregister(ChatHandler* handler, std::string const& name)
{
    auto guid = FindGuid(name);
    if (!guid) { handler->PSendSysMessage("Custom playerbot not found."); return false; }
    if (Player* bot = sRandomPlayerbotMgr.GetPlayerBot(*guid))
        sRandomPlayerbotMgr.LogoutPlayerBot(bot->GetGUID());
    CharacterDatabase.Execute("DELETE FROM custom_playerbots WHERE guid = {}", guid->GetCounter());
    handler->PSendSysMessage("{} is no longer managed as a custom playerbot; its character was not deleted.", name);
    return true;
}

bool Login(ChatHandler* handler, std::string const& name)
{
    auto guid = FindGuid(name); if (!guid) { handler->PSendSysMessage("Character not found."); return false; }
    sRandomPlayerbotMgr.AddPlayerBot(*guid, 0);
    handler->PSendSysMessage("Login requested for {}.", name); return true;
}

bool Logout(ChatHandler* handler, std::string const& name)
{
    auto guid = FindGuid(name); if (!guid) { handler->PSendSysMessage("Character not found."); return false; }
    if (Player* bot = sRandomPlayerbotMgr.GetPlayerBot(*guid)) { sRandomPlayerbotMgr.LogoutPlayerBot(bot->GetGUID()); handler->PSendSysMessage("Logout requested for {}.", name); return true; }
    handler->PSendSysMessage("{} is not logged in as a custom playerbot.", name); return false;
}

void List(ChatHandler* handler)
{
    QueryResult rows = CharacterDatabase.Query("SELECT c.name, c.race, c.gender, c.class, c.level, cp.autologin FROM custom_playerbots cp JOIN characters c ON c.guid = cp.guid ORDER BY c.name");
    if (!rows) { handler->PSendSysMessage("Custom playerbot roster is empty."); return; }
    do { Field* f = rows->Fetch(); handler->PSendSysMessage("{} race={} gender={} class={} level={} autologin={}", f[0].Get<std::string>(), f[1].Get<uint8>(), f[2].Get<uint8>(), f[3].Get<uint8>(), f[4].Get<uint8>(), f[5].Get<uint8>()); } while (rows->NextRow());
}

void QueueStartupLogins()
{
    if (!sConfigMgr->GetOption<bool>("CustomPlayerbots.Enable", true))
    {
        LOG_INFO("server.loading", "mod-custom-playerbots is disabled in mod_custom_playerbots.conf");
        return;
    }

    LOG_INFO("server.loading", "---------------------------------------");
    LOG_INFO("server.loading", " Initializing mod-custom-playerbots ");
    LOG_INFO("server.loading", " Loading persistent custom playerbot roster...");
    LOG_INFO("server.loading", "---------------------------------------");

    startupTimer = sConfigMgr->GetOption<uint32>("CustomPlayerbots.AutoLoginDelayMs", 15000);
    QueryResult rows = CharacterDatabase.Query("SELECT guid FROM custom_playerbots WHERE autologin = 1");
    if (!rows)
    {
        LOG_INFO("server.loading", ">> No persistent custom playerbots are enabled for autologin");
        return;
    }
    do { startupQueue.emplace_back(HighGuid::Player, rows->Fetch()[0].Get<uint32>()); } while (rows->NextRow());
    startupTotal = startupQueue.size();
    startupLogged = 0;
    LOG_INFO("server.loading", ">> {} persistent custom playerbots queued for autologin", startupQueue.size());
}

void Update(uint32 diff)
{
    for (auto it = startupPending.begin(); it != startupPending.end();)
    {
        if (sRandomPlayerbotMgr.GetPlayerBot(*it))
        {
            std::string name;
            sCharacterCache->GetCharacterNameByGuid(*it, name);
            LOG_INFO("module.custom_playerbots", "{}/{} custom bot {} logged in.", ++startupLogged, startupTotal, name);
            it = startupPending.erase(it);
        }
        else
            ++it;
    }

    if (startupQueue.empty()) return;
    if (startupTimer > diff) { startupTimer -= diff; return; }
    startupTimer = 0;
    uint32 batch = sConfigMgr->GetOption<uint32>("CustomPlayerbots.AutoLoginBatchSize", 5);
    while (batch-- && !startupQueue.empty())
    {
        ObjectGuid guid = startupQueue.front();
        sRandomPlayerbotMgr.AddPlayerBot(guid, 0);
        startupPending.insert(guid);
        startupQueue.pop_front();
    }
}

void LogoutAllForShutdown()
{
    LOG_INFO("server.loading", "Logging out all custom bots...");

    QueryResult rows = CharacterDatabase.Query("SELECT guid FROM custom_playerbots");
    if (!rows)
        return;

    uint32 loggedOut = 0;
    do
    {
        ObjectGuid guid(HighGuid::Player, rows->Fetch()[0].Get<uint32>());
        if (Player* bot = sRandomPlayerbotMgr.GetPlayerBot(guid))
        {
            sRandomPlayerbotMgr.LogoutPlayerBot(bot->GetGUID());
            ++loggedOut;
        }
    } while (rows->NextRow());

    LOG_INFO("server.loading", ">> {} custom bots logged out", loggedOut);
}
}
