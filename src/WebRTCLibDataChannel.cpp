#include "WebRTCLibDataChannel.hpp"

using namespace godot_webrtc;

void _on_channel_open(UINT64 p_user, RtcDataChannel *p_channel) {
	WARN_PRINT("open!");
}

void _on_channel_message(UINT64 p_user, RtcDataChannel *p_channel, BOOL p_is_binary, PBYTE p_buffer, UINT32 p_buffer_size) {
	WARN_PRINT("on message!");
}
#if 0
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
#endif

// DataChannel
WebRTCLibDataChannel *WebRTCLibDataChannel::new_data_channel(RtcDataChannel *p_channel) {
	// Invalid channel result in NULL return
	ERR_FAIL_COND_V(!p_channel, nullptr);

	// Instance a WebRTCDataChannelGDNative object
	godot::WebRTCDataChannelGDNative *out = godot::WebRTCDataChannelGDNative::_new();
	// Set our implementation as it's script
	godot::NativeScript *script = godot::NativeScript::_new();
	script->set_library(godot::get_wrapper<godot::GDNativeLibrary>((godot_object *)godot::gdnlib));
	script->set_class_name("WebRTCLibDataChannel");
	out->set_script(script);

	// Bind the data channel to the ScriptInstance userdata (our script)
	WebRTCLibDataChannel *tmp = godot::as<WebRTCLibDataChannel>(out);
	tmp->bind_channel(p_channel);

	return tmp;
}

void WebRTCLibDataChannel::bind_channel(RtcDataChannel *p_channel) {
	ERR_FAIL_COND(!p_channel);

	channel = p_channel;
	label = godot::String(channel->name);
	dataChannelOnOpen(p_channel, (UINT64)this, _on_channel_open);
	dataChannelOnMessage(p_channel, (UINT64)this, _on_channel_message);
#if 0
	label = p_channel->label();
	protocol = p_channel->protocol();
	channel->RegisterObserver(&observer);
#endif
}

void WebRTCLibDataChannel::queue_packet(const uint8_t *data, uint32_t size) {
	mutex->lock();

	godot::PoolByteArray packet;
	packet.resize(size);
	{
		godot::PoolByteArray::Write w = packet.write();
		memcpy(w.ptr(), data, size);
	}
	packet_queue.push(packet);

	mutex->unlock();
}

void WebRTCLibDataChannel::set_write_mode(godot_int mode) {
}

godot_int WebRTCLibDataChannel::get_write_mode() const {
	return 0;
}

bool WebRTCLibDataChannel::was_string_packet() const {
	return false;
}

WebRTCLibDataChannel::ChannelState WebRTCLibDataChannel::get_ready_state() const {
#if 0
	ERR_FAIL_COND_V(channel.get() == nullptr, STATE_CLOSED);
	return (ChannelState)channel->state();
#endif
	return (ChannelState)0;
}

const char *WebRTCLibDataChannel::get_label() const {
	ERR_FAIL_COND_V(!channel, "");
	return label.utf8().get_data();
}

bool WebRTCLibDataChannel::is_ordered() const {
#if 0
	ERR_FAIL_COND_V(channel.get() == nullptr, false);
	return channel->ordered();
#endif
	return false;
}

int WebRTCLibDataChannel::get_id() const {
#if 0
	ERR_FAIL_COND_V(channel.get() == nullptr, -1);
	return channel->id();
#endif
	return 0;
}

int WebRTCLibDataChannel::get_max_packet_life_time() const {
#if 0
	ERR_FAIL_COND_V(channel.get() == nullptr, 0);
	return channel->maxRetransmitTime();
#endif
	return 0;
}

int WebRTCLibDataChannel::get_max_retransmits() const {
#if 0
	ERR_FAIL_COND_V(channel.get() == nullptr, 0);
	return channel->maxRetransmits();
#endif
	return 0;
}

const char *WebRTCLibDataChannel::get_protocol() const {
#if 0
	ERR_FAIL_COND_V(channel.get() == nullptr, "");
	return protocol.c_str();
#endif
	return "";
}

bool WebRTCLibDataChannel::is_negotiated() const {
#if 0
	ERR_FAIL_COND_V(channel.get() == nullptr, false);
	return channel->negotiated();
#endif
	return false;
}

godot_error WebRTCLibDataChannel::poll() {
	return GODOT_OK;
}

void WebRTCLibDataChannel::close() {
#if 0
	if(channel.get() != nullptr) {
		channel->Close();
		channel->UnregisterObserver();
	}
#endif
}

godot_error WebRTCLibDataChannel::get_packet(const uint8_t **r_buffer, int *r_len) {
	ERR_FAIL_COND_V(packet_queue.empty(), GODOT_ERR_UNAVAILABLE);

	mutex->lock();

	// Update current packet and pop queue
	current_packet = packet_queue.front();
	packet_queue.pop();
	// Set out buffer and size (buffer will be gone at next get_packet or close)
	*r_buffer = current_packet.read().ptr();
	*r_len = current_packet.size();

	mutex->unlock();

	return GODOT_OK;
}

godot_error WebRTCLibDataChannel::put_packet(const uint8_t *p_buffer, int p_len) {
#if 0
	ERR_FAIL_COND_V(channel.get() == nullptr, GODOT_ERR_UNAVAILABLE);

	webrtc::DataBuffer webrtc_buffer(rtc::CopyOnWriteBuffer(p_buffer, p_len), true);
	ERR_FAIL_COND_V(!channel->Send(webrtc_buffer), GODOT_FAILED);
#endif
	return GODOT_OK;
}

godot_int WebRTCLibDataChannel::get_available_packet_count() const {
	return packet_queue.size();
}

godot_int WebRTCLibDataChannel::get_max_packet_size() const {
	return 1200;
}

void WebRTCLibDataChannel::_register_methods() {
}

void WebRTCLibDataChannel::_init() {
	register_interface(&interface);
}

WebRTCLibDataChannel::WebRTCLibDataChannel() {
	mutex = new std::mutex;
}

WebRTCLibDataChannel::~WebRTCLibDataChannel() {
	close();
	if (_owner) {
		register_interface(NULL);
	}
	delete mutex;
}
