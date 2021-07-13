#include "dvlnet/base.h"

#include <algorithm>
#include <cstring>
#include <memory>

namespace devilution {
namespace net {

void base::setup_gameinfo(buffer_t info)
{
	game_init_info = std::move(info);
}

void base::setup_password(std::string pw)
{
	pktfty = std::make_unique<packet_factory>(pw);
}

void base::RunEventHandler(_SNETEVENT &ev)
{
	auto f = registered_handlers[static_cast<event_type>(ev.eventid)];
	if (f != nullptr) {
		f(&ev);
	}
}

void base::DisconnectNet(plr_t plr)
{
}

void base::HandleAccept(packet &pkt)
{
	if (plr_self != PLR_BROADCAST) {
		return; // already have player id
	}
	if (pkt.Cookie() == cookie_self) {
		plr_self = pkt.NewPlayer();
		connected_table[plr_self] = true;
	}
	if (game_init_info != pkt.Info()) {
		if (pkt.Info().size() != sizeof(GameData)) {
			ABORT();
		}
		// we joined and did not create
		game_init_info = pkt.Info();
		_SNETEVENT ev;
		ev.eventid = EVENT_TYPE_PLAYER_CREATE_GAME;
		ev.playerid = plr_self;
		ev.data = const_cast<unsigned char *>(pkt.Info().data());
		ev.databytes = pkt.Info().size();
		RunEventHandler(ev);
	}
}

void base::ClearMsg(plr_t plr)
{
	message_queue.erase(std::remove_if(message_queue.begin(),
	                        message_queue.end(),
	                        [&](message_t &msg) {
		                        return msg.sender == plr;
	                        }),
	    message_queue.end());
}

void base::RecvLocal(packet &pkt)
{
	if (pkt.Source() < MAX_PLRS) {
		connected_table[pkt.Source()] = true;
	}
	switch (pkt.Type()) {
	case PT_MESSAGE:
		message_queue.emplace_back(pkt.Source(), pkt.Message());
		break;
	case PT_TURN:
		turn_queue[pkt.Source()].push_back(pkt.Turn());
		break;
	case PT_JOIN_ACCEPT:
		HandleAccept(pkt);
		break;
	case PT_CONNECT:
		connected_table[pkt.NewPlayer()] = true; // this can probably be removed
		break;
	case PT_DISCONNECT:
		if (pkt.NewPlayer() != plr_self) {
			if (connected_table[pkt.NewPlayer()]) {
				auto leaveinfo = pkt.LeaveInfo();
				_SNETEVENT ev;
				ev.eventid = EVENT_TYPE_PLAYER_LEAVE_GAME;
				ev.playerid = pkt.NewPlayer();
				ev.data = reinterpret_cast<unsigned char *>(&leaveinfo);
				ev.databytes = sizeof(leaveinfo_t);
				RunEventHandler(ev);
				connected_table[pkt.NewPlayer()] = false;
				DisconnectNet(pkt.NewPlayer());
				ClearMsg(pkt.NewPlayer());
				turn_queue[pkt.NewPlayer()].clear();
			}
		} else {
			ABORT(); // we were dropped by the owner?!?
		}
		break;
	default:
		break;
		// otherwise drop
	}
}

bool base::SNetReceiveMessage(int *sender, void **data, uint32_t *size)
{
	poll();
	if (message_queue.empty())
		return false;
	message_last = message_queue.front();
	message_queue.pop_front();
	*sender = message_last.sender;
	*size = message_last.payload.size();
	*data = message_last.payload.data();
	return true;
}

bool base::SNetSendMessage(int playerId, void *data, unsigned int size)
{
	if (playerId != SNPLAYER_ALL && playerId != SNPLAYER_OTHERS
	    && (playerId < 0 || playerId >= MAX_PLRS))
		abort();
	auto *rawMessage = reinterpret_cast<unsigned char *>(data);
	buffer_t message(rawMessage, rawMessage + size);
	if (playerId == plr_self || playerId == SNPLAYER_ALL)
		message_queue.emplace_back(plr_self, message);
	plr_t dest;
	if (playerId == SNPLAYER_ALL || playerId == SNPLAYER_OTHERS)
		dest = PLR_BROADCAST;
	else
		dest = playerId;
	if (dest != plr_self) {
		auto pkt = pktfty->make_packet<PT_MESSAGE>(plr_self, dest, message);
		send(*pkt);
	}
	return true;
}

bool base::SNetReceiveTurns(char **data, unsigned int *size, DWORD *status)
{
	poll();
	bool allTurnsArrived = true;
	for (auto i = 0; i < MAX_PLRS; ++i) {
		status[i] = 0;
		if (connected_table[i]) {
			status[i] |= PS_CONNECTED;
			if (turn_queue[i].empty())
				allTurnsArrived = false;
		}
	}
	if (allTurnsArrived) {
		for (auto i = 0; i < MAX_PLRS; ++i) {
			if (connected_table[i]) {
				size[i] = sizeof(turn_t);
				status[i] |= PS_ACTIVE;
				status[i] |= PS_TURN_ARRIVED;
				turn_last[i] = turn_queue[i].front();
				turn_queue[i].pop_front();
				data[i] = reinterpret_cast<char *>(&turn_last[i]);
			}
		}
		return true;
	}
	for (auto i = 0; i < MAX_PLRS; ++i) {
		if (connected_table[i]) {
			if (!turn_queue[i].empty()) {
				status[i] |= PS_ACTIVE;
			}
		}
	}
	return false;
}

bool base::SNetSendTurn(char *data, unsigned int size)
{
	if (size != sizeof(turn_t))
		ABORT();
	turn_t turn;
	std::memcpy(&turn, data, sizeof(turn));
	auto pkt = pktfty->make_packet<PT_TURN>(plr_self, PLR_BROADCAST, turn);
	send(*pkt);
	turn_queue[plr_self].push_back(pkt->Turn());
	return true;
}

void base::SNetGetProviderCaps(struct _SNETCAPS *caps)
{
	caps->size = 0;                  // engine writes only ?!?
	caps->flags = 0;                 // unused
	caps->maxmessagesize = 512;      // capped to 512; underflow if < 24
	caps->maxqueuesize = 0;          // unused
	caps->maxplayers = MAX_PLRS;     // capped to 4
	caps->bytessec = 1000000;        // ?
	caps->latencyms = 0;             // unused
	caps->defaultturnssec = 10;      // ?
	caps->defaultturnsintransit = 1; // maximum acceptable number
	                                 // of turns in queue?
}

bool base::SNetUnregisterEventHandler(event_type evtype)
{
	registered_handlers.erase(evtype);
	return true;
}

bool base::SNetRegisterEventHandler(event_type evtype, SEVTHANDLER func)
{
	/*
  engine registers handler for:
  EVENT_TYPE_PLAYER_LEAVE_GAME
  EVENT_TYPE_PLAYER_CREATE_GAME (should be raised during SNetCreateGame
  for non-creating player)
  EVENT_TYPE_PLAYER_MESSAGE (for bnet? not implemented)
  (engine uses same function for all three)
*/
	registered_handlers[evtype] = func;
	return true;
}

bool base::SNetLeaveGame(int type)
{
	auto pkt = pktfty->make_packet<PT_DISCONNECT>(plr_self, PLR_BROADCAST,
	    plr_self, type);
	send(*pkt);
	return true;
}

bool base::SNetDropPlayer(int playerid, DWORD flags)
{
	auto pkt = pktfty->make_packet<PT_DISCONNECT>(plr_self,
	    PLR_BROADCAST,
	    (plr_t)playerid,
	    (leaveinfo_t)flags);
	send(*pkt);
	RecvLocal(*pkt);
	return true;
}

plr_t base::GetOwner()
{
	for (auto i = 0; i < MAX_PLRS; ++i) {
		if (connected_table[i]) {
			return i;
		}
	}
	return PLR_BROADCAST; // should be unreachable
}

bool base::SNetGetOwnerTurnsWaiting(DWORD *turns)
{
	*turns = turn_queue[GetOwner()].size();
	return true;
}

bool base::SNetGetTurnsInTransit(DWORD *turns)
{
	*turns = turn_queue[plr_self].size();
	return true;
}

} // namespace net
} // namespace devilution
