/**
 * @file portal.h
 *
 * Interface of functionality for handling town portals.
 */
#pragma once

#include "engine/point.hpp"
#include "gendung.h"

namespace devilution {

#define MAXPORTAL 4

struct PortalStruct {
	bool open;
	Point position;
	int level;
	dungeon_type ltype;
	bool setlvl;
};

extern PortalStruct Portals[MAXPORTAL];

void InitPortals();
void SetPortalStats(int i, bool o, int x, int y, int lvl, dungeon_type lvltype);
void AddWarpMissile(int i, int x, int y);
void SyncPortals();
void AddInTownPortal(int i);
void ActivatePortal(int i, int x, int y, int lvl, dungeon_type lvltype, bool sp);
void DeactivatePortal(int i);
bool PortalOnLevel(int i);
void RemovePortalMissile(int id);
void SetCurrentPortal(int p);
void GetPortalLevel();
void GetPortalLvlPos();
bool PosOkPortal(int lvl, int x, int y);

} // namespace devilution
