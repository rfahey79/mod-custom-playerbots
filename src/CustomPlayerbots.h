#pragma once

#include "Define.h"
#include <optional>
#include <string>
#include <vector>

struct CustomPlayerbotAppearance
{
    std::optional<uint8> skinColor;
    std::optional<uint8> face;
    std::optional<uint8> hairStyle;
    std::optional<uint8> hairColor;
    std::optional<uint8> facialHair;
};

struct CustomPlayerbotRequest
{
    std::string name;
    uint8 race = 0;
    uint8 gender = 0;
    uint8 playerClass = 0;
    uint8 level = 1;
    uint32 accountId = 0;
    bool autologin = true;
    CustomPlayerbotAppearance appearance;
};

class ChatHandler;
class Player;

namespace CustomPlayerbots
{
bool Create(ChatHandler* handler, CustomPlayerbotRequest const& request);
bool Register(ChatHandler* handler, std::string const& name, bool autologin);
bool SetAutologin(ChatHandler* handler, std::string const& name, bool enabled);
bool SetAutonomous(ChatHandler* handler, std::string const& name, bool enabled);
void SetAllAutonomous(ChatHandler* handler, bool enabled);
bool Unregister(ChatHandler* handler, std::string const& name);
bool Login(ChatHandler* handler, std::string const& name);
bool Logout(ChatHandler* handler, std::string const& name);
void List(ChatHandler* handler);
void QueueStartupLogins();
void QueueGuildGreeting(Player* player);
void Update(uint32 diff);
void LogoutAllForShutdown();
}
