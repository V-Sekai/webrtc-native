/*************************************************************************/
/*  WebRTCLibDataChannel.hpp                                             */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2021 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2021 Godot Engine contributors (cf. AUTHORS.md).   */
/*                                                                       */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                       */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/

#ifndef WEBRTC_DATA_CHANNEL_H
#define WEBRTC_DATA_CHANNEL_H

#ifdef GDNATIVE_WEBRTC
#include "net/WebRTCDataChannelNative.hpp"
#include <Godot.hpp> // Godot.hpp must go first, or windows builds breaks
#undef GDCLASS
#define GDCLASS(arg1, arg2) GODOT_CLASS(WebRTCLibDataChannel, WebRTCDataChannelNative);
namespace godot {
using WebRTCDataChannelExtension = WebRTCDataChannelNative;
};
#else
#include <godot_cpp/classes/web_rtc_data_channel_extension.hpp>
#endif

#include "api/peer_connection_interface.h" // interface for all things needed from WebRTC
#include "media/base/media_engine.h" // needed for CreateModularPeerConnectionFactory

#include <mutex>

namespace godot_webrtc {

class WebRTCLibDataChannel : public godot::WebRTCDataChannelExtension {
	GDCLASS(WebRTCLibDataChannel, WebRTCDataChannelExtension);

private:
	class ChannelObserver : public webrtc::DataChannelObserver {
	public:
		WebRTCLibDataChannel *parent;

		ChannelObserver(WebRTCLibDataChannel *parent);
		void OnMessage(const webrtc::DataBuffer &buffer) override;
		void OnStateChange() override; // UNUSED
		void OnBufferedAmountChange(uint64_t previous_amount) override; // UNUSED
	};

	ChannelObserver observer;
	rtc::scoped_refptr<webrtc::DataChannelInterface> channel;

	std::mutex *mutex;
	std::queue<std::vector<uint8_t>> packet_queue;
	std::vector<uint8_t> current_packet;
	godot::String label;
	godot::String protocol;

protected:
	static void _bind_methods() {}

public:
	static WebRTCLibDataChannel *new_data_channel(rtc::scoped_refptr<webrtc::DataChannelInterface> p_channel);
	static void _register_methods();

	void bind_channel(rtc::scoped_refptr<webrtc::DataChannelInterface> p_channel);
	void queue_packet(const uint8_t *data, uint32_t size);

	/* PacketPeer */
	virtual int64_t _get_packet(const void *r_buffer, int32_t *r_len) override;
	virtual int64_t _put_packet(const void *p_buffer, int64_t p_len) override;
	virtual int64_t _get_available_packet_count() const override;
	virtual int64_t _get_max_packet_size() const override;

	/* WebRTCDataChannel */
	int64_t _poll() override;
	void _close() override;

	void _set_write_mode(int64_t mode) override;
	int64_t _get_write_mode() const override;
	bool _was_string_packet() const override;

	int64_t _get_ready_state() const override;
	godot::String _get_label() const override;
	bool _is_ordered() const override;
	int64_t _get_id() const override;
	int64_t _get_max_packet_life_time() const override;
	int64_t _get_max_retransmits() const override;
	godot::String _get_protocol() const override;
	bool _is_negotiated() const override;
	int64_t _get_buffered_amount() const override;

	WebRTCLibDataChannel();
	~WebRTCLibDataChannel();
};

} // namespace godot_webrtc

#endif // WEBRTC_DATA_CHANNEL_H
