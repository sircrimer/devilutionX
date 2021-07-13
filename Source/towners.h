/**
 * @file towners.h
 *
 * Interface of functionality for loading and spawning towners.
 */
#pragma once

#include <cstdint>
#include <memory>
#include "utils/stdcompat/string_view.hpp"

#include "items.h"
#include "player.h"
#include "quests.h"
#include "utils/stdcompat/cstddef.hpp"

namespace devilution {

#define NUM_TOWNERS 16

enum _talker_id : uint8_t {
	TOWN_SMITH,
	TOWN_HEALER,
	TOWN_DEADGUY,
	TOWN_TAVERN,
	TOWN_STORY,
	TOWN_DRUNK,
	TOWN_WITCH,
	TOWN_BMAID,
	TOWN_PEGBOY,
	TOWN_COW,
	TOWN_FARMER,
	TOWN_GIRL,
	TOWN_COWFARM,
	NUM_TOWNER_TYPES,
};

struct TownerStruct {
	byte *_tNAnim[8];
	std::unique_ptr<byte[]> _tNData;
	byte *_tAnimData;
	/** Used to get a voice line and text related to active quests when the player speaks to a town npc */
	int16_t _tSeed;
	/** Tile position of NPC */
	Point position;
	int16_t _tAnimWidth;
	int16_t _tAnimDelay; // Tick length of each frame in the current animation
	int16_t _tAnimCnt;   // Increases by one each game tick, counting how close we are to _pAnimDelay
	uint8_t _tAnimLen;   // Number of frames in current animation
	uint8_t _tAnimFrame; // Current frame of animation.
	uint8_t _tAnimFrameCnt;
	string_view _tName;
	/** Specifies the animation frame sequence. */
	const uint8_t *animOrder; // unowned
	std::size_t animOrderSize;
	void (*talk)(PlayerStruct &player, TownerStruct &barOwner);
	bool _tbtcnt;
	_talker_id _ttype;
};

extern TownerStruct Towners[NUM_TOWNERS];

void InitTowners();
void FreeTownerGFX();
void ProcessTowners();
void TalkToTowner(PlayerStruct &player, int t);

extern _speech_id QuestDialogTable[NUM_TOWNER_TYPES][MAXQUESTS];

} // namespace devilution
