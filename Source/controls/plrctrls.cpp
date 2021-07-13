#include "controls/plrctrls.h"

#include <algorithm>
#include <cstdint>
#include <list>

#include "automap.h"
#include "control.h"
#include "controls/controller.h"
#include "controls/controller_motion.h"
#include "controls/game_controls.h"
#include "cursor.h"
#include "doom.h"
#include "engine/point.hpp"
#include "gmenu.h"
#include "help.h"
#include "inv.h"
#include "minitext.h"
#include "missiles.h"
#include "stores.h"
#include "towners.h"
#include "trigs.h"

#define SPLICONLENGTH 56

namespace devilution {

bool sgbControllerActive = false;
Point speedspellscoords[50];
int speedspellcount = 0;

/**
 * Native game menu, controlled by simulating a keyboard.
 */
bool InGameMenu()
{
	return stextflag != STORE_NONE
	    || HelpFlag
	    || talkflag
	    || qtextflag
	    || gmenu_is_active()
	    || PauseMode == 2
	    || Players[MyPlayerId]._pInvincible;
}

namespace {

int Slot = SLOTXY_INV_FIRST;

/**
 * Number of angles to turn to face the coordinate
 * @param destination Tile coordinates
 * @return -1 == down
 */
int GetRotaryDistance(Point destination)
{
	auto &myPlayer = Players[MyPlayerId];

	if (myPlayer.position.future == destination)
		return -1;

	int d1 = myPlayer._pdir;
	int d2 = GetDirection(myPlayer.position.future, destination);

	int d = abs(d1 - d2);
	if (d > 4)
		return 4 - (d % 4);

	return d;
}

/**
 * @brief Get the best case walking steps to coordinates
 * @param Position Tile coordinates
 */
int GetMinDistance(Point position)
{
	return Players[MyPlayerId].position.future.WalkingDistance(position);
}

/**
 * @brief Get walking steps to coordinate
 * @param destination Tile coordinates
 * @param maxDistance the max number of steps to search
 * @return number of steps, or 0 if not reachable
 */
int GetDistance(Point destination, int maxDistance)
{
	if (GetMinDistance(destination) > maxDistance) {
		return 0;
	}

	int8_t walkpath[MAX_PATH_LENGTH];
	int steps = FindPath(PosOkPlayer, MyPlayerId, Players[MyPlayerId].position.future.x, Players[MyPlayerId].position.future.y, destination.x, destination.y, walkpath);
	if (steps > maxDistance)
		return 0;

	return steps;
}

/**
 * @brief Get distance to coordinate
 * @param destination Tile coordinates
 */
int GetDistanceRanged(Point destination)
{
	return Players[MyPlayerId].position.future.ExactDistance(destination);
}

/**
 * @todo The loops in this function are iterating through tiles offset from the player position. This
 * could be accomplished by looping over the values in the Direction enum and making use of
 * Point math instead of nested loops from [-1, 1].
 */
void FindItemOrObject()
{
	int mx = Players[MyPlayerId].position.future.x;
	int my = Players[MyPlayerId].position.future.y;
	int rotations = 5;

	// As the player can not stand on the edge of the map this is safe from OOB
	for (int xx = -1; xx < 2; xx++) {
		for (int yy = -1; yy < 2; yy++) {
			if (dItem[mx + xx][my + yy] <= 0)
				continue;
			int i = dItem[mx + xx][my + yy] - 1;
			if (Items[i].isEmpty()
			    || Items[i]._iSelFlag == 0)
				continue;
			int newRotations = GetRotaryDistance({ mx + xx, my + yy });
			if (rotations < newRotations)
				continue;
			if (xx != 0 && yy != 0 && GetDistance({ mx + xx, my + yy }, 1) == 0)
				continue;
			rotations = newRotations;
			pcursitem = i;
			cursmx = mx + xx;
			cursmy = my + yy;
		}
	}

	if (leveltype == DTYPE_TOWN || pcursitem != -1)
		return; // Don't look for objects in town

	for (int xx = -1; xx < 2; xx++) {
		for (int yy = -1; yy < 2; yy++) {
			if (dObject[mx + xx][my + yy] == 0)
				continue;
			int o = dObject[mx + xx][my + yy] > 0 ? dObject[mx + xx][my + yy] - 1 : -(dObject[mx + xx][my + yy] + 1);
			if (Objects[o]._oSelFlag == 0)
				continue;
			if (xx == 0 && yy == 0 && Objects[o]._oDoorFlag)
				continue; // Ignore doorway so we don't get stuck behind barrels
			int newRotations = GetRotaryDistance({ mx + xx, my + yy });
			if (rotations < newRotations)
				continue;
			if (xx != 0 && yy != 0 && GetDistance({ mx + xx, my + yy }, 1) == 0)
				continue;
			rotations = newRotations;
			pcursobj = o;
			cursmx = mx + xx;
			cursmy = my + yy;
		}
	}
}

void CheckTownersNearby()
{
	for (int i = 0; i < 16; i++) {
		int distance = GetDistance(Towners[i].position, 2);
		if (distance == 0)
			continue;
		pcursmonst = i;
	}
}

bool HasRangedSpell()
{
	int spl = Players[MyPlayerId]._pRSpell;

	return spl != SPL_INVALID
	    && spl != SPL_TOWN
	    && spl != SPL_TELEPORT
	    && spelldata[spl].sTargeted
	    && !spelldata[spl].sTownSpell;
}

bool CanTargetMonster(int mi)
{
	const MonsterStruct &monst = Monsters[mi];

	if ((monst._mFlags & (MFLAG_HIDDEN | MFLAG_GOLEM)) != 0)
		return false;
	if (monst._mhitpoints >> 6 <= 0) // dead
		return false;

	const int mx = monst.position.tile.x;
	const int my = monst.position.tile.y;
	if ((dFlags[mx][my] & BFLAG_LIT) == 0) // not visible
		return false;
	if (dMonster[mx][my] == 0)
		return false;

	return true;
}

void FindRangedTarget()
{
	int rotations = 0;
	int distance = 0;
	bool canTalk = false;

	// The first MAX_PLRS monsters are reserved for players' golems.
	for (int mi = MAX_PLRS; mi < MAXMONSTERS; mi++) {
		if (!CanTargetMonster(mi))
			continue;

		const bool newCanTalk = CanTalkToMonst(mi);
		if (pcursmonst != -1 && !canTalk && newCanTalk)
			continue;
		const int newDdistance = GetDistanceRanged(Monsters[mi].position.future);
		const int newRotations = GetRotaryDistance(Monsters[mi].position.future);
		if (pcursmonst != -1 && canTalk == newCanTalk) {
			if (distance < newDdistance)
				continue;
			if (distance == newDdistance && rotations < newRotations)
				continue;
		}
		distance = newDdistance;
		rotations = newRotations;
		canTalk = newCanTalk;
		pcursmonst = mi;
	}
}

void FindMeleeTarget()
{
	bool visited[MAXDUNX][MAXDUNY] = { {} };
	int maxSteps = 25; // Max steps for FindPath is 25
	int rotations = 0;
	bool canTalk = false;

	struct SearchNode {
		int x, y;
		int steps;
	};
	std::list<SearchNode> queue;

	{
		const int startX = Players[MyPlayerId].position.future.x;
		const int startY = Players[MyPlayerId].position.future.y;
		visited[startX][startY] = true;
		queue.push_back({ startX, startY, 0 });
	}

	while (!queue.empty()) {
		SearchNode node = queue.front();
		queue.pop_front();

		for (auto pathDir : PathDirs) {
			const int dx = node.x + pathDir.deltaX;
			const int dy = node.y + pathDir.deltaY;

			if (visited[dx][dy])
				continue; // already visisted

			if (node.steps > maxSteps) {
				visited[dx][dy] = true;
				continue;
			}

			if (!PosOkPlayer(MyPlayerId, { dx, dy })) {
				visited[dx][dy] = true;

				if (dMonster[dx][dy] != 0) {
					const int mi = dMonster[dx][dy] > 0 ? dMonster[dx][dy] - 1 : -(dMonster[dx][dy] + 1);
					if (CanTargetMonster(mi)) {
						const bool newCanTalk = CanTalkToMonst(mi);
						if (pcursmonst != -1 && !canTalk && newCanTalk)
							continue;
						const int newRotations = GetRotaryDistance({ dx, dy });
						if (pcursmonst != -1 && canTalk == newCanTalk && rotations < newRotations)
							continue;
						rotations = newRotations;
						canTalk = newCanTalk;
						pcursmonst = mi;
						if (!canTalk)
							maxSteps = node.steps; // Monsters found, cap search to current steps
					}
				}

				continue;
			}

			PATHNODE pPath;
			pPath.position = { node.x, node.y };

			if (path_solid_pieces(&pPath, dx, dy)) {
				queue.push_back({ dx, dy, node.steps + 1 });
				visited[dx][dy] = true;
			}
		}
	}
}

void CheckMonstersNearby()
{
	if (Players[MyPlayerId]._pwtype == WT_RANGED || HasRangedSpell()) {
		FindRangedTarget();
		return;
	}

	FindMeleeTarget();
}

void CheckPlayerNearby()
{
	int newDdistance;
	int rotations = 0;
	int distance = 0;

	if (pcursmonst != -1)
		return;

	auto &myPlayer = Players[MyPlayerId];

	int spl = myPlayer._pRSpell;
	if (gbFriendlyMode && spl != SPL_RESURRECT && spl != SPL_HEALOTHER)
		return;

	for (int i = 0; i < MAX_PLRS; i++) {
		if (i == MyPlayerId)
			continue;
		const auto &player = Players[i];
		const int mx = player.position.future.x;
		const int my = player.position.future.y;
		if (dPlayer[mx][my] == 0
		    || (dFlags[mx][my] & BFLAG_LIT) == 0
		    || (player._pHitPoints == 0 && spl != SPL_RESURRECT))
			continue;

		if (myPlayer._pwtype == WT_RANGED || HasRangedSpell() || spl == SPL_HEALOTHER) {
			newDdistance = GetDistanceRanged(player.position.future);
		} else {
			newDdistance = GetDistance(player.position.future, distance);
			if (newDdistance == 0)
				continue;
		}

		if (pcursplr != -1 && distance < newDdistance)
			continue;
		const int newRotations = GetRotaryDistance(player.position.future);
		if (pcursplr != -1 && distance == newDdistance && rotations < newRotations)
			continue;

		distance = newDdistance;
		rotations = newRotations;
		pcursplr = i;
	}
}

void FindActor()
{
	if (leveltype != DTYPE_TOWN)
		CheckMonstersNearby();
	else
		CheckTownersNearby();

	if (gbIsMultiplayer)
		CheckPlayerNearby();
}

int pcursmissile;
int pcurstrig;
int pcursquest;

void FindTrigger()
{
	int rotations = 0;
	int distance = 0;

	if (pcursitem != -1 || pcursobj != -1)
		return; // Prefer showing items/objects over triggers (use of cursm* conflicts)

	for (int i = 0; i < ActiveMissileCount; i++) {
		int mi = ActiveMissiles[i];
		if (Missiles[mi]._mitype == MIS_TOWN || Missiles[mi]._mitype == MIS_RPORTAL) {
			const int newDistance = GetDistance(Missiles[mi].position.tile, 2);
			if (newDistance == 0)
				continue;
			if (pcursmissile != -1 && distance < newDistance)
				continue;
			const int newRotations = GetRotaryDistance(Missiles[mi].position.tile);
			if (pcursmissile != -1 && distance == newDistance && rotations < newRotations)
				continue;
			cursmx = Missiles[mi].position.tile.x;
			cursmy = Missiles[mi].position.tile.y;
			pcursmissile = mi;
			distance = newDistance;
			rotations = newRotations;
		}
	}

	if (pcursmissile == -1) {
		for (int i = 0; i < numtrigs; i++) {
			int tx = trigs[i].position.x;
			int ty = trigs[i].position.y;
			if (trigs[i]._tlvl == 13)
				ty -= 1;
			const int newDistance = GetDistance({ tx, ty }, 2);
			if (newDistance == 0)
				continue;
			cursmx = tx;
			cursmy = ty;
			pcurstrig = i;
		}

		if (pcurstrig == -1) {
			for (int i = 0; i < MAXQUESTS; i++) {
				if (i == Q_BETRAYER || currlevel != Quests[i]._qlevel || Quests[i]._qslvl == 0)
					continue;
				const int newDistance = GetDistance(Quests[i].position, 2);
				if (newDistance == 0)
					continue;
				cursmx = Quests[i].position.x;
				cursmy = Quests[i].position.y;
				pcursquest = i;
			}
		}
	}

	if (pcursmonst != -1 || pcursplr != -1 || cursmx == -1 || cursmy == -1)
		return; // Prefer monster/player info text

	CheckTrigForce();
	CheckTown();
	CheckRportal();
}

void Interact()
{
	if (leveltype == DTYPE_TOWN && pcursmonst != -1) {
		NetSendCmdLocParam1(true, CMD_TALKXY, Towners[pcursmonst].position, pcursmonst);
	} else if (pcursmonst != -1) {
		if (Players[MyPlayerId]._pwtype != WT_RANGED || CanTalkToMonst(pcursmonst)) {
			NetSendCmdParam1(true, CMD_ATTACKID, pcursmonst);
		} else {
			NetSendCmdParam1(true, CMD_RATTACKID, pcursmonst);
		}
	} else if (leveltype != DTYPE_TOWN && pcursplr != -1 && !gbFriendlyMode) {
		NetSendCmdParam1(true, Players[MyPlayerId]._pwtype == WT_RANGED ? CMD_RATTACKPID : CMD_ATTACKPID, pcursplr);
	}
}

void AttrIncBtnSnap(AxisDirection dir)
{
	static AxisDirectionRepeater repeater;
	dir = repeater.Get(dir);
	if (dir.y == AxisDirectionY_NONE)
		return;

	if (chrbtnactive && Players[MyPlayerId]._pStatPts <= 0)
		return;

	// first, find our cursor location
	int slot = 0;
	for (int i = 0; i < 4; i++) {
		if (ChrBtnsRect[i].Contains(MousePosition)) {
			slot = i;
			break;
		}
	}

	if (dir.y == AxisDirectionY_UP) {
		if (slot > 0)
			--slot;
	} else if (dir.y == AxisDirectionY_DOWN) {
		if (slot < 3)
			++slot;
	}

	// move cursor to our new location
	int x = ChrBtnsRect[slot].position.x + (ChrBtnsRect[slot].size.width / 2);
	int y = ChrBtnsRect[slot].position.y + (ChrBtnsRect[slot].size.height / 2);
	SetCursorPos(x, y);
}

Point InvGetEquipSlotCoord(const inv_body_loc invSlot)
{
	Point result { RIGHT_PANEL, 0 };
	result.x -= (icursW28 - 1) * (InventorySlotSizeInPixels.width / 2);
	switch (invSlot) {
	case INVLOC_HEAD:
		result.x += ((InvRect[SLOTXY_HEAD_FIRST].x + InvRect[SLOTXY_HEAD_LAST].x) / 2);
		result.y += ((InvRect[SLOTXY_HEAD_FIRST].y + InvRect[SLOTXY_HEAD_LAST].y) / 2);
		break;
	case INVLOC_RING_LEFT:
		result.x += InvRect[SLOTXY_RING_LEFT].x;
		result.y += InvRect[SLOTXY_RING_LEFT].y;
		break;
	case INVLOC_RING_RIGHT:
		result.x += InvRect[SLOTXY_RING_RIGHT].x;
		result.y += InvRect[SLOTXY_RING_RIGHT].y;
		break;
	case INVLOC_AMULET:
		result.x += InvRect[SLOTXY_AMULET].x;
		result.y += InvRect[SLOTXY_AMULET].y;
		break;
	case INVLOC_HAND_LEFT:
		result.x += ((InvRect[SLOTXY_HAND_LEFT_FIRST].x + InvRect[SLOTXY_HAND_LEFT_LAST].x) / 2);
		result.y += ((InvRect[SLOTXY_HAND_LEFT_FIRST].y + InvRect[SLOTXY_HAND_LEFT_LAST].y) / 2);
		break;
	case INVLOC_HAND_RIGHT:
		result.x += ((InvRect[SLOTXY_HAND_RIGHT_FIRST].x + InvRect[SLOTXY_HAND_RIGHT_LAST].x) / 2);
		result.y += ((InvRect[SLOTXY_HAND_RIGHT_FIRST].y + InvRect[SLOTXY_HAND_RIGHT_LAST].y) / 2);
		break;
	case INVLOC_CHEST:
		result.x += ((InvRect[SLOTXY_CHEST_FIRST].x + InvRect[SLOTXY_CHEST_LAST].x) / 2);
		result.y += ((InvRect[SLOTXY_CHEST_FIRST].y + InvRect[SLOTXY_CHEST_LAST].y) / 2);
		break;
	default:
		break;
	}

	return result;
}

Point InvGetEquipSlotCoordFromInvSlot(const inv_xy_slot slot)
{
	switch (slot) {
	case SLOTXY_HEAD_FIRST:
	case SLOTXY_HEAD_LAST:
		return InvGetEquipSlotCoord(INVLOC_HEAD);
	case SLOTXY_RING_LEFT:
		return InvGetEquipSlotCoord(INVLOC_RING_LEFT);
	case SLOTXY_RING_RIGHT:
		return InvGetEquipSlotCoord(INVLOC_RING_RIGHT);
	case SLOTXY_AMULET:
		return InvGetEquipSlotCoord(INVLOC_AMULET);
	case SLOTXY_HAND_LEFT_FIRST:
	case SLOTXY_HAND_LEFT_LAST:
		return InvGetEquipSlotCoord(INVLOC_HAND_LEFT);
	case SLOTXY_HAND_RIGHT_FIRST:
	case SLOTXY_HAND_RIGHT_LAST:
		return InvGetEquipSlotCoord(INVLOC_HAND_RIGHT);
	case SLOTXY_CHEST_FIRST:
	case SLOTXY_CHEST_LAST:
		return InvGetEquipSlotCoord(INVLOC_CHEST);
	default:
		return {};
	}
}

/**
 * Get coordinates for a given inventory slot (for belt use BeltGetSlotCoord)
 */
Point InvGetSlotCoord(int slot)
{
	assert(slot <= SLOTXY_INV_LAST);
	return { InvRect[slot].x + RIGHT_PANEL, InvRect[slot].y };
}

/**
 * Get coordinates for a given belt slot (for normal inventory use InvGetSlotCoord)
 */
Point BeltGetSlotCoord(int slot)
{
	assert(slot >= SLOTXY_BELT_FIRST && slot <= SLOTXY_BELT_LAST);
	return { InvRect[slot].x + PANEL_LEFT, InvRect[slot].y + PANEL_TOP };
}

/**
 * Get item size (grid size) on the slot specified. Returns 1x1 if none exists.
 */
Size GetItemSizeOnSlot(int slot, char &itemInvId)
{
	if (slot >= SLOTXY_INV_FIRST && slot <= SLOTXY_INV_LAST) {
		int ig = slot - SLOTXY_INV_FIRST;
		auto &myPlayer = Players[MyPlayerId];
		int8_t ii = myPlayer.InvGrid[ig];
		if (ii != 0) {
			int8_t iv = ii;
			if (ii <= 0) {
				iv = -ii;
			}

			ItemStruct &item = myPlayer.InvList[iv - 1];
			if (!item.isEmpty()) {
				auto size = GetInvItemSize(item._iCurs + CURSOR_FIRSTITEM);
				size.width /= InventorySlotSizeInPixels.width;
				size.height /= InventorySlotSizeInPixels.height;

				itemInvId = ii;
				return size;
			}
		}
	}
	itemInvId = 0;
	return { 1, 1 };
}

/**
 * Reset cursor position based on the current slot.
 */
void ResetInvCursorPosition()
{
	Point mousePos {};
	if (Slot < SLOTXY_INV_FIRST) {
		mousePos = InvGetEquipSlotCoordFromInvSlot((inv_xy_slot)Slot);
	} else if (Slot < SLOTXY_BELT_FIRST) {
		char itemInvId;
		auto itemSize = GetItemSizeOnSlot(Slot, itemInvId);

		// search the 'first slot' for that item in the inventory, it should have the positive number of that same InvId
		if (itemInvId < 0) {
			for (int s = 0; s < SLOTXY_INV_LAST - SLOTXY_INV_FIRST; ++s) {
				if (Players[MyPlayerId].InvGrid[s] == -itemInvId) {
					Slot = SLOTXY_INV_FIRST + s;
					break;
				}
			}
		}

		// offset the slot to always move to the top-left most slot of that item
		Slot -= ((itemSize.height - 1) * INV_ROW_SLOT_SIZE);
		mousePos = InvGetSlotCoord(Slot);
		mousePos.x += ((itemSize.width - 1) * InventorySlotSizeInPixels.width) / 2;
		mousePos.y += ((itemSize.height - 1) * InventorySlotSizeInPixels.height) / 2;
	} else {
		mousePos = BeltGetSlotCoord(Slot);
	}

	mousePos.x += (InventorySlotSizeInPixels.width / 2);
	mousePos.y -= (InventorySlotSizeInPixels.height / 2);

	SetCursorPos(mousePos.x, mousePos.y);
}

/**
 * Move the cursor around in our inventory
 * If mouse coords are at SLOTXY_CHEST_LAST, consider this center of equipment
 * small inventory squares are 29x29 (roughly)
 */
void InvMove(AxisDirection dir)
{
	static AxisDirectionRepeater repeater(/*min_interval_ms=*/150);
	dir = repeater.Get(dir);
	if (dir.x == AxisDirectionX_NONE && dir.y == AxisDirectionY_NONE)
		return;

	char itemInvId;
	auto itemSize = GetItemSizeOnSlot(Slot, itemInvId);

	Point mousePos = MousePosition;

	const bool isHoldingItem = pcurs > 1;

	// normalize slots
	if (Slot < 0)
		Slot = 0;
	else if (Slot >= SLOTXY_HEAD_FIRST && Slot <= SLOTXY_HEAD_LAST)
		Slot = SLOTXY_HEAD_FIRST;
	else if (Slot >= SLOTXY_HAND_LEFT_FIRST && Slot <= SLOTXY_HAND_LEFT_LAST)
		Slot = SLOTXY_HAND_LEFT_FIRST;
	else if (Slot >= SLOTXY_CHEST_FIRST && Slot <= SLOTXY_CHEST_LAST)
		Slot = SLOTXY_CHEST_FIRST;
	else if (Slot >= SLOTXY_HAND_RIGHT_FIRST && Slot <= SLOTXY_HAND_RIGHT_LAST)
		Slot = SLOTXY_HAND_RIGHT_FIRST;
	else if (Slot > SLOTXY_BELT_LAST)
		Slot = SLOTXY_BELT_LAST;

	const int initialSlot = Slot;
	auto &myPlayer = Players[MyPlayerId];

	// when item is on cursor (pcurs > 1), this is the real cursor XY
	if (dir.x == AxisDirectionX_LEFT) {
		if (isHoldingItem) {
			if (Slot >= SLOTXY_INV_FIRST && Slot <= SLOTXY_INV_LAST) {
				if (Slot == SLOTXY_INV_ROW1_FIRST || Slot == SLOTXY_INV_ROW2_FIRST || Slot == SLOTXY_INV_ROW3_FIRST || Slot == SLOTXY_INV_ROW4_FIRST) {
					Slot += INV_ROW_SLOT_SIZE - icursW28;
				} else {
					Slot -= 1;
				}
				mousePos = InvGetSlotCoord(Slot);
			} else if (Slot > SLOTXY_BELT_FIRST && Slot <= SLOTXY_BELT_LAST) {
				Slot -= 1;
				mousePos = BeltGetSlotCoord(Slot);
			} else if (myPlayer.HoldItem._itype == ITYPE_RING) {
				Slot = SLOTXY_RING_LEFT;
				mousePos = InvGetEquipSlotCoord(INVLOC_RING_LEFT);
			} else if (myPlayer.HoldItem.isWeapon() || myPlayer.HoldItem.isShield()) {
				if (Slot == SLOTXY_HAND_LEFT_FIRST) {
					Slot = SLOTXY_HAND_RIGHT_FIRST;
					mousePos = InvGetEquipSlotCoord(INVLOC_HAND_RIGHT);
				} else if (Slot == SLOTXY_HAND_RIGHT_FIRST) {
					Slot = SLOTXY_HAND_LEFT_FIRST;
					mousePos = InvGetEquipSlotCoord(INVLOC_HAND_LEFT);
				}
			}
		} else {
			if (Slot == SLOTXY_HAND_RIGHT_FIRST) {
				Slot = SLOTXY_CHEST_FIRST;
				mousePos = InvGetEquipSlotCoord(INVLOC_CHEST);
			} else if (Slot == SLOTXY_CHEST_FIRST) {
				Slot = SLOTXY_HAND_LEFT_FIRST;
				mousePos = InvGetEquipSlotCoord(INVLOC_HAND_LEFT);
			} else if (Slot == SLOTXY_AMULET) {
				Slot = SLOTXY_HEAD_FIRST;
				mousePos = InvGetEquipSlotCoord(INVLOC_HEAD);
			} else if (Slot == SLOTXY_RING_RIGHT) {
				Slot = SLOTXY_RING_LEFT;
				mousePos = InvGetEquipSlotCoord(INVLOC_RING_LEFT);
			} else if (Slot >= SLOTXY_INV_FIRST && Slot <= SLOTXY_INV_LAST) {
				if (Slot == SLOTXY_INV_ROW1_FIRST || Slot == SLOTXY_INV_ROW2_FIRST || Slot == SLOTXY_INV_ROW3_FIRST || Slot == SLOTXY_INV_ROW4_FIRST) {
					Slot += INV_ROW_SLOT_SIZE - 1;
				} else {
					Slot -= 1;
				}
				mousePos = InvGetSlotCoord(Slot);
			} else if (Slot > SLOTXY_BELT_FIRST && Slot <= SLOTXY_BELT_LAST) {
				Slot -= 1;
				mousePos = BeltGetSlotCoord(Slot);
			}
		}
	} else if (dir.x == AxisDirectionX_RIGHT) {
		if (isHoldingItem) {
			if (Slot >= SLOTXY_INV_FIRST && Slot <= SLOTXY_INV_LAST) {
				if (
				    Slot == SLOTXY_INV_ROW1_LAST + 1 - icursW28 || Slot == SLOTXY_INV_ROW2_LAST + 1 - icursW28 || Slot == SLOTXY_INV_ROW3_LAST + 1 - icursW28 || Slot == SLOTXY_INV_ROW4_LAST + 1 - icursW28) {
					Slot -= INV_ROW_SLOT_SIZE - icursW28;
				} else {
					Slot += 1;
				}
				mousePos = InvGetSlotCoord(Slot);
			} else if (Slot >= SLOTXY_BELT_FIRST && Slot < SLOTXY_BELT_LAST) {
				Slot += 1;
				mousePos = BeltGetSlotCoord(Slot);
			} else if (myPlayer.HoldItem._itype == ITYPE_RING) {
				Slot = SLOTXY_RING_RIGHT;
				mousePos = InvGetEquipSlotCoord(INVLOC_RING_RIGHT);
			} else if (myPlayer.HoldItem.isWeapon() || myPlayer.HoldItem.isShield()) {
				if (Slot == SLOTXY_HAND_LEFT_FIRST) {
					Slot = SLOTXY_HAND_RIGHT_FIRST;
					mousePos = InvGetEquipSlotCoord(INVLOC_HAND_RIGHT);
				} else if (Slot == SLOTXY_HAND_RIGHT_FIRST) {
					Slot = SLOTXY_HAND_LEFT_FIRST;
					mousePos = InvGetEquipSlotCoord(INVLOC_HAND_LEFT);
				}
			}
		} else {
			if (Slot == SLOTXY_RING_LEFT) {
				Slot = SLOTXY_RING_RIGHT;
				mousePos = InvGetEquipSlotCoord(INVLOC_RING_RIGHT);
			} else if (Slot == SLOTXY_HAND_LEFT_FIRST) {
				Slot = SLOTXY_CHEST_FIRST;
				mousePos = InvGetEquipSlotCoord(INVLOC_CHEST);
			} else if (Slot == SLOTXY_CHEST_FIRST) {
				Slot = SLOTXY_HAND_RIGHT_FIRST;
				mousePos = InvGetEquipSlotCoord(INVLOC_HAND_RIGHT);
			} else if (Slot == SLOTXY_HEAD_FIRST) {
				Slot = SLOTXY_AMULET;
				mousePos = InvGetEquipSlotCoord(INVLOC_AMULET);
			} else if (Slot >= SLOTXY_INV_FIRST && Slot <= SLOTXY_INV_LAST) {
				if (
				    Slot == SLOTXY_INV_ROW1_LAST + 1 - itemSize.width || Slot == SLOTXY_INV_ROW2_LAST + 1 - itemSize.width || Slot == SLOTXY_INV_ROW3_LAST + 1 - itemSize.width || Slot == SLOTXY_INV_ROW4_LAST + 1 - itemSize.width) {
					Slot -= INV_ROW_SLOT_SIZE - itemSize.width;
				} else {
					Slot += itemSize.width;
				}
				mousePos = InvGetSlotCoord(Slot);
			} else if (Slot >= SLOTXY_BELT_FIRST && Slot < SLOTXY_BELT_LAST) {
				Slot += 1;
				mousePos = BeltGetSlotCoord(Slot);
			}
		}
	}
	if (dir.y == AxisDirectionY_UP) {
		if (isHoldingItem) {
			if (Slot >= SLOTXY_INV_ROW2_FIRST) { // general inventory
				Slot -= INV_ROW_SLOT_SIZE;
				mousePos = InvGetSlotCoord(Slot);
			} else if (Slot >= SLOTXY_INV_FIRST) {
				if (myPlayer.HoldItem._itype == ITYPE_RING) {
					if (Slot >= SLOTXY_INV_ROW1_FIRST && Slot <= SLOTXY_INV_ROW1_FIRST + (INV_ROW_SLOT_SIZE / 2) - 1) {
						Slot = SLOTXY_RING_LEFT;
						mousePos = InvGetEquipSlotCoord(INVLOC_RING_LEFT);
					} else {
						Slot = SLOTXY_RING_RIGHT;
						mousePos = InvGetEquipSlotCoord(INVLOC_RING_RIGHT);
					}
				} else if (myPlayer.HoldItem.isWeapon()) {
					Slot = SLOTXY_HAND_LEFT_FIRST;
					mousePos = InvGetEquipSlotCoord(INVLOC_HAND_LEFT);
				} else if (myPlayer.HoldItem.isShield()) {
					Slot = SLOTXY_HAND_RIGHT_FIRST;
					mousePos = InvGetEquipSlotCoord(INVLOC_HAND_RIGHT);
				} else if (myPlayer.HoldItem.isHelm()) {
					Slot = SLOTXY_HEAD_FIRST;
					mousePos = InvGetEquipSlotCoord(INVLOC_HEAD);
				} else if (myPlayer.HoldItem.isArmor()) {
					Slot = SLOTXY_CHEST_FIRST;
					mousePos = InvGetEquipSlotCoord(INVLOC_CHEST);
				} else if (myPlayer.HoldItem._itype == ITYPE_AMULET) {
					Slot = SLOTXY_AMULET;
					mousePos = InvGetEquipSlotCoord(INVLOC_AMULET);
				}
			}
		} else {
			if (Slot >= SLOTXY_INV_ROW1_FIRST && Slot < SLOTXY_INV_ROW1_FIRST + 3) { // first 3 general slots
				Slot = SLOTXY_RING_LEFT;
				mousePos = InvGetEquipSlotCoord(INVLOC_RING_LEFT);
			} else if (Slot >= SLOTXY_INV_ROW1_FIRST + 3 && Slot < SLOTXY_INV_ROW1_FIRST + 7) { // middle 4 general slots
				Slot = SLOTXY_CHEST_FIRST;
				mousePos = InvGetEquipSlotCoord(INVLOC_CHEST);
			} else if (Slot >= SLOTXY_INV_ROW1_FIRST + 7 && Slot < SLOTXY_INV_ROW1_LAST) { // last 3 general slots
				Slot = SLOTXY_RING_RIGHT;
				mousePos = InvGetEquipSlotCoord(INVLOC_RING_RIGHT);
			} else if (Slot == SLOTXY_CHEST_FIRST || Slot == SLOTXY_HAND_LEFT_FIRST) {
				Slot = SLOTXY_HEAD_FIRST;
				mousePos = InvGetEquipSlotCoord(INVLOC_HEAD);
			} else if (Slot == SLOTXY_RING_LEFT) {
				Slot = SLOTXY_HAND_LEFT_FIRST;
				mousePos = InvGetEquipSlotCoord(INVLOC_HAND_LEFT);
			} else if (Slot == SLOTXY_RING_RIGHT) {
				Slot = SLOTXY_HAND_RIGHT_FIRST;
				mousePos = InvGetEquipSlotCoord(INVLOC_HAND_RIGHT);
			} else if (Slot == SLOTXY_HAND_RIGHT_FIRST) {
				Slot = SLOTXY_AMULET;
				mousePos = InvGetEquipSlotCoord(INVLOC_AMULET);
			} else if (Slot >= SLOTXY_INV_ROW2_FIRST) {
				Slot -= INV_ROW_SLOT_SIZE;
				mousePos = InvGetSlotCoord(Slot);
			}
		}
	} else if (dir.y == AxisDirectionY_DOWN) {
		if (isHoldingItem) {
			if (Slot == SLOTXY_HEAD_FIRST || Slot == SLOTXY_CHEST_FIRST) {
				Slot = SLOTXY_INV_ROW1_FIRST + 4;
				mousePos = InvGetSlotCoord(Slot);
			} else if (Slot == SLOTXY_RING_LEFT || Slot == SLOTXY_HAND_LEFT_FIRST) {
				Slot = SLOTXY_INV_ROW1_FIRST + 1;
				mousePos = InvGetSlotCoord(Slot);
			} else if (Slot == SLOTXY_RING_RIGHT || Slot == SLOTXY_HAND_RIGHT_FIRST || Slot == SLOTXY_AMULET) {
				Slot = SLOTXY_INV_ROW1_LAST - 1;
				mousePos = InvGetSlotCoord(Slot);
			} else if (Slot <= (SLOTXY_INV_ROW4_LAST - (icursH28 * INV_ROW_SLOT_SIZE))) {
				Slot += INV_ROW_SLOT_SIZE;
				mousePos = InvGetSlotCoord(Slot);
			} else if (Slot <= SLOTXY_INV_LAST && myPlayer.HoldItem._itype == ITYPE_MISC && icursW28 == 1 && icursH28 == 1) { // forcing only 1x1 misc items
				Slot += INV_ROW_SLOT_SIZE;
				if (Slot > SLOTXY_BELT_LAST)
					Slot = SLOTXY_BELT_LAST;
				mousePos = BeltGetSlotCoord(Slot);
			}
		} else {
			if (Slot == SLOTXY_HEAD_FIRST) {
				Slot = SLOTXY_CHEST_FIRST;
				mousePos = InvGetEquipSlotCoord(INVLOC_CHEST);
			} else if (Slot == SLOTXY_CHEST_FIRST) {
				Slot = SLOTXY_INV_ROW1_FIRST + (INV_ROW_SLOT_SIZE / 2);
				mousePos = InvGetSlotCoord(Slot);
			} else if (Slot == SLOTXY_HAND_LEFT_FIRST) {
				Slot = SLOTXY_RING_LEFT;
				mousePos = InvGetEquipSlotCoord(INVLOC_RING_LEFT);
			} else if (Slot == SLOTXY_RING_LEFT) {
				Slot = SLOTXY_INV_ROW1_FIRST + 1;
				mousePos = InvGetSlotCoord(Slot);
			} else if (Slot == SLOTXY_RING_RIGHT) {
				Slot = SLOTXY_INV_ROW1_LAST - 1;
				mousePos = InvGetSlotCoord(Slot);
			} else if (Slot == SLOTXY_AMULET) {
				Slot = SLOTXY_HAND_RIGHT_FIRST;
				mousePos = InvGetEquipSlotCoord(INVLOC_HAND_RIGHT);
			} else if (Slot == SLOTXY_HAND_RIGHT_FIRST) {
				Slot = SLOTXY_RING_RIGHT;
				mousePos = InvGetEquipSlotCoord(INVLOC_RING_RIGHT);
			} else if (Slot <= (SLOTXY_INV_ROW4_LAST - (itemSize.height * INV_ROW_SLOT_SIZE))) {
				Slot += itemSize.height * INV_ROW_SLOT_SIZE;
				mousePos = InvGetSlotCoord(Slot);
			} else if (Slot <= SLOTXY_INV_LAST) {
				Slot += itemSize.height * INV_ROW_SLOT_SIZE;
				if (Slot > SLOTXY_BELT_LAST)
					Slot = SLOTXY_BELT_LAST;
				mousePos = BeltGetSlotCoord(Slot);
			}
		}
	}

	// no movement was made
	if (Slot == initialSlot)
		return;

	// get item under new slot if navigating on the inventory
	if (!isHoldingItem && Slot >= SLOTXY_INV_FIRST && Slot <= SLOTXY_INV_LAST) {
		itemSize = GetItemSizeOnSlot(Slot, itemInvId);

		// search the 'first slot' for that item in the inventory, it should have the positive number of that same InvId
		if (itemInvId < 0) {
			for (int s = 0; s < SLOTXY_INV_LAST - SLOTXY_INV_FIRST; ++s) {
				if (myPlayer.InvGrid[s] == -itemInvId) {
					Slot = SLOTXY_INV_FIRST + s;
					break;
				}
			}
		}

		// offset the slot to always move to the top-left most slot of that item
		Slot -= ((itemSize.height - 1) * INV_ROW_SLOT_SIZE);
		mousePos = InvGetSlotCoord(Slot);
		mousePos.x += ((itemSize.width - 1) * InventorySlotSizeInPixels.width) / 2;
		mousePos.y += ((itemSize.height - 1) * InventorySlotSizeInPixels.height) / 2;
	}

	// move cursor to the center of the slot if not holding anything or top left is holding an object
	if (isHoldingItem) {
		if (Slot >= SLOTXY_INV_FIRST)
			mousePos.y -= InventorySlotSizeInPixels.height;
		else
			mousePos.y -= (int)((icursH28 / 2.0) * InventorySlotSizeInPixels.height) + (InventorySlotSizeInPixels.height / 2);
	} else {
		mousePos.x += (InventorySlotSizeInPixels.width / 2);
		mousePos.y -= (InventorySlotSizeInPixels.height / 2);
	}

	if (mousePos == MousePosition) {
		return; // Avoid wobeling when scalled
	}

	SetCursorPos(mousePos.x, mousePos.y);
}

/**
 * check if hot spell at X Y exists
 */
bool HSExists(Point target)
{
	for (int r = 0; r < speedspellcount; r++) {
		if (target.x >= speedspellscoords[r].x - SPLICONLENGTH / 2
		    && target.x < speedspellscoords[r].x + SPLICONLENGTH / 2
		    && target.y >= speedspellscoords[r].y - SPLICONLENGTH / 2
		    && target.y < speedspellscoords[r].y + SPLICONLENGTH / 2) {
			return true;
		}
	}
	return false;
}

void HotSpellMove(AxisDirection dir)
{
	static AxisDirectionRepeater repeater;
	dir = repeater.Get(dir);
	if (dir.x == AxisDirectionX_NONE && dir.y == AxisDirectionY_NONE)
		return;

	int spbslot = Players[MyPlayerId]._pRSpell;
	for (int r = 0; r < speedspellcount; r++) {
		if (MousePosition.x >= speedspellscoords[r].x - SPLICONLENGTH / 2
		    && MousePosition.x < speedspellscoords[r].x + SPLICONLENGTH / 2
		    && MousePosition.y >= speedspellscoords[r].y - SPLICONLENGTH / 2
		    && MousePosition.y < speedspellscoords[r].y + SPLICONLENGTH / 2) {
			spbslot = r;
			break;
		}
	}

	Point newMousePosition = speedspellscoords[spbslot];

	if (dir.x == AxisDirectionX_LEFT) {
		if (spbslot < speedspellcount - 1) {
			newMousePosition = speedspellscoords[spbslot + 1];
		}
	} else if (dir.x == AxisDirectionX_RIGHT) {
		if (spbslot > 0) {
			newMousePosition = speedspellscoords[spbslot - 1];
		}
	}

	if (dir.y == AxisDirectionY_UP) {
		if (HSExists(newMousePosition - Displacement { 0, SPLICONLENGTH })) {
			newMousePosition.y -= SPLICONLENGTH;
		}
	} else if (dir.y == AxisDirectionY_DOWN) {
		if (HSExists(newMousePosition + Displacement { 0, SPLICONLENGTH })) {
			newMousePosition.y += SPLICONLENGTH;
		}
	}

	if (newMousePosition != MousePosition) {
		SetCursorPos(newMousePosition.x, newMousePosition.y);
	}
}

void SpellBookMove(AxisDirection dir)
{
	static AxisDirectionRepeater repeater;
	dir = repeater.Get(dir);

	if (dir.x == AxisDirectionX_LEFT) {
		if (sbooktab > 0)
			sbooktab--;
	} else if (dir.x == AxisDirectionX_RIGHT) {
		if ((gbIsHellfire && sbooktab < 4) || (!gbIsHellfire && sbooktab < 3))
			sbooktab++;
	}
}

const Direction FaceDir[3][3] = {
	// NONE      UP      DOWN
	{ DIR_OMNI, DIR_N, DIR_S }, // NONE
	{ DIR_W, DIR_NW, DIR_SW },  // LEFT
	{ DIR_E, DIR_NE, DIR_SE },  // RIGHT
};

/**
 * @brief check if stepping in direction (dir) from position is blocked.
 *
 * If you step from A to B, at leat one of the Xs need to be clear:
 *
 *  AX
 *  XB
 *
 *  @return true if step is blocked
 */
bool IsPathBlocked(Point position, Direction dir)
{
	Direction d1;
	Direction d2;

	switch (dir) {
	case DIR_N:
		d1 = DIR_NW;
		d2 = DIR_NE;
		break;
	case DIR_E:
		d1 = DIR_NE;
		d2 = DIR_SE;
		break;
	case DIR_S:
		d1 = DIR_SE;
		d2 = DIR_SW;
		break;
	case DIR_W:
		d1 = DIR_SW;
		d2 = DIR_NW;
		break;
	default:
		return false;
	}

	auto leftStep { position + d1 };
	auto rightStep { position + d2 };

	if (!nSolidTable[dPiece[leftStep.x][leftStep.y]] && !nSolidTable[dPiece[rightStep.x][rightStep.y]])
		return false;

	return !PosOkPlayer(MyPlayerId, leftStep) && !PosOkPlayer(MyPlayerId, rightStep);
}

bool CanChangeDirection(const PlayerStruct &player)
{
	if (player._pmode == PM_STAND)
		return true;
	if (player._pmode == PM_ATTACK && player.AnimInfo.CurrentFrame > player._pAFNum)
		return true;
	if (player._pmode == PM_RATTACK && player.AnimInfo.CurrentFrame > player._pAFNum)
		return true;
	if (player._pmode == PM_SPELL && player.AnimInfo.CurrentFrame > player._pSFNum)
		return true;
	return false;
}

void WalkInDir(int playerId, AxisDirection dir)
{
	auto &player = Players[playerId];

	if (dir.x == AxisDirectionX_NONE && dir.y == AxisDirectionY_NONE) {
		if (sgbControllerActive && player.walkpath[0] != WALK_NONE && player.destAction == ACTION_NONE)
			NetSendCmdLoc(playerId, true, CMD_WALKXY, player.position.future); // Stop walking
		return;
	}

	const Direction pdir = FaceDir[static_cast<std::size_t>(dir.x)][static_cast<std::size_t>(dir.y)];
	const auto delta = player.position.future + pdir;

	if (CanChangeDirection(player))
		player._pdir = pdir;

	if (PosOkPlayer(playerId, delta) && IsPathBlocked(player.position.future, pdir))
		return; // Don't start backtrack around obstacles

	NetSendCmdLoc(playerId, true, CMD_WALKXY, delta);
}

void QuestLogMove(AxisDirection moveDir)
{
	static AxisDirectionRepeater repeater;
	moveDir = repeater.Get(moveDir);
	if (moveDir.y == AxisDirectionY_UP)
		QuestlogUp();
	else if (moveDir.y == AxisDirectionY_DOWN)
		QuestlogDown();
}

void StoreMove(AxisDirection moveDir)
{
	static AxisDirectionRepeater repeater;
	moveDir = repeater.Get(moveDir);
	if (moveDir.y == AxisDirectionY_UP)
		StoreUp();
	else if (moveDir.y == AxisDirectionY_DOWN)
		StoreDown();
}

using HandleLeftStickOrDPadFn = void (*)(devilution::AxisDirection);

HandleLeftStickOrDPadFn GetLeftStickOrDPadGameUIHandler()
{
	if (invflag) {
		return &InvMove;
	}
	if (chrflag && Players[MyPlayerId]._pStatPts > 0) {
		return &AttrIncBtnSnap;
	}
	if (spselflag) {
		return &HotSpellMove;
	}
	if (sbookflag) {
		return &SpellBookMove;
	}
	if (QuestLogIsOpen) {
		return &QuestLogMove;
	}
	if (stextflag != STORE_NONE) {
		return &StoreMove;
	}
	return nullptr;
}

void ProcessLeftStickOrDPadGameUI()
{
	HandleLeftStickOrDPadFn handler = GetLeftStickOrDPadGameUIHandler();
	if (handler != nullptr)
		handler(GetLeftStickOrDpadDirection(true));
}

void Movement(int playerId)
{
	if (InGameMenu()
	    || IsControllerButtonPressed(ControllerButton_BUTTON_START)
	    || IsControllerButtonPressed(ControllerButton_BUTTON_BACK))
		return;

	AxisDirection moveDir = GetMoveDirection();
	if (moveDir.x != AxisDirectionX_NONE || moveDir.y != AxisDirectionY_NONE) {
		sgbControllerActive = true;
	}

	if (GetLeftStickOrDPadGameUIHandler() == nullptr) {
		WalkInDir(playerId, moveDir);
	}
}

struct RightStickAccumulator {

	RightStickAccumulator()
	{
		lastTc = SDL_GetTicks();
		hiresDX = 0;
		hiresDY = 0;
	}

	void Pool(int *x, int *y, int slowdown)
	{
		const Uint32 tc = SDL_GetTicks();
		const int dtc = tc - lastTc;
		hiresDX += rightStickX * dtc;
		hiresDY += rightStickY * dtc;
		const int dx = static_cast<int>(hiresDX / slowdown);
		const int dy = static_cast<int>(hiresDY / slowdown);
		*x += dx;
		*y -= dy;
		lastTc = tc;
		// keep track of remainder for sub-pixel motion
		hiresDX -= dx * slowdown;
		hiresDY -= dy * slowdown;
	}

	void Clear()
	{
		lastTc = SDL_GetTicks();
	}

	DWORD lastTc;
	float hiresDX;
	float hiresDY;
};

} // namespace

void StoreSpellCoords()
{
	const int startX = PANEL_LEFT + 12 + SPLICONLENGTH / 2;
	const int endX = startX + SPLICONLENGTH * 10;
	const int endY = PANEL_TOP - 17 - SPLICONLENGTH / 2;
	speedspellcount = 0;
	int xo = endX;
	int yo = endY;
	for (int i = RSPLTYPE_SKILL; i <= RSPLTYPE_CHARGES; i++) {
		std::uint64_t spells;
		auto &myPlayer = Players[MyPlayerId];
		switch (i) {
		case RSPLTYPE_SKILL:
			spells = myPlayer._pAblSpells;
			break;
		case RSPLTYPE_SPELL:
			spells = myPlayer._pMemSpells;
			break;
		case RSPLTYPE_SCROLL:
			spells = myPlayer._pScrlSpells;
			break;
		case RSPLTYPE_CHARGES:
			spells = myPlayer._pISpells;
			break;
		}
		std::uint64_t spell = 1;
		for (int j = 1; j < MAX_SPELLS; j++) {
			if ((spell & spells) != 0) {
				speedspellscoords[speedspellcount] = { xo, yo };
				++speedspellcount;
				xo -= SPLICONLENGTH;
				if (xo < startX) {
					xo = endX;
					yo -= SPLICONLENGTH;
				}
			}
			spell <<= 1;
		}
		if (spells != 0 && xo != endX)
			xo -= SPLICONLENGTH;
		if (xo < startX) {
			xo = endX;
			yo -= SPLICONLENGTH;
		}
	}
}

bool IsAutomapActive()
{
	return AutomapActive && leveltype != DTYPE_TOWN;
}

bool IsMovingMouseCursorWithController()
{
	return rightStickX != 0 || rightStickY != 0;
}

void HandleRightStickMotion()
{
	static RightStickAccumulator acc;
	// deadzone is handled in ScaleJoystickAxes() already
	if (rightStickX == 0 && rightStickY == 0) {
		acc.Clear();
		return;
	}

	if (IsAutomapActive()) { // move map
		int dx = 0;
		int dy = 0;
		acc.Pool(&dx, &dy, 32);
		AutomapOffset.deltaX += dy + dx;
		AutomapOffset.deltaY += dy - dx;
		return;
	}

	{ // move cursor
		sgbControllerActive = false;
		int x = MousePosition.x;
		int y = MousePosition.y;
		acc.Pool(&x, &y, 2);
		x = std::min(std::max(x, 0), gnScreenWidth - 1);
		y = std::min(std::max(y, 0), gnScreenHeight - 1);

		// We avoid calling `SetCursorPos` within the same SDL tick because
		// that can cause all stick motion events to arrive before all
		// cursor position events.
		static int lastMouseSetTick = 0;
		const int now = SDL_GetTicks();
		if (now - lastMouseSetTick > 0) {
			SetCursorPos(x, y);
			lastMouseSetTick = now;
		}
	}
}

/**
 * @brief Moves the mouse to the first inventory slot.
 */
void FocusOnInventory()
{
	Slot = SLOTXY_INV_FIRST;
	ResetInvCursorPosition();
}

void plrctrls_after_check_curs_move()
{
	// check for monsters first, then items, then towners.
	if (sgbControllerActive) {
		// Clear focuse set by cursor
		pcursplr = -1;
		pcursmonst = -1;
		pcursitem = -1;
		pcursobj = -1;
		pcursmissile = -1;
		pcurstrig = -1;
		pcursquest = -1;
		cursmx = -1;
		cursmy = -1;
		if (Players[MyPlayerId]._pInvincible) {
			return;
		}
		if (DoomFlag) {
			return;
		}
		if (!invflag) {
			*infostr = '\0';
			ClearPanel();
			FindActor();
			FindItemOrObject();
			FindTrigger();
		}
	}
}

void plrctrls_every_frame()
{
	ProcessLeftStickOrDPadGameUI();
	HandleRightStickMotion();
}

void plrctrls_after_game_logic()
{
	Movement(MyPlayerId);
}

void UseBeltItem(int type)
{
	for (int i = 0; i < MAXBELTITEMS; i++) {
		auto &myPlayer = Players[MyPlayerId];
		const int id = AllItemsList[myPlayer.SpdList[i].IDidx].iMiscId;
		const int spellId = AllItemsList[myPlayer.SpdList[i].IDidx].iSpell;
		if ((type == BLT_HEALING && (id == IMISC_HEAL || id == IMISC_FULLHEAL || (id == IMISC_SCROLL && spellId == SPL_HEAL)))
		    || (type == BLT_MANA && (id == IMISC_MANA || id == IMISC_FULLMANA))
		    || id == IMISC_REJUV || id == IMISC_FULLREJUV) {
			if (myPlayer.SpdList[i]._itype > -1) {
				UseInvItem(MyPlayerId, INVITEM_BELT_FIRST + i);
				break;
			}
		}
	}
}

void PerformPrimaryAction()
{
	if (invflag) { // inventory is open
		if (pcurs > CURSOR_HAND && pcurs < CURSOR_FIRSTITEM) {
			TryIconCurs();
			NewCursor(CURSOR_HAND);
		} else {
			CheckInvItem();
		}
		return;
	}

	if (spselflag) {
		SetSpell();
		return;
	}

	if (chrflag && !chrbtnactive && Players[MyPlayerId]._pStatPts > 0) {
		CheckChrBtns();
		for (int i = 0; i < 4; i++) {
			if (ChrBtnsRect[i].Contains(MousePosition)) {
				chrbtn[i] = true;
				chrbtnactive = true;
				ReleaseChrBtns(false);
			}
		}
		return;
	}

	Interact();
}

bool SpellHasActorTarget()
{
	int spl = Players[MyPlayerId]._pRSpell;
	if (spl == SPL_TOWN || spl == SPL_TELEPORT)
		return false;

	if (spl == SPL_FIREWALL && pcursmonst != -1) {
		cursmx = Monsters[pcursmonst].position.tile.x;
		cursmy = Monsters[pcursmonst].position.tile.y;
	}

	return pcursplr != -1 || pcursmonst != -1;
}

void UpdateSpellTarget()
{
	if (SpellHasActorTarget())
		return;

	pcursplr = -1;
	pcursmonst = -1;

	int range = 1;
	if (Players[MyPlayerId]._pRSpell == SPL_TELEPORT)
		range = 4;

	auto cursm = Players[MyPlayerId].position.future + Displacement::fromDirection(Players[MyPlayerId]._pdir) * range;
	cursmx = cursm.x;
	cursmy = cursm.y;
}

/**
 * @brief Try dropping item in all 9 possible places
 */
bool TryDropItem()
{
	const auto &myPlayer = Players[MyPlayerId];

	cursmx = myPlayer.position.future.x + 1;
	cursmy = myPlayer.position.future.y;
	if (!DropItemBeforeTrig()) {
		// Try to drop on the other side
		cursmx = myPlayer.position.future.x;
		cursmy = myPlayer.position.future.y + 1;
		DropItemBeforeTrig();
	}

	return pcurs == CURSOR_HAND;
}

void PerformSpellAction()
{
	if (InGameMenu() || QuestLogIsOpen || sbookflag)
		return;

	if (invflag) {
		if (pcurs >= CURSOR_FIRSTITEM)
			TryDropItem();
		else if (pcurs > CURSOR_HAND) {
			TryIconCurs();
			NewCursor(CURSOR_HAND);
		} else {
			CheckInvItem(true);
			ResetInvCursorPosition();
		}
		return;
	}

	if (pcurs >= CURSOR_FIRSTITEM && !TryDropItem())
		return;
	if (pcurs > CURSOR_HAND)
		NewCursor(CURSOR_HAND);

	if (spselflag) {
		SetSpell();
		return;
	}

	const auto &myPlayer = Players[MyPlayerId];
	int spl = myPlayer._pRSpell;
	if ((pcursplr == -1 && (spl == SPL_RESURRECT || spl == SPL_HEALOTHER))
	    || (pcursobj == -1 && spl == SPL_DISARM)) {
		myPlayer.Say(HeroSpeech::ICantCastThatHere);
		return;
	}

	UpdateSpellTarget();
	CheckPlrSpell();
}

void CtrlUseInvItem()
{
	ItemStruct *item;

	if (pcursinvitem == -1)
		return;

	auto &myPlayer = Players[MyPlayerId];

	if (pcursinvitem <= INVITEM_INV_LAST)
		item = &myPlayer.InvList[pcursinvitem - INVITEM_INV_FIRST];
	else
		item = &myPlayer.SpdList[pcursinvitem - INVITEM_BELT_FIRST];

	if ((item->_iMiscId == IMISC_SCROLLT || item->_iMiscId == IMISC_SCROLL) && spelldata[item->_iSpell].sTargeted) {
		return;
	}

	if (item->isEquipment()) {
		CheckInvItem(true); // auto-equip if it's an equipment
		ResetInvCursorPosition();
	} else {
		UseInvItem(MyPlayerId, pcursinvitem);
	}
}

void PerformSecondaryAction()
{
	if (invflag) {
		CtrlUseInvItem();
		return;
	}

	if (pcurs >= CURSOR_FIRSTITEM && !TryDropItem())
		return;
	if (pcurs > CURSOR_HAND)
		NewCursor(CURSOR_HAND);

	if (pcursitem != -1) {
		NetSendCmdLocParam1(true, CMD_GOTOAGETITEM, { cursmx, cursmy }, pcursitem);
	} else if (pcursobj != -1) {
		NetSendCmdLocParam1(true, CMD_OPOBJXY, { cursmx, cursmy }, pcursobj);
	} else if (pcursmissile != -1) {
		MakePlrPath(MyPlayerId, Missiles[pcursmissile].position.tile, true);
		Players[MyPlayerId].destAction = ACTION_WALK;
	} else if (pcurstrig != -1) {
		MakePlrPath(MyPlayerId, trigs[pcurstrig].position, true);
		Players[MyPlayerId].destAction = ACTION_WALK;
	} else if (pcursquest != -1) {
		MakePlrPath(MyPlayerId, Quests[pcursquest].position, true);
		Players[MyPlayerId].destAction = ACTION_WALK;
	}
}

} // namespace devilution
