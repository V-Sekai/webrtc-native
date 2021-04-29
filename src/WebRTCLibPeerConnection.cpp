#include "WebRTCDataChannel.hpp"
#include "WebRTCDataChannelGDNative.hpp"
#include "WebRTCLibPeerConnection.hpp"
#include "WebRTCLibDataChannel.hpp"

#include "JSON.hpp"
#include "JSONParseResult.hpp"

using namespace godot_webrtc;

godot_error _parse_ice_server(RtcConfiguration *r_config, godot::Dictionary p_server, int *r_pos) {
	ERR_FAIL_COND_V(!p_server.has("urls"), GODOT_ERR_INVALID_PARAMETER);

	godot::Variant v;
	godot::String url;
	int url_size = 0;
	int cur = *r_pos;

	// Parse mandatory URL
	v = p_server["urls"];
	if (v.get_type() == godot::Variant::STRING) {
		url = v;
		url_size = url.utf8().length();
		ERR_FAIL_COND_V(url_size > MAX_ICE_CONFIG_URI_LEN, GODOT_ERR_INVALID_PARAMETER);
		memcpy(r_config->iceServers[cur].urls, url.utf8().get_data(), url_size);
		cur++;
	} else if (v.get_type() == godot::Variant::ARRAY) {
		godot::Array names = v;
		ERR_FAIL_COND_V(cur + names.size() >= MAX_ICE_SERVERS_COUNT, GODOT_ERR_INVALID_PARAMETER);
		for (int j = 0; j < names.size(); j++) {
			v = names[j];
			ERR_FAIL_COND_V(v.get_type() != godot::Variant::STRING, GODOT_ERR_INVALID_PARAMETER);
			url = v;
			url_size = url.utf8().length();
			ERR_FAIL_COND_V(url_size > MAX_ICE_CONFIG_URI_LEN, GODOT_ERR_INVALID_PARAMETER);
			memcpy(r_config->iceServers[cur].urls, url.utf8().get_data(), url_size);
			cur++;
		}
	} else {
		ERR_FAIL_V(GODOT_ERR_INVALID_PARAMETER);
	}
	*r_pos = cur;

#if 0
	// Parse credentials (only meaningful for TURN, only support password)
	if (p_server.has("username") && (v = p_server["username"]) && v.get_type() == godot::Variant::STRING) {
		ice_server.username = (v.operator godot::String()).utf8().get_data();
	}
	if (p_server.has("credential") && (v = p_server["credential"]) && v.get_type() == godot::Variant::STRING) {
		ice_server.password = (v.operator godot::String()).utf8().get_data();
	}

	r_config.servers.push_back(ice_server);
#endif
	return GODOT_OK;
}

godot_error _parse_channel_config(RtcDataChannelInit *r_config, godot::Dictionary p_dict) {
	godot::Variant v;
#define _SET_N(PROP, PNAME, TYPE) if (p_dict.has(#PROP)) { v = p_dict[#PROP]; if(v.get_type() == godot::Variant::TYPE) r_config->PNAME = v; }
#define _SET(PROP, TYPE) _SET_N(PROP, PROP, TYPE)
	// FIXME not supported!
	_SET(negotiated, BOOL);
	//_SET(id, INT);
	_SET(maxPacketLifeTime, INT);
	_SET(maxRetransmits, INT);
	_SET(ordered, BOOL);
#undef _SET
	if (p_dict.has("protocol") && (v = p_dict["protocol"]) && v.get_type() == godot::Variant::STRING) {
		// TODO
		//r_config.protocol = v.operator godot::String().utf8().get_data();
	}

	// ID makes sense only when negotiated is true (and must be set in that case)
	// FIXME not supported.
	//ERR_FAIL_COND_V(r_config->negotiated ? r_config.id == -1 : r_config.id != -1, GODOT_ERR_INVALID_PARAMETER);
	// Only one of maxRetransmits and maxRetransmitTime can be set on a channel.
	//ERR_FAIL_COND_V(r_config->maxRetransmits != 0xFFFF && r_config->maxPacketLifeTime != 0xFFFF, GODOT_ERR_INVALID_PARAMETER);
	return GODOT_OK;
}

void _on_ice_candidate(UINT64 p_user, PCHAR p_candidate) {
	if (!p_candidate) {
		return;
	}
	RtcIceCandidateInit session;
	memset(&session, 0, sizeof(session));
	WARN_PRINT(godot::String(p_candidate));
	deserializeRtcIceCandidateInit(p_candidate, strlen(p_candidate), &session);
	godot::JSON *json = godot::JSON::get_singleton();
	godot::Ref<godot::JSONParseResult> parsed = json->parse(godot::String(p_candidate));
	godot::Variant result = parsed->get_result();
	ERR_FAIL_COND(result.get_type() != godot::Variant::DICTIONARY);
	godot::Dictionary dict = result;
	ERR_FAIL_COND(!dict.has("candidate"));
	ERR_FAIL_COND(!dict.has("sdpMLineIndex"));
	ERR_FAIL_COND(!dict.has("sdpMid"));

	godot::String sdp_candidate = dict["candidate"];
	int sdp_mline = dict["sdpMLineIndex"];
	godot::String sdp_mid = dict["sdpMid"];
	((WebRTCLibPeerConnection *)p_user)->queue_signal("ice_candidate_created", 3, sdp_mid, sdp_mline, sdp_candidate);
}

void _on_data_channel(UINT64 p_user, RtcDataChannel *p_channel) {
	WARN_PRINT("data channel received");
	((WebRTCLibPeerConnection *)p_user)->queue_signal("data_channel_received", 1, WebRTCLibDataChannel::new_data_channel(p_channel));
}

void _on_connection_state_change(UINT64 p_user, RTC_PEER_CONNECTION_STATE p_state) {
	WARN_PRINT("State: " + godot::String::num(p_state));
}

void WebRTCLibPeerConnection::queue_candidate(godot::String p_mid_name, int p_mline, godot::String p_candidate) {
	godot::Array data;
	data.push_back(p_mid_name);
	data.push_back(p_mline);
	data.push_back(p_candidate);
	candidates.push_back(data);
}

void WebRTCLibPeerConnection::emit_candidates() {
	while (candidates.size()) {
		godot::Array sdp = candidates.pop_front();
		queue_signal("ice_candidate_created", 3, sdp[0], sdp[1], sdp[2]);
	}
}

WebRTCLibPeerConnection::ConnectionState WebRTCLibPeerConnection::get_connection_state() const {
#if 0
	ERR_FAIL_COND_V(peer_connection.get() == nullptr, STATE_CLOSED);

	webrtc::PeerConnectionInterface::IceConnectionState state = peer_connection->ice_connection_state();
	switch(state) {
		case webrtc::PeerConnectionInterface::kIceConnectionNew:
			return STATE_NEW;
		case webrtc::PeerConnectionInterface::kIceConnectionChecking:
			return STATE_CONNECTING;
		case webrtc::PeerConnectionInterface::kIceConnectionConnected:
			return STATE_CONNECTED;
		case webrtc::PeerConnectionInterface::kIceConnectionCompleted:
			return STATE_CONNECTED;
		case webrtc::PeerConnectionInterface::kIceConnectionFailed:
			return STATE_FAILED;
		case webrtc::PeerConnectionInterface::kIceConnectionDisconnected:
			return STATE_DISCONNECTED;
		case webrtc::PeerConnectionInterface::kIceConnectionClosed:
			return STATE_CLOSED;
		default:
			return STATE_CLOSED;
	}
#endif
	return STATE_CLOSED;
}

godot_error WebRTCLibPeerConnection::initialize(const godot_dictionary *p_config) {
	RtcConfiguration config;
	memset(&config, 0, sizeof(config));
	godot::Dictionary d = *(godot::Dictionary *)p_config;
	godot::Variant v;
	if (d.has("iceServers") && (v = d["iceServers"]) && v.get_type() == godot::Variant::ARRAY) {
		godot::Array servers = v;
		int urls = 0;
		for (int i = 0; i < servers.size(); i++) {
			v = servers[i];
			ERR_FAIL_COND_V(v.get_type() != godot::Variant::DICTIONARY, GODOT_ERR_INVALID_PARAMETER);
			godot_error err;
			godot::Dictionary server = v;
			err = _parse_ice_server(&config, server, &urls);
			ERR_FAIL_COND_V(err != GODOT_OK, err);
		}
	}
	return _create_pc(&config);
}

godot_object *WebRTCLibPeerConnection::create_data_channel(const char *p_channel, const godot_dictionary *p_channel_config) {
	ERR_FAIL_COND_V(!peer_connection, nullptr);

	// Read config from dictionary
	RtcDataChannelInit config;
	memset(&config, 0, sizeof(config));
	godot::Dictionary d = *(godot::Dictionary *)p_channel_config;
	godot_error err = _parse_channel_config(&config, d);
	ERR_FAIL_COND_V(err != GODOT_OK, nullptr);

	RtcDataChannel *channel = nullptr;
	STATUS status = createDataChannel(peer_connection, (char *)p_channel, &config, &channel);
	ERR_FAIL_COND_V(status != STATUS_SUCCESS, nullptr);
	WebRTCLibDataChannel *wrapper = WebRTCLibDataChannel::new_data_channel(channel);
	ERR_FAIL_COND_V(!wrapper, nullptr);
	return wrapper->_owner;
}

godot_error WebRTCLibPeerConnection::create_offer() {
	ERR_FAIL_COND_V(!peer_connection, GODOT_ERR_UNCONFIGURED);
	RtcSessionDescriptionInit session;
	memset(&session, 0, sizeof(session));
	STATUS err = createOffer(peer_connection, &session);
	if (err != STATUS_SUCCESS) {
		ERR_PRINT("createOffer failed with error " + godot::String::num(err));
		ERR_FAIL_V(GODOT_FAILED);
	}
	queue_signal("session_description_created", 2, "offer", godot::String(session.sdp));
	return GODOT_OK;
}

#define _MAKE_DESC(TYPE, SDP) ERR_FAIL_COND_V(strlen(sdp) > MAX_SESSION_DESCRIPTION_INIT_SDP_LEN, GODOT_ERR_INVALID_PARAMETER); RtcSessionDescriptionInit session; memset(&session, 0, sizeof(session)); session.type = godot::String(TYPE) == "offer" ? SDP_TYPE_OFFER : SDP_TYPE_ANSWER; memcpy(session.sdp, SDP, strlen(sdp));
godot_error WebRTCLibPeerConnection::set_remote_description(const char *type, const char *sdp) {
	ERR_FAIL_COND_V(!peer_connection, GODOT_ERR_UNCONFIGURED);
	_MAKE_DESC(type, sdp);
	STATUS err = setRemoteDescription(peer_connection, &session);
	if (err != STATUS_SUCCESS) {
		ERR_PRINT("setRemoteDescription failed with error " + godot::String::num(err));
		ERR_FAIL_V(GODOT_FAILED);
	}
	RtcSessionDescriptionInit answer;
	memset(&answer, 0, sizeof(answer));
	if (session.type != SDP_TYPE_OFFER) {
		return GODOT_OK;
	}
	err = createAnswer(peer_connection, &answer);
	if (err != STATUS_SUCCESS) {
		ERR_PRINT("createAnser failed with error " + godot::String::num(err));
		ERR_FAIL_V(GODOT_FAILED);
	}
	queue_signal("session_description_created", 2, "answer", godot::String(session.sdp));
	return GODOT_OK;
}

godot_error WebRTCLibPeerConnection::set_local_description(const char *type, const char *sdp) {
	ERR_FAIL_COND_V(!peer_connection, GODOT_ERR_UNCONFIGURED);
	_MAKE_DESC(type, sdp);
	STATUS err = setLocalDescription(peer_connection, &session);
	if (err != STATUS_SUCCESS) {
		ERR_PRINT("setLocalDescription failed with error " + godot::String::num(err));
		ERR_FAIL_V(GODOT_FAILED);
	}
	return GODOT_OK;
}
#undef _MAKE_DESC

godot_error WebRTCLibPeerConnection::add_ice_candidate(const char *sdpMidName, int sdpMlineIndexName, const char *sdpName) {
	ERR_FAIL_COND_V(!peer_connection, GODOT_ERR_UNCONFIGURED);
	WARN_PRINT(godot::String::num((uint64_t)this));

	godot::Dictionary dict;
	dict["candidate"] = godot::String(sdpName);
	dict["sdpMid"] = godot::String(sdpMidName);
	dict["sdpMLineIndex"] = sdpMlineIndexName;
	godot::String config = "{\"candidate\":\"" + godot::String(sdpName) + "\",\"sdpMid\":\"" + godot::String::num(sdpMlineIndexName) + "\",\"sdpMLineIndex\":" + sdpMidName + "}";
	WARN_PRINT(config);
	RtcIceCandidateInit session;
	STATUS err = deserializeRtcIceCandidateInit((char *)config.utf8().get_data(), config.utf8().length(), &session);
	ERR_FAIL_COND_V(err != STATUS_SUCCESS, GODOT_ERR_INVALID_PARAMETER);

	err = addIceCandidate(peer_connection, session.candidate);
	ERR_FAIL_COND_V(err != STATUS_SUCCESS, GODOT_FAILED);
	return GODOT_OK;
}

godot_error WebRTCLibPeerConnection::poll() {
	ERR_FAIL_COND_V(!peer_connection, GODOT_ERR_UNCONFIGURED);

	std::function<void()> signal;
	while (!signal_queue.empty()) {
		mutex_signal_queue->lock();
		signal = signal_queue.front();
		signal_queue.pop();
		mutex_signal_queue->unlock();

		signal();
	}
	return GODOT_OK;
}

void WebRTCLibPeerConnection::close() {
	if (peer_connection != nullptr) {
		closePeerConnection(peer_connection);
		freePeerConnection(&peer_connection);
		peer_connection = nullptr;
	}
	while(!signal_queue.empty()) {
		signal_queue.pop();
	}
}

void WebRTCLibPeerConnection::_register_methods() {
}

void WebRTCLibPeerConnection::_init() {
	WARN_PRINT(godot::String::num((uint64_t)this));
	register_interface(&interface);

	// initialize variables:
	mutex_signal_queue = new std::mutex;
	RtcConfiguration config;
	memset(&config, 0, sizeof(config));
	_create_pc(&config);
}

godot_error WebRTCLibPeerConnection::_create_pc(RtcConfiguration *config) {
	STATUS err = createPeerConnection(config, &peer_connection);
	if (err != STATUS_SUCCESS) {
		WARN_PRINT("Error creating PeerConnection: " + godot::String::num(err));
		return GODOT_FAILED;
	}
	err = peerConnectionOnIceCandidate(peer_connection, (UINT64)this, _on_ice_candidate);
	ERR_FAIL_COND_V(err, GODOT_FAILED);
	err = peerConnectionOnDataChannel(peer_connection, (UINT64)this, _on_data_channel);
	ERR_FAIL_COND_V(err, GODOT_FAILED);
	err = peerConnectionOnConnectionStateChange(peer_connection, (UINT64)this, _on_connection_state_change);
	ERR_FAIL_COND_V(err, GODOT_FAILED);
	return GODOT_OK;
}

WebRTCLibPeerConnection::WebRTCLibPeerConnection() {
}

WebRTCLibPeerConnection::~WebRTCLibPeerConnection() {
	if (_owner) {
		register_interface(NULL);
	}
	close();
	delete mutex_signal_queue;
}

void WebRTCLibPeerConnection::queue_signal(godot::String p_name, int p_argc, const godot::Variant &p_arg1, const godot::Variant &p_arg2, const godot::Variant &p_arg3) {
	mutex_signal_queue->lock();
	signal_queue.push(
			[this, p_name, p_argc, p_arg1, p_arg2, p_arg3] {
				if (p_argc == 1) {
					emit_signal(p_name, p_arg1);
				} else if (p_argc == 2) {
					emit_signal(p_name, p_arg1, p_arg2);
				} else {
					emit_signal(p_name, p_arg1, p_arg2, p_arg3);
				}
			});
	mutex_signal_queue->unlock();
}
