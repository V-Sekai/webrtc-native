/*************************************************************************/
/*  WebRTCLibPeerConnection.cpp                                          */
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

#include "WebRTCLibPeerConnection.hpp"
#include "WebRTCLibDataChannel.hpp"

using namespace godot;
using namespace godot_webrtc;

std::unique_ptr<rtc::Thread> WebRTCLibPeerConnection::signaling_thread = nullptr;

// PeerConnectionObserver
void WebRTCLibPeerConnection::GodotPCO::OnIceCandidate(const webrtc::IceCandidateInterface *candidate) {
	Dictionary candidateSDP;
	String candidateSdpMidName = candidate->sdp_mid().c_str();
	int candidateSdpMlineIndexName = candidate->sdp_mline_index();
	std::string sdp;
	candidate->ToString(&sdp);
	String candidateSdpName = sdp.c_str();
	parent->queue_signal("ice_candidate_created", 3, candidateSdpMidName, candidateSdpMlineIndexName, candidateSdpName);
}

// SetSessionDescriptionObserver
void WebRTCLibPeerConnection::GodotSSDO::OnSuccess() {
	if (make_offer) {
		make_offer = false;
		parent->peer_connection->CreateAnswer(parent->ptr_csdo, webrtc::PeerConnectionInterface::RTCOfferAnswerOptions());
	}
}

void WebRTCLibPeerConnection::GodotSSDO::OnFailure(webrtc::RTCError error) {
	make_offer = false;
	ERR_PRINT(error.message());
}

// CreateSessionDescriptionObserver
void WebRTCLibPeerConnection::GodotCSDO::OnSuccess(webrtc::SessionDescriptionInterface *desc) {
	// serialize this offer and send it to the remote peer:
	std::string sdp;
	desc->ToString(&sdp);
	parent->queue_signal("session_description_created", 2, desc->type().c_str(), sdp.c_str());
}

void WebRTCLibPeerConnection::GodotCSDO::OnFailure(webrtc::RTCError error) {
	ERR_PRINT(error.message());
}

void WebRTCLibPeerConnection::initialize_signaling() {
	if (signaling_thread.get() == nullptr) {
		signaling_thread = rtc::Thread::Create();
	}
	signaling_thread->Start();
}

void WebRTCLibPeerConnection::deinitialize_signaling() {
	if (signaling_thread.get() != nullptr) {
		signaling_thread->Stop();
	}
}

Error WebRTCLibPeerConnection::_parse_ice_server(webrtc::PeerConnectionInterface::RTCConfiguration &r_config, Dictionary p_server) {
	Variant nil;
	Variant v;
	webrtc::PeerConnectionInterface::IceServer ice_server;
	String url;

	ERR_FAIL_COND_V(!p_server.has("urls"), ERR_INVALID_PARAMETER);

	// Parse mandatory URL
	v = p_server.get("urls", nil);
	if (v.get_type() == Variant::STRING) {
		url = v;
		ice_server.urls.push_back(url.utf8().get_data());
	} else if (v.get_type() == Variant::ARRAY) {
		Array names = v;
		// TODO No accessors for arrays
#if 0
		for (int j = 0; j < names.size(); j++) {
			v = names[j];
			ERR_FAIL_COND_V(v.get_type() != Variant::STRING, ERR_INVALID_PARAMETER);
			url = v;
			ice_server.urls.push_back(url.utf8().get_data());
		}
#endif
	} else {
		ERR_FAIL_V(ERR_INVALID_PARAMETER);
	}
	// Parse credentials (only meaningful for TURN, only support password)
	if (p_server.has("username") && (v = p_server.get("username", nil)) && v.get_type() == Variant::STRING) {
		ice_server.username = (v.operator String()).utf8().get_data();
	}
	if (p_server.has("credential") && (v = p_server.get("credential", nil)) && v.get_type() == Variant::STRING) {
		ice_server.password = (v.operator String()).utf8().get_data();
	}

	r_config.servers.push_back(ice_server);
	return OK;
}

Error WebRTCLibPeerConnection::_parse_channel_config(webrtc::DataChannelInit &r_config, const Dictionary &p_dict) {
	Variant nil;
	Variant v;
#define _SET_N(PROP, PNAME, TYPE)          \
	if (p_dict.has(#PROP)) {               \
		v = p_dict.get(#PROP, nil);        \
		if (v.get_type() == Variant::TYPE) \
			r_config.PNAME = v;            \
	}
#define _SET(PROP, TYPE) _SET_N(PROP, PROP, TYPE)
	_SET(negotiated, BOOL);
	_SET(id, INT);
	_SET_N(maxPacketLifeTime, maxRetransmitTime, INT);
	_SET(maxRetransmits, INT);
	_SET(ordered, BOOL);
#undef _SET
	if (p_dict.has("protocol") && (v = p_dict.get("protocol", nil)) && v.get_type() == Variant::STRING) {
		r_config.protocol = v.operator String().utf8().get_data();
	}

	// ID makes sense only when negotiated is true (and must be set in that case)
	ERR_FAIL_COND_V(r_config.negotiated ? r_config.id == -1 : r_config.id != -1, ERR_INVALID_PARAMETER);
	// Only one of maxRetransmits and maxRetransmitTime can be set on a channel.
	ERR_FAIL_COND_V(r_config.maxRetransmits && r_config.maxRetransmitTime, ERR_INVALID_PARAMETER);
	return OK;
}

int64_t WebRTCLibPeerConnection::_get_connection_state() const {
	ERR_FAIL_COND_V(peer_connection.get() == nullptr, STATE_CLOSED);

	webrtc::PeerConnectionInterface::IceConnectionState state = peer_connection->ice_connection_state();
	switch (state) {
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
}

int64_t WebRTCLibPeerConnection::_initialize(const Dictionary &p_config) {
	webrtc::PeerConnectionInterface::RTCConfiguration config;
	Variant v;
	// TODO FIXME No accessors for array.
#if 0
	if (p_config.has("iceServers") && (v = p_config["iceServers"]) && v.get_type() == Variant::ARRAY) {
		Array servers = v;
		for (int i = 0; i < servers.size(); i++) {
			v = servers[i];
			ERR_FAIL_COND_V(v.get_type() != Variant::DICTIONARY, ERR_INVALID_PARAMETER);
			Dictionary server = v;
			Error err = _parse_ice_server(config, server);
			ERR_FAIL_COND_V(err != OK, err);
		}
	}
#endif
	return _create_pc(config);
}

Object *WebRTCLibPeerConnection::_create_data_channel(const String &p_channel, const Dictionary &p_channel_config) {
	ERR_FAIL_COND_V(peer_connection.get() == nullptr, nullptr);

	// Read config from dictionary
	webrtc::DataChannelInit config;
	Error err = _parse_channel_config(config, p_channel_config);
	ERR_FAIL_COND_V(err != OK, nullptr);

	rtc::scoped_refptr<webrtc::DataChannelInterface> ch = peer_connection->CreateDataChannel(p_channel.utf8().get_data(), &config);
	WebRTCLibDataChannel *wrapper = WebRTCLibDataChannel::new_data_channel(ch);
	ERR_FAIL_COND_V(wrapper == nullptr, nullptr);
	return wrapper;
}

int64_t WebRTCLibPeerConnection::_create_offer() {
	ERR_FAIL_COND_V(peer_connection.get() == nullptr, ERR_UNCONFIGURED);
	peer_connection->CreateOffer(ptr_csdo, webrtc::PeerConnectionInterface::RTCOfferAnswerOptions());
	return OK;
}

#define _MAKE_DESC(TYPE, SDP) webrtc::CreateSessionDescription((String(TYPE) == String("offer") ? webrtc::SdpType::kOffer : webrtc::SdpType::kAnswer), SDP.utf8().get_data())
int64_t WebRTCLibPeerConnection::_set_remote_description(const String &type, const String &sdp) {
	ERR_FAIL_COND_V(peer_connection.get() == nullptr, ERR_UNCONFIGURED);
	std::unique_ptr<webrtc::SessionDescriptionInterface> desc = _MAKE_DESC(type, sdp);
	if (desc->GetType() == webrtc::SdpType::kOffer) {
		ptr_ssdo->make_offer = true;
	}
	peer_connection->SetRemoteDescription(ptr_ssdo, desc.release());
	return OK;
}

int64_t WebRTCLibPeerConnection::_set_local_description(const String &type, const String &sdp) {
	ERR_FAIL_COND_V(peer_connection.get() == nullptr, ERR_UNCONFIGURED);
	std::unique_ptr<webrtc::SessionDescriptionInterface> desc = _MAKE_DESC(type, sdp);
	peer_connection->SetLocalDescription(ptr_ssdo, desc.release());
	return OK;
}
#undef _MAKE_DESC

int64_t WebRTCLibPeerConnection::_add_ice_candidate(const String &sdpMidName, int64_t sdpMlineIndexName, const String &sdpName) {
	ERR_FAIL_COND_V(peer_connection.get() == nullptr, ERR_UNCONFIGURED);

	webrtc::SdpParseError *error = nullptr;
	webrtc::IceCandidateInterface *candidate = webrtc::CreateIceCandidate(
			sdpMidName.utf8().get_data(),
			sdpMlineIndexName,
			sdpName.utf8().get_data(),
			error);

	ERR_FAIL_COND_V(error || !candidate, ERR_INVALID_PARAMETER);
	ERR_FAIL_COND_V(!peer_connection->AddIceCandidate(candidate), FAILED);

	return OK;
}

int64_t WebRTCLibPeerConnection::_poll() {
	ERR_FAIL_COND_V(peer_connection.get() == nullptr, ERR_UNCONFIGURED);

	while (!signal_queue.empty()) {
		mutex_signal_queue->lock();
		Signal signal = signal_queue.front();
		signal_queue.pop();
		mutex_signal_queue->unlock();
		signal.emit(this);
	}
	return OK;
}

void WebRTCLibPeerConnection::_close() {
	if (peer_connection.get() != nullptr) {
		peer_connection->Close();
	}
	peer_connection = nullptr;
	while (!signal_queue.empty()) {
		signal_queue.pop();
	}
}

void WebRTCLibPeerConnection::_init() {
	// initialize variables:
	mutex_signal_queue = new std::mutex;

	// create a PeerConnectionFactoryInterface:
	webrtc::PeerConnectionFactoryDependencies deps;

	ERR_FAIL_COND(signaling_thread.get() == nullptr);
	deps.signaling_thread = signaling_thread.get();
	pc_factory = webrtc::CreateModularPeerConnectionFactory(std::move(deps));

	// Create peer connection with default configuration.
	webrtc::PeerConnectionInterface::RTCConfiguration config;
	_create_pc(config);
}

Error WebRTCLibPeerConnection::_create_pc(webrtc::PeerConnectionInterface::RTCConfiguration &config) {
	ERR_FAIL_COND_V(pc_factory.get() == nullptr, ERR_BUG);
	peer_connection = nullptr;
	peer_connection = pc_factory->CreatePeerConnection(config, nullptr, nullptr, &pco);
	if (peer_connection.get() == nullptr) { // PeerConnection couldn't be created. Fail the method call.
		ERR_PRINT("PeerConnection could not be created");
		return FAILED;
	}
	return OK;
}

WebRTCLibPeerConnection::WebRTCLibPeerConnection() :
		pco(this),
		ptr_csdo(new rtc::RefCountedObject<GodotCSDO>(this)),
		ptr_ssdo(new rtc::RefCountedObject<GodotSSDO>(this)) {
	_init();
}

WebRTCLibPeerConnection::~WebRTCLibPeerConnection() {
	_close();
	delete mutex_signal_queue;
}

void WebRTCLibPeerConnection::queue_signal(String p_name, int p_argc, const Variant &p_arg1, const Variant &p_arg2, const Variant &p_arg3) {
	mutex_signal_queue->lock();
	const Variant argv[3] = { p_arg1, p_arg2, p_arg3 };
	signal_queue.push(Signal(p_name, p_argc, argv));
	mutex_signal_queue->unlock();
}
