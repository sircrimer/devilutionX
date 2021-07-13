#include "dvlnet/frame_queue.h"

#include <cstring>

#include "dvlnet/packet.h"

namespace devilution {
namespace net {

framesize_t frame_queue::Size() const
{
	return current_size;
}

buffer_t frame_queue::Read(framesize_t s)
{
	if (current_size < s)
		throw frame_queue_exception();
	buffer_t ret;
	while (s > 0 && s >= buffer_deque.front().size()) {
		s -= buffer_deque.front().size();
		current_size -= buffer_deque.front().size();
		ret.insert(ret.end(),
		    buffer_deque.front().begin(),
		    buffer_deque.front().end());
		buffer_deque.pop_front();
	}
	if (s > 0) {
		ret.insert(ret.end(),
		    buffer_deque.front().begin(),
		    buffer_deque.front().begin() + s);
		buffer_deque.front().erase(buffer_deque.front().begin(),
		    buffer_deque.front().begin() + s);
		current_size -= s;
	}
	return ret;
}

void frame_queue::Write(buffer_t buf)
{
	current_size += buf.size();
	buffer_deque.push_back(std::move(buf));
}

bool frame_queue::PacketReady()
{
	if (nextsize == 0) {
		if (Size() < sizeof(framesize_t))
			return false;
		auto szbuf = Read(sizeof(framesize_t));
		std::memcpy(&nextsize, &szbuf[0], sizeof(framesize_t));
		if (nextsize == 0)
			throw frame_queue_exception();
	}
	return Size() >= nextsize;
}

buffer_t frame_queue::ReadPacket()
{
	if (nextsize == 0 || Size() < nextsize)
		throw frame_queue_exception();
	auto ret = Read(nextsize);
	nextsize = 0;
	return ret;
}

buffer_t frame_queue::MakeFrame(buffer_t packetbuf)
{
	buffer_t ret;
	if (packetbuf.size() > max_frame_size)
		ABORT();
	framesize_t size = packetbuf.size();
	ret.insert(ret.end(), packet_out::begin(size), packet_out::end(size));
	ret.insert(ret.end(), packetbuf.begin(), packetbuf.end());
	return ret;
}

} // namespace net
} // namespace devilution
