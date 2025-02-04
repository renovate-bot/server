﻿/*
===========================================================================

  Copyright (c) 2010-2015 Darkstar Dev Teams

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see http://www.gnu.org/licenses/

===========================================================================
*/

#include "common/logging.h"

#include <cstring>

#include "../item_container.h"
#include "../universal_container.h"

#include "../lua/luautils.h"

#include "../packets/char_sync.h"
#include "../packets/char_update.h"
#include "../packets/fishing.h"
#include "../packets/inventory_finish.h"
#include "../packets/message_name.h"
#include "../packets/message_system.h"
#include "../packets/message_text.h"
#include "../packets/release.h"
#include "../packets/chat_message.h"

#include "../map.h"
#include "../vana_time.h"
#include "charutils.h"
#include "fishingutils.h"
#include "itemutils.h"
#include "zoneutils.h"

namespace fishingutils
{
    /************************************************************************
     *																		*
     *  Массив смещений для сообщений рыбалки								*
     *																		*
     ************************************************************************/

    uint16 MessageOffset[MAX_ZONEID];

    void LoadFishingMessages()
    {
        zoneutils::ForEachZone([](CZone* PZone) { MessageOffset[PZone->GetID()] = luautils::GetTextIDVariable(PZone->GetID(), "FISHING_MESSAGE_OFFSET"); });
    }

    /************************************************************************
     *																		*
     *  Получение смещения для сообщений рыбалки								*
     *																		*
     ************************************************************************/

    uint16 GetMessageOffset(uint16 ZoneID)
    {
        return MessageOffset[ZoneID];
    }

    /************************************************************************
     *																		*
     *  Проверяем наличие удочки, наживки и возможности ловли				*
     *																		*
     ************************************************************************/

    void StartFishing(CCharEntity* PChar)
    {
        // NOTE: Fishing is disabled until further notice, since it is a security liability.
        ShowWarning("Fishing is currently disabled");
        PChar->pushPacket(new CChatMessagePacket(PChar, CHAT_MESSAGE_TYPE::MESSAGE_SYSTEM_1, "Fishing is currently disabled"));
        PChar->pushPacket(new CReleasePacket(PChar, RELEASE_TYPE::FISHING));
        return;
        /*
        if (PChar->animation != ANIMATION_NONE)
        {
            PChar->pushPacket(new CMessageSystemPacket(0, 0, 142));
            PChar->pushPacket(new CReleasePacket(PChar, RELEASE_TYPE::FISHING));
            return;
        }

        uint16 MessageOffset = GetMessageOffset(PChar->getZone());

        if (MessageOffset == 0)
        {
            ShowWarning("Player wants to fish in %s", PChar->loc.zone->GetName());
            PChar->pushPacket(new CReleasePacket(PChar, RELEASE_TYPE::FISHING));
            return;
        }

        CItemWeapon* WeaponItem = nullptr;

        WeaponItem = (CItemWeapon*)PChar->getEquip(SLOT_RANGED);

        if ((WeaponItem == nullptr) || !(WeaponItem->isType(ITEM_WEAPON)) || (WeaponItem->getSkillType() != SKILL_FISHING))
        {
            // message: "You can't fish without a rod in your hands"

            PChar->pushPacket(new CMessageTextPacket(PChar, MessageOffset + 0x01));
            PChar->pushPacket(new CReleasePacket(PChar, RELEASE_TYPE::FISHING));
            return;
        }

        WeaponItem = (CItemWeapon*)PChar->getEquip(SLOT_AMMO);

        if ((WeaponItem == nullptr) || !(WeaponItem->isType(ITEM_WEAPON)) || (WeaponItem->getSkillType() != SKILL_FISHING))
        {
            // message: "You can't fish without bait on the hook"

            PChar->pushPacket(new CMessageTextPacket(PChar, MessageOffset + 0x02));
            PChar->pushPacket(new CReleasePacket(PChar, RELEASE_TYPE::FISHING));
            return;
        }

        PChar->animation = ANIMATION_FISHING_START;
        PChar->updatemask |= UPDATE_HP;

        PChar->pushPacket(new CCharUpdatePacket(PChar));
        PChar->pushPacket(new CCharSyncPacket(PChar));
        */
    }

    /************************************************************************
     *																		*
     *  Персонаж ломает удочку												*
     *																		*
     ************************************************************************/

    bool CheckFisherLuck(CCharEntity* PChar)
    {
        if (PChar->UContainer->GetType() != UCONTAINER_EMPTY)
        {
            ShowDebug("Player cannot fish! UContainer is not empty");
            return false;
        }

        CItemFish*   PFish      = nullptr;
        CItemWeapon* WeaponItem = nullptr;

        WeaponItem = (CItemWeapon*)PChar->getEquip(SLOT_RANGED);

        XI_DEBUG_BREAK_IF(WeaponItem == nullptr);
        XI_DEBUG_BREAK_IF(WeaponItem->isType(ITEM_WEAPON) == false);
        XI_DEBUG_BREAK_IF(WeaponItem->getSkillType() != SKILL_FISHING);

        uint16 RodID = WeaponItem->getID();

        WeaponItem = (CItemWeapon*)PChar->getEquip(SLOT_AMMO);

        XI_DEBUG_BREAK_IF(WeaponItem == nullptr);
        XI_DEBUG_BREAK_IF(WeaponItem->isType(ITEM_WEAPON) == false);
        XI_DEBUG_BREAK_IF(WeaponItem->getSkillType() != SKILL_FISHING);

        uint16 LureID = WeaponItem->getID();

        int32 FishingChance = xirand::GetRandomNumber(100);

        if (FishingChance <= 20)
        {
            const char* Query = "SELECT "
                                "fish.fishid,"    // 0
                                "fish.max,"       // 1
                                "fish.watertype," // 2
                                "fish.size,"      // 3
                                "fish.stamina,"   // 4
                                "fish.log,"       // 5
                                "fish.quest,"     // 6
                                "rod.flag "       // 7
                                "FROM fishing_zone AS zone "
                                "INNER JOIN fishing_rod  AS rod  USING (fishid) "
                                "INNER JOIN fishing_lure AS lure USING (fishid) "
                                "INNER JOIN fishing_fish AS fish USING (fishid) "
                                "WHERE zone.zoneid = %u AND rod.rodid = %u AND lure.lureid = %u AND lure.luck = 0";

            int32 ret = sql->Query(Query, PChar->getZone(), RodID, LureID);

            if (ret != SQL_ERROR && sql->NumRows() != 0)
            {
                // array to store fish ids that i can get
                std::vector<int32> fishIDs((int32)sql->NumRows());
                int32              fishCounter       = 0;
                bool               caughtQuestedFish = false;

                while (sql->NextRow() == SQL_SUCCESS)
                {
                    // store fish id
                    fishIDs[fishCounter] = sql->GetIntData(0);

                    // ловля предметов, необходимых для поисков

                    uint8  logid = (uint8)sql->GetIntData(5);
                    uint16 quest = (uint16)sql->GetIntData(6);

                    if (logid < MAX_QUESTAREA && quest < MAX_QUESTID)
                    {
                        uint8 current  = PChar->m_questLog[logid].current[quest / 8] & (1 << (quest % 8));
                        uint8 complete = PChar->m_questLog[logid].complete[quest / 8] & (1 << (quest % 8));

                        if (complete == 0 && current != 0)
                        {
                            PFish = new CItemFish(*itemutils::GetItemPointer(sql->GetIntData(0)));

                            PChar->UContainer->SetType(UCONTAINER_FISHING);
                            PChar->UContainer->SetItem(0, PFish);

                            // got my quested fish
                            caughtQuestedFish = true;
                            break;
                        }
                    }
                    fishCounter++;
                    // TODO: ловля простых предметов
                }

                if (!caughtQuestedFish)
                {
                    int32 luckyFish = xirand::GetRandomNumber((int32)sql->NumRows());
                    PFish           = new CItemFish(*itemutils::GetItemPointer(fishIDs[luckyFish]));

                    PChar->UContainer->SetType(UCONTAINER_FISHING);
                    PChar->UContainer->SetItem(0, PFish);
                }
            }
        }
        else
        {
            const char* Query = "SELECT "
                                "fish.fishid,"    // 0
                                "fish.min,"       // 1
                                "fish.max,"       // 2
                                "fish.size,"      // 3
                                "fish.stamina,"   // 4
                                "fish.watertype," // 5
                                "rod.flag, "      // 6
                                "lure.luck "      // 7
                                "FROM fishing_zone AS zone "
                                "INNER JOIN fishing_rod  AS rod  USING (fishid) "
                                "INNER JOIN fishing_lure AS lure USING (fishid) "
                                "INNER JOIN fishing_fish AS fish USING (fishid) "
                                "WHERE zone.zoneid = %u AND rod.rodid = %u AND lure.lureid = %u AND lure.luck != 0 "
                                "ORDER BY luck";

            int32 ret = sql->Query(Query, PChar->getZone(), RodID, LureID);

            if (ret != SQL_ERROR && sql->NumRows() != 0)
            {
                int32 FisherLuck    = 0;
                int32 FishingChance = xirand::GetRandomNumber(1000);

                while (sql->NextRow() == SQL_SUCCESS)
                {
                    FisherLuck += sql->GetIntData(7);

                    if (FishingChance <= FisherLuck)
                    {
                        PFish = new CItemFish(*itemutils::GetItemPointer(sql->GetIntData(0)));

                        PChar->UContainer->SetType(UCONTAINER_FISHING);
                        PChar->UContainer->SetItem(0, PFish);
                        break;
                    }
                }
            }
        }

        return (PFish != nullptr);
    }

    /************************************************************************
     *																		*
     *  Персонаж теряет наживку (теряет блесну лишь при условии RemoveFly)	*
     *																		*
     ************************************************************************/

    bool LureLoss(CCharEntity* PChar, bool RemoveFly)
    {
        CItemWeapon* PLure = (CItemWeapon*)PChar->getEquip(SLOT_AMMO);

        XI_DEBUG_BREAK_IF(PLure == nullptr);
        XI_DEBUG_BREAK_IF(PLure->isType(ITEM_WEAPON) == false);
        XI_DEBUG_BREAK_IF(PLure->getSkillType() != SKILL_FISHING);

        if (!RemoveFly && (PLure->getStackSize() == 1))
        {
            return false;
        }
        if (PLure->getQuantity() == 1)
        {
            charutils::EquipItem(PChar, 0, SLOT_AMMO, LOC_INVENTORY);
        }

        charutils::UpdateItem(PChar, PLure->getLocationID(), PLure->getSlotID(), -1);
        PChar->pushPacket(new CInventoryFinishPacket());
        return true;
    }

    /************************************************************************
     *																		*
     *  Персонаж ломает удочку												*
     *																		*
     ************************************************************************/

    void RodBreaks(CCharEntity* PChar)
    {
        uint8  SlotID = PChar->equip[SLOT_RANGED];
        CItem* PRod   = PChar->getStorage(LOC_INVENTORY)->GetItem(SlotID);

        XI_DEBUG_BREAK_IF(PRod == nullptr);

        uint16 BrokenRodID = 0;

        switch (PRod->getID())
        {
            case 0x4276:
                BrokenRodID = 0x0728;
                break;
            case 0x4277:
                BrokenRodID = 0x0729;
                break;
            case 0x43E4:
                BrokenRodID = 0x01E3;
                break;
            case 0x43E5:
                BrokenRodID = 0x01D9;
                break;
            case 0x43E6:
                BrokenRodID = 0x01D8;
                break;
            case 0x43E7:
                BrokenRodID = 0x01E2;
                break;
            case 0x43E8:
                BrokenRodID = 0x01EA;
                break;
            case 0x43E9:
                BrokenRodID = 0x01EB;
                break;
            case 0x43EA:
                BrokenRodID = 0x01E9;
                break;
            case 0x43EB:
                BrokenRodID = 0x01E4;
                break;
            case 0x43EC:
                BrokenRodID = 0x01E8;
                break;
            case 0x43ED:
                BrokenRodID = 0x01E7;
                break;
            case 0x43EE:
                BrokenRodID = 0x01E6;
                break;
            case 0x43EF:
                BrokenRodID = 0x01E5;
                break;
        }

        XI_DEBUG_BREAK_IF(BrokenRodID == 0);

        charutils::EquipItem(PChar, 0, SLOT_RANGED, LOC_INVENTORY);
        charutils::UpdateItem(PChar, LOC_INVENTORY, SlotID, -1);
        charutils::AddItem(PChar, LOC_INVENTORY, BrokenRodID, 1);
    }

    /************************************************************************
     *																		*
     *																		*
     *																		*
     ************************************************************************/

    void FishingAction(CCharEntity* PChar, FISH_ACTION action, uint16 stamina, uint32 special)
    {
        // NOTE: Fishing is disabled until further notice, since it is a security liability.
        ShowWarning("Fishing is currently disabled, but somehow we have somone commencing a fishing action");
        // Unlikely anyone can get here legit, since we already disabled "startFishing"
        PChar->animation = ANIMATION_FISHING_STOP;
        return;
        /*
        uint16 MessageOffset = GetMessageOffset(PChar->getZone());

        switch (action)
        {
            case FISH_ACTION::CHECK:
            {
                if (CheckFisherLuck(PChar))
                {
                    // message: "Something caught the hook!"

                    // PChar->animation = ANIMATION_FISHING_FISH;
                    // PChar->updatemask |= UPDATE_HP;
                    PChar->pushPacket(new CMessageTextPacket(PChar, MessageOffset + 0x08));
                    PChar->pushPacket(new CFishingPacket(10128, 128, 20, 500, 13, 140, 60, 0, 0));
                }
                else
                {
                    // message: "You didn't catch anything."

                    PChar->animation = ANIMATION_FISHING_STOP;
                    PChar->updatemask |= UPDATE_HP;
                    PChar->pushPacket(new CMessageTextPacket(PChar, MessageOffset + 0x04));
                }
            }
            break;
            case FISH_ACTION::FINISH:
            {
                if (stamina == 0)
                {
                    // message: "You caught fish!"

                    XI_DEBUG_BREAK_IF(PChar->UContainer->GetType() != UCONTAINER_FISHING);
                    XI_DEBUG_BREAK_IF(PChar->UContainer->GetItem(0) == nullptr);

                    PChar->animation = ANIMATION_FISHING_CAUGHT;
                    PChar->updatemask |= UPDATE_HP;

                    CItem* PFish = PChar->UContainer->GetItem(0);

                    // TODO: анализируем RodFlag

                    charutils::AddItem(PChar, LOC_INVENTORY, PFish->getID(), 1);
                    PChar->loc.zone->PushPacket(PChar, CHAR_INRANGE_SELF, new CMessageNamePacket(PChar, MessageOffset + 0x27, PChar, PFish->getID()));

                    if (PFish->isType(ITEM_USABLE))
                    {
                        LureLoss(PChar, false);
                    }
                    delete PFish;
                }
                else if (stamina <= 0x64)
                {
                    // message: "Your line breaks!"

                    PChar->animation = ANIMATION_FISHING_LINE_BREAK;
                    PChar->updatemask |= UPDATE_HP;
                    LureLoss(PChar, true);
                    PChar->pushPacket(new CMessageTextPacket(PChar, MessageOffset + 0x06));
                }
                else if (stamina <= 0x100)
                {
                    // message: "You give up!"

                    PChar->animation = ANIMATION_FISHING_STOP;
                    PChar->updatemask |= UPDATE_HP;

                    if (PChar->UContainer->GetType() == UCONTAINER_FISHING && LureLoss(PChar, false))
                    {
                        PChar->pushPacket(new CMessageTextPacket(PChar, MessageOffset + 0x24));
                    }
                    else
                    {
                        PChar->pushPacket(new CMessageTextPacket(PChar, MessageOffset + 0x25));
                    }
                }
                else
                {
                    // message: "You lost your catch!"

                    PChar->animation = ANIMATION_FISHING_STOP;
                    PChar->updatemask |= UPDATE_HP;
                    LureLoss(PChar, false);
                    PChar->pushPacket(new CMessageTextPacket(PChar, MessageOffset + 0x09));
                }
                PChar->UContainer->Clean();
            }
            break;
            case FISH_ACTION::WARNING:
            {
                // message: "You don't know how much longer you can keep this one on the line..."

                PChar->pushPacket(new CMessageTextPacket(PChar, MessageOffset + 0x28));
                return;
            }
            break;
            case FISH_ACTION::END:
            {
                // skillup

                PChar->animation = ANIMATION_NONE;
                PChar->updatemask |= UPDATE_HP;
            }
            break;
        }

        PChar->pushPacket(new CCharUpdatePacket(PChar));
        PChar->pushPacket(new CCharSyncPacket(PChar));
        */
    }

} // namespace fishingutils
