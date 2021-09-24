/*************************************************************************/
/*  WebRTCLibPeerConnection.hpp                                          */
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

#ifndef WEBRTC_PEER_H
#define WEBRTC_PEER_H

#include "api/peer_connection_interface.h" // interface for all things needed from WebRTC
#include "media/base/media_engine.h" // needed for CreateModularPeerConnectionFactory
#include <mutex>

#include <godot_cpp/classes/web_rtc_peer_connection_extension.hpp>

namespace godot_webrtc {

class WebRTCLibPeerConnection : public godot::WebRTCPeerConnectionExtension {
	GDCLASS(WebRTCLibPeerConnection, WebRTCPeerConnectionExtension);

private:
	static std::unique_ptr<rtc::Thread> signaling_thread;

	godot::Error _create_pc(webrtc::PeerConnectionInterface::RTCConfiguration &config);
	godot::Error _parse_ice_server(webrtc::PeerConnectionInterface::RTCConfiguration &r_config, godot::Dictionary p_server);
	godot::Error _parse_channel_config(webrtc::DataChannelInit &r_config, const godot::Dictionary &p_dict);

protected:
	static void _bind_methods() {}

public:
	static void _register_methods() {}
	static void initialize_signaling();
	static void deinitialize_signaling();

	void _init();

	int64_t _get_connection_state() const;

	int64_t _initialize(const godot::Dictionary &p_config) override;
	godot::Object *_create_data_channel(const godot::String &p_channel, const godot::Dictionary &p_channel_config) override;
	int64_t _create_offer() override;
	int64_t _set_remote_description(const godot::String &type, const godot::String &sdp) override;
	int64_t _set_local_description(const godot::String &type, const godot::String &sdp) override;
	int64_t _add_ice_candidate(const godot::String &sdpMidName, int64_t sdpMlineIndexName, const godot::String &sdpName) override;
	int64_t _poll() override;
	void _close() override;

	WebRTCLibPeerConnection();
	~WebRTCLibPeerConnection();

private:
	/* helper functions */
	void queue_signal(godot::String p_name, int p_argc, const godot::Variant &p_arg1 = godot::Variant(), const godot::Variant &p_arg2 = godot::Variant(), const godot::Variant &p_arg3 = godot::Variant());
	void queue_packet(uint8_t *, int);

	/** PeerConnectionObserver callback functions **/
	class GodotPCO : public webrtc::PeerConnectionObserver {
	public:
		WebRTCLibPeerConnection *parent;

		GodotPCO(WebRTCLibPeerConnection *p_parent) {
			parent = p_parent;
		}
		void OnIceCandidate(const webrtc::IceCandidateInterface *candidate) override;

		void OnSignalingChange(webrtc::PeerConnectionInterface::SignalingState new_state) override {}
		void OnAddStream(rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) override {}
		void OnRemoveStream(rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) override {}
		void OnDataChannel(rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel) override {}
		void OnRenegotiationNeeded() override {}
		void OnIceConnectionChange(webrtc::PeerConnectionInterface::IceConnectionState new_state) override {}
		void OnIceGatheringChange(webrtc::PeerConnectionInterface::IceGatheringState new_state) override {}
	};

	/** CreateSessionDescriptionObserver callback functions **/
	class GodotCSDO : public webrtc::CreateSessionDescriptionObserver {
	public:
		WebRTCLibPeerConnection *parent = nullptr;

		GodotCSDO(WebRTCLibPeerConnection *p_parent) {
			parent = p_parent;
		}
		void OnSuccess(webrtc::SessionDescriptionInterface *desc) override;
		void OnFailure(webrtc::RTCError error) override;
	};

	/** SetSessionDescriptionObserver callback functions **/
	class GodotSSDO : public webrtc::SetSessionDescriptionObserver {
	public:
		WebRTCLibPeerConnection *parent = nullptr;
		bool make_offer = false;

		GodotSSDO(WebRTCLibPeerConnection *p_parent) {
			parent = p_parent;
		}
		void OnSuccess() override;
		void OnFailure(webrtc::RTCError error) override;
	};

	class Signal {
		godot::String method;
		godot::Variant argv[3];
		int argc = 0;

	public:
		Signal(godot::String p_method, int p_argc, const godot::Variant *p_argv) {
			method = p_method;
			argc = p_argc;
			for (int i = 0; i < argc; i++) {
				argv[i] = p_argv[i];
			}
		}

		void emit(godot::Object *p_object) {
			if (argc == 0) {
				p_object->emit_signal(method);
			} else if (argc == 1) {
				p_object->emit_signal(method, argv[0]);
			} else if (argc == 2) {
				p_object->emit_signal(method, argv[0], argv[1]);
			} else if (argc == 3) {
				p_object->emit_signal(method, argv[0], argv[1], argv[2]);
			}
		}
	};

	GodotPCO pco;
	rtc::scoped_refptr<GodotSSDO> ptr_ssdo;
	rtc::scoped_refptr<GodotCSDO> ptr_csdo;

	std::mutex *mutex_signal_queue = nullptr;
	std::queue<Signal> signal_queue;

	rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> pc_factory;
	rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection;
};

} // namespace godot_webrtc

#endif // WEBRTC_PEER_H
