/*************************************************************************/
/*  WebRTCLibDataChannel.cpp                                             */
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

#include "WebRTCLibDataChannel.hpp"

using namespace godot;
using namespace godot_webrtc;

// Channel observer
WebRTCLibDataChannel::ChannelObserver::ChannelObserver(WebRTCLibDataChannel *parent) {
	this->parent = parent;
}

void WebRTCLibDataChannel::ChannelObserver::OnMessage(const webrtc::DataBuffer &buffer) {
	parent->queue_packet(buffer.data.data<uint8_t>(), buffer.data.size());
}

void WebRTCLibDataChannel::ChannelObserver::OnStateChange() {
}

void WebRTCLibDataChannel::ChannelObserver::OnBufferedAmountChange(uint64_t previous_amount) {
}

// DataChannel
WebRTCLibDataChannel *WebRTCLibDataChannel::new_data_channel(rtc::scoped_refptr<webrtc::DataChannelInterface> p_channel) {
	// Invalid channel result in NULL return
	ERR_FAIL_COND_V(p_channel.get() == nullptr, nullptr);

	WebRTCLibDataChannel *out = memnew(WebRTCLibDataChannel());
	out->bind_channel(p_channel);
	return out;
}

void WebRTCLibDataChannel::bind_channel(rtc::scoped_refptr<webrtc::DataChannelInterface> p_channel) {
	ERR_FAIL_COND(p_channel.get() == nullptr);

	channel = p_channel;
	label = p_channel->label();
	protocol = p_channel->protocol();
	channel->RegisterObserver(&observer);
}

void WebRTCLibDataChannel::queue_packet(const uint8_t *data, uint32_t size) {
	mutex->lock();

	std::vector<uint8_t> packet;
	packet.resize(size);
	memcpy(&packet[0], data, size);
	packet_queue.push(packet);

	mutex->unlock();
}

void WebRTCLibDataChannel::_set_write_mode(int64_t mode) {
}

int64_t WebRTCLibDataChannel::_get_write_mode() const {
	return 0;
}

bool WebRTCLibDataChannel::_was_string_packet() const {
	return false;
}

int64_t WebRTCLibDataChannel::_get_ready_state() const {
	ERR_FAIL_COND_V(channel.get() == nullptr, STATE_CLOSED);
	return channel->state();
}

godot::String WebRTCLibDataChannel::_get_label() const {
	ERR_FAIL_COND_V(channel.get() == nullptr, "");
	return label.c_str();
}

bool WebRTCLibDataChannel::_is_ordered() const {
	ERR_FAIL_COND_V(channel.get() == nullptr, false);
	return channel->ordered();
}

int64_t WebRTCLibDataChannel::_get_id() const {
	ERR_FAIL_COND_V(channel.get() == nullptr, -1);
	return channel->id();
}

int64_t WebRTCLibDataChannel::_get_max_packet_life_time() const {
	ERR_FAIL_COND_V(channel.get() == nullptr, 0);
	return channel->maxRetransmitTime();
}

int64_t WebRTCLibDataChannel::_get_max_retransmits() const {
	ERR_FAIL_COND_V(channel.get() == nullptr, 0);
	return channel->maxRetransmits();
}

godot::String WebRTCLibDataChannel::_get_protocol() const {
	ERR_FAIL_COND_V(channel.get() == nullptr, "");
	return protocol.c_str();
}

bool WebRTCLibDataChannel::_is_negotiated() const {
	ERR_FAIL_COND_V(channel.get() == nullptr, false);
	return channel->negotiated();
}

int64_t WebRTCLibDataChannel::_get_buffered_amount() const {
	ERR_FAIL_COND_V(channel.get() == nullptr, 0);
	return channel->buffered_amount();
}

int64_t WebRTCLibDataChannel::_poll() {
	return 0;
}

void WebRTCLibDataChannel::_close() {
	if (channel.get() != nullptr) {
		channel->Close();
		channel->UnregisterObserver();
	}
}

int64_t WebRTCLibDataChannel::_get_packet(const uint8_t **r_buffer, int32_t *r_len) {
	ERR_FAIL_COND_V(packet_queue.empty(), 1);

	mutex->lock();

	// Update current packet and pop queue
	current_packet = packet_queue.front();
	packet_queue.pop();
	// Set out buffer and size (buffer will be gone at next get_packet or close)
	*r_buffer = &current_packet[0];
	*r_len = current_packet.size();

	mutex->unlock();

	return 0;
}

int64_t WebRTCLibDataChannel::_put_packet(const uint8_t *p_buffer, int p_len) {
	ERR_FAIL_COND_V(channel.get() == nullptr, 1);

	webrtc::DataBuffer webrtc_buffer(rtc::CopyOnWriteBuffer(p_buffer, p_len), true);
	ERR_FAIL_COND_V(!channel->Send(webrtc_buffer), 1);

	return 0;
}

int64_t WebRTCLibDataChannel::_get_available_packet_count() const {
	return packet_queue.size();
}

int64_t WebRTCLibDataChannel::_get_max_packet_size() const {
	return 1200;
}

void WebRTCLibDataChannel::_register_methods() {
}

WebRTCLibDataChannel::WebRTCLibDataChannel() :
		observer(this) {
	mutex = new std::mutex;
}

WebRTCLibDataChannel::~WebRTCLibDataChannel() {
	close();
	delete mutex;
}
