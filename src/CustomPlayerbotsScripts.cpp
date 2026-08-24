#include "CustomPlayerbots.h"
#include "AccountMgr.h"
#include "Chat.h"
#include "Config.h"
#include "ScriptMgr.h"
#include "WorldScript.h"

#include <charconv>
#include <cctype>
#include <map>
#include <sstream>

namespace
{
using namespace Acore::ChatCommands;

bool ParseUInt(std::string const& text, uint8& value) { unsigned n; auto [p, e] = std::from_chars(text.data(), text.data()+text.size(), n); if (e != std::errc() || p != text.data()+text.size() || n > 255) return false; value = uint8(n); return true; }
bool ParseBool(std::string const& text, bool& value) { if (text == "1" || text == "true" || text == "yes" || text == "on") { value = true; return true; } if (text == "0" || text == "false" || text == "no" || text == "off") { value = false; return true; } return false; }
uint8 Race(std::string s) { static std::map<std::string,uint8> const v={{"human",1},{"orc",2},{"dwarf",3},{"nightelf",4},{"undead",5},{"tauren",6},{"gnome",7},{"troll",8},{"bloodelf",10},{"draenei",11}}; for(char& c:s)c=tolower(c); uint8 n; return ParseUInt(s,n)?n:(v.count(s)?v.at(s):0); }
uint8 Class(std::string s) { static std::map<std::string,uint8> const v={{"warrior",1},{"paladin",2},{"hunter",3},{"rogue",4},{"priest",5},{"deathknight",6},{"shaman",7},{"mage",8},{"warlock",9},{"druid",11}}; for(char& c:s)c=tolower(c); uint8 n; return ParseUInt(s,n)?n:(v.count(s)?v.at(s):0); }
uint8 Gender(std::string s) { for(char& c:s)c=tolower(c); if(s=="male"||s=="m")return 0; if(s=="female"||s=="f")return 1; uint8 n; return ParseUInt(s,n)?n:255; }

class custom_playerbots_commands : public CommandScript
{
public: custom_playerbots_commands() : CommandScript("custom_playerbots_commands") {}
    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable bot = {{"create", HandleCreate, rbac::RBAC_PERM_COMMAND_GM, Console::Yes}, {"list", HandleList, rbac::RBAC_PERM_COMMAND_GM, Console::Yes}, {"login", HandleLogin, rbac::RBAC_PERM_COMMAND_GM, Console::Yes}, {"logout", HandleLogout, rbac::RBAC_PERM_COMMAND_GM, Console::Yes}, {"autologin", HandleAutologin, rbac::RBAC_PERM_COMMAND_GM, Console::Yes}, {"unregister", HandleUnregister, rbac::RBAC_PERM_COMMAND_GM, Console::Yes}};
        return {{"custombot", bot}};
    }
    static bool HandleCreate(ChatHandler* h, std::string_view args)
    {
        std::istringstream in{std::string(args)}; std::string race, gender, cls, level, account, autoLogin, token; CustomPlayerbotRequest r;
        if (!(in >> r.name >> race >> gender >> cls >> level >> account >> autoLogin) || !ParseUInt(level, r.level)) { h->PSendSysMessage("Syntax: .custombot create Name Race Gender Class Level Account AutoLogin [skin=N face=N hairstyle=N haircolor=N facialhair=N]"); return true; }
        r.race=Race(race); r.gender=Gender(gender); r.playerClass=Class(cls); if (!ParseBool(autoLogin,r.autologin)) { h->PSendSysMessage("AutoLogin must be 0/1, true/false, yes/no, or on/off."); return true; } r.accountId=AccountMgr::GetId(account);
        while(in >> token) { auto at=token.find('='); if(at==std::string::npos) { h->PSendSysMessage("Appearance options use key=value."); return true; } uint8 n; if(!ParseUInt(token.substr(at+1),n)){h->PSendSysMessage("Appearance values must be 0-255.");return true;} auto key=token.substr(0,at); if(key=="skin")r.appearance.skinColor=n; else if(key=="face")r.appearance.face=n; else if(key=="hairstyle")r.appearance.hairStyle=n; else if(key=="haircolor")r.appearance.hairColor=n; else if(key=="facialhair")r.appearance.facialHair=n; else {h->PSendSysMessage("Supported appearance keys: skin, face, hairstyle, haircolor, facialhair.");return true;} }
        CustomPlayerbots::Create(h,r);
        return true;
    }
    static bool HandleList(ChatHandler* h, std::string_view) { CustomPlayerbots::List(h); return true; }
    static bool HandleLogin(ChatHandler* h, std::string_view a) { CustomPlayerbots::Login(h,std::string(a)); return true; }
    static bool HandleLogout(ChatHandler* h, std::string_view a) { CustomPlayerbots::Logout(h,std::string(a)); return true; }
    static bool HandleUnregister(ChatHandler* h, std::string_view a) { CustomPlayerbots::Unregister(h,std::string(a)); return true; }
    static bool HandleAutologin(ChatHandler* h, std::string_view a) { std::istringstream in{std::string(a)};std::string n,v;if(!(in>>n>>v)){h->PSendSysMessage("Syntax: .custombot autologin Name on|off");return true;}bool b;if(!ParseBool(v,b)){h->PSendSysMessage("Use on or off.");return true;}CustomPlayerbots::SetAutologin(h,n,b);return true; }
};
class custom_playerbots_world : public WorldScript { public: custom_playerbots_world() : WorldScript("custom_playerbots_world") {} void OnStartup() override { CustomPlayerbots::QueueStartupLogins(); } void OnUpdate(uint32 diff) override { CustomPlayerbots::Update(diff); } };
}
void AddSC_mod_custom_playerbots() { new custom_playerbots_commands(); new custom_playerbots_world(); }
