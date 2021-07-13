/**
 * @file quests.cpp
 *
 * Interface of functionality for handling quests.
 */
#pragma once

#include <cstdint>

#include "engine.h"
#include "engine/cel_sprite.hpp"
#include "engine/point.hpp"
#include "gendung.h"
#include "textdat.h"
#include "utils/stdcompat/optional.hpp"

namespace devilution {

#define MAXQUESTS 24

/** States of the mushroom quest */
enum {
	QS_INIT,
	QS_TOMESPAWNED,
	QS_TOMEGIVEN,
	QS_MUSHSPAWNED,
	QS_MUSHPICKED,
	QS_MUSHGIVEN,
	QS_BRAINSPAWNED,
	QS_BRAINGIVEN,
};

enum quest_state : uint8_t {
	QUEST_NOTAVAIL, // quest did not spawn this game
	QUEST_INIT,     // quest has spawned, waiting to trigger
	QUEST_ACTIVE,   // quest is currently in progress
	QUEST_DONE,     // quest log closed and finished
	QUEST_HIVE_TEASE1 = 7,
	QUEST_HIVE_TEASE2,
	QUEST_HIVE_ACTIVE,
	QUEST_HIVE_DONE,
	QUEST_INVALID = 0xFF,
};

struct QuestStruct {
	uint8_t _qlevel;
	uint8_t _qtype;
	quest_state _qactive;
	dungeon_type _qlvltype;
	Point position;
	_setlevels _qslvl;
	uint8_t _qidx;
	_speech_id _qmsg;
	uint8_t _qvar1;
	uint8_t _qvar2;
	bool _qlog;
};

struct QuestDataStruct {
	uint8_t _qdlvl;
	int8_t _qdmultlvl;
	dungeon_type _qlvlt;
	uint8_t _qdtype;
	uint8_t _qdrnd;
	_setlevels _qslvl;
	bool isSinglePlayerOnly;
	_speech_id _qdmsg;
	const char *_qlstr;
};

extern bool QuestLogIsOpen;
extern std::optional<CelSprite> pQLogCel;
extern QuestStruct Quests[MAXQUESTS];
extern int ReturnLvlX;
extern int ReturnLvlY;
extern dungeon_type ReturnLevelType;
extern int ReturnLevel;

void InitQuests();
void CheckQuests();
bool ForceQuests();
bool QuestStatus(int i);
void CheckQuestKill(int m, bool sendmsg);
void DRLG_CheckQuests(int x, int y);
void SetReturnLvlPos();
void GetReturnLvlPos();
void LoadPWaterPalette();
void UpdatePWaterPalette();
void ResyncMPQuests();
void ResyncQuests();
void DrawQuestLog(const Surface &out);
void StartQuestlog();
void QuestlogUp();
void QuestlogDown();
void QuestlogEnter();
void QuestlogESC();
void SetMultiQuest(int q, quest_state s, bool log, int v1);

/* rdata */
extern QuestDataStruct QuestData[];

} // namespace devilution
