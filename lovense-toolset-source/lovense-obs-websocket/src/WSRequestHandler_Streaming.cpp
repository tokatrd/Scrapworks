#include "obs-websocket.h"
#include "Utils.h"
#include "WSEvents.h"

#include "WSRequestHandler.h"

#define STREAM_SERVICE_ID "websocket_custom_service"

 /**
 * Get current streaming and recording status.
 *
 * @return {boolean} `streaming` Current streaming status.
 * @return {boolean} `recording` Current recording status.
 * @return {boolean} `recording-paused` If recording is paused.
 * @return {boolean} `virtualcam` Current virtual cam status.
 * @return {boolean} `preview-only` Always false. Retrocompatibility with OBSRemote.
 * @return {String (optional)} `stream-timecode` Time elapsed since streaming started (only present if currently streaming).
 * @return {String (optional)} `rec-timecode` Time elapsed since recording started (only present if currently recording).
 * @return {String (optional)} `virtualcam-timecode` Time elapsed since virtual cam started (only present if virtual cam currently active).
 *
 * @api requests
 * @name GetStreamingStatus
 * @category streaming
 * @since 0.3
 */
RpcResponse WSRequestHandler::GetStreamingStatus(const RpcRequest& request) {
	auto events = GetEventsSystem();

	OBSDataAutoRelease data = obs_data_create();
	obs_data_set_bool(data, "streaming", obs_frontend_streaming_active());
	obs_data_set_bool(data, "recording", obs_frontend_recording_active());
	obs_data_set_bool(data, "recording-paused", obs_frontend_recording_paused());
	obs_data_set_bool(data, "virtualcam", obs_frontend_virtualcam_active());
	obs_data_set_bool(data, "preview-only", false);

	if (obs_frontend_streaming_active()) {
		QString streamingTimecode = events->getStreamingTimecode();
#ifndef DISABLE_QT
		obs_data_set_string(data, "stream-timecode", streamingTimecode.toUtf8().constData());
#else
		obs_data_set_string(data, "stream-timecode", streamingTimecode.toUtf8());
#endif
	}

	if (obs_frontend_recording_active()) {
		QString recordingTimecode = events->getRecordingTimecode();
#ifndef DISABLE_QT
		obs_data_set_string(data, "rec-timecode", recordingTimecode.toUtf8().constData());
#else
		obs_data_set_string(data, "rec-timecode", recordingTimecode.toUtf8());
#endif
	}

	if (obs_frontend_virtualcam_active()) {
		QString virtualCamTimecode = events->getVirtualCamTimecode();
#ifndef DISABLE_QT
		obs_data_set_string(data, "virtualcam-timecode", virtualCamTimecode.toUtf8().constData());
#else
		obs_data_set_string(data, "virtualcam-timecode", virtualCamTimecode.toUtf8());
#endif
	}

	return request.success(data);
}

/**
 * Toggle streaming on or off (depending on the current stream state).
 *
 * @api requests
 * @name StartStopStreaming
 * @category streaming
 * @since 0.3
 */
RpcResponse WSRequestHandler::StartStopStreaming(const RpcRequest& request) {
	if (obs_frontend_streaming_active())
		return StopStreaming(request);
	else
		return StartStreaming(request);
}

/**
 * Start streaming.
 * Will return an `error` if streaming is already active.
 *
 * @param {Object (optional)} `stream` Special stream configuration. Note: these won't be saved to OBS' configuration.
 * @param {String (optional)} `stream.type` If specified ensures the type of stream matches the given type (usually 'rtmp_custom' or 'rtmp_common'). If the currently configured stream type does not match the given stream type, all settings must be specified in the `settings` object or an error will occur when starting the stream.
 * @param {Object (optional)} `stream.metadata` Adds the given object parameters as encoded query string parameters to the 'key' of the RTMP stream. Used to pass data to the RTMP service about the streaming. May be any String, Numeric, or Boolean field.
 * @param {Object (optional)} `stream.settings` Settings for the stream.
 * @param {String (optional)} `stream.settings.server` The publish URL.
 * @param {String (optional)} `stream.settings.key` The publish key of the stream.
 * @param {boolean (optional)} `stream.settings.use_auth` Indicates whether authentication should be used when connecting to the streaming server.
 * @param {String (optional)} `stream.settings.username` If authentication is enabled, the username for the streaming server. Ignored if `use_auth` is not set to `true`.
 * @param {String (optional)} `stream.settings.password` If authentication is enabled, the password for the streaming server. Ignored if `use_auth` is not set to `true`.
 *
 * @api requests
 * @name StartStreaming
 * @category streaming
 * @since 4.1.0
 */
RpcResponse WSRequestHandler::StartStreaming(const RpcRequest& request) {
	
	QString id = request.messageId();
	int iid = id.toInt();
	if (obs_frontend_streaming_active() == false) {
		OBSService configuredService = obs_frontend_get_streaming_service();
		OBSService newService = nullptr;

		// TODO: fix service memory leak

		if (request.hasField("stream")) {
			OBSDataAutoRelease streamData = obs_data_get_obj(request.parameters(), "stream");
			OBSDataAutoRelease newSettings = obs_data_get_obj(streamData, "settings");
			OBSDataAutoRelease newMetadata = obs_data_get_obj(streamData, "metadata");

			OBSDataAutoRelease csHotkeys =
				obs_hotkeys_save_service(configuredService);

			QString currentType = obs_service_get_type(configuredService);
			QString newType = obs_data_get_string(streamData, "type");
			if (newType.isEmpty() || newType.isNull()) {
				newType = currentType;
			}

			//Supporting adding metadata parameters to key query string
			QString query = Utils::ParseDataToQueryString(newMetadata);
			if (!query.isEmpty()
					&& obs_data_has_user_value(newSettings, "key"))
			{
				const char* key = obs_data_get_string(newSettings, "key");
				size_t keylen = strlen(key);

				bool hasQuestionMark = false;
				for (size_t i = 0; i < keylen; i++) {
					if (key[i] == '?') {
						hasQuestionMark = true;
						break;
					}
				}

				if (hasQuestionMark) {
					query.prepend('&');
				} else {
					query.prepend('?');
				}

				query.prepend(key);
				obs_data_set_string(newSettings, "key", query.toUtf8());
			}

			if (newType == currentType) {
				// Service type doesn't change: apply settings to current service

				// By doing this, you can send a request to the websocket
				// that only contains settings you want to change, instead of
				// having to do a get and then change them

				OBSDataAutoRelease currentSettings = obs_service_get_settings(configuredService);
				OBSDataAutoRelease updatedSettings = obs_data_create();

				obs_data_apply(updatedSettings, currentSettings); //first apply the existing settings
				obs_data_apply(updatedSettings, newSettings); //then apply the settings from the request should they exist

				newService = obs_service_create(
					newType.toUtf8(), STREAM_SERVICE_ID,
					updatedSettings, csHotkeys);
			}
			else {
				// Service type changed: override service settings
				newService = obs_service_create(
					newType.toUtf8(), STREAM_SERVICE_ID,
					newSettings, csHotkeys);
			}

			obs_frontend_set_streaming_service(newService);
		}

		obs_frontend_streaming_start();
        
        calldata_t cData;
        calldata_init(&cData);
        calldata_set_bool(&cData, "ConnectSuccess", false);
#ifdef WIN32
		HANDLE eventConnect = CreateEvent(NULL,false,false,NULL);
        calldata_set_int(&cData, "ConnectEvent", (long long)eventConnect);
#else
        std::mutex notifyMutex;
        std::condition_variable notifyCV;
        calldata_set_int(&cData, "ConnectEvent", (long long)&notifyCV);
#endif
        
		auto eventCallback = [](enum obs_frontend_event event, void* param) {

			calldata_t* data = (calldata_t*)param;
			
#ifdef WIN32
            long long ievent = 0;
            calldata_get_int(data,"ConnectEvent",&ievent);
			HANDLE pevent = (HANDLE)ievent;
			if (event == OBS_FRONTEND_EVENT_STREAMING_STARTED) {
				
				//return request.success();
				calldata_set_bool(data, "ConnectSuccess", true);
				SetEvent(pevent);
			}
			else if (event == OBS_FRONTEND_EVENT_STREAMING_STOPPED)
			{
				//return request.failed("streaming already active");
				calldata_set_bool(data, "ConnectSuccess", false);
				SetEvent(pevent);
			}
#else
            long long pvc = 0;
            calldata_get_int(data,"ConnectEvent",&pvc);
            std::condition_variable *vc = (std::condition_variable *)pvc;
            if (event == OBS_FRONTEND_EVENT_STREAMING_STARTED) {
                //return request.success();
                calldata_set_bool(data, "ConnectSuccess", true);
                vc->notify_all();
            }
            else if (event == OBS_FRONTEND_EVENT_STREAMING_STOPPED)
            {
                //return request.failed("streaming already active");
                calldata_set_bool(data, "ConnectSuccess", false);
                vc->notify_all();
            }
#endif
		};
		obs_frontend_add_event_callback(eventCallback, (void*)&cData);
#ifdef WIN32
		WaitForSingleObject(eventConnect, INFINITE);
		CloseHandle(eventConnect);
#else
        {
            std::unique_lock<std::mutex> lock(notifyMutex);
            notifyCV.wait(lock);
        }
#endif
		bool bSuccess = false;
		calldata_get_bool(&cData, "ConnectSuccess", &bSuccess);
		obs_frontend_remove_event_callback(eventCallback, (void*)&cData);

		// Stream settings provided in StartStreaming are not persisted to disk
		if (newService != nullptr) {
			obs_frontend_set_streaming_service(configuredService);
		}
		if(bSuccess)
			return request.success();
		else
			return request.failed("streaming connect error");

	} else {
		return request.failed("streaming already active");
	}
}

/**
 * Stop streaming.
 * Will return an `error` if streaming is not active.
 *
 * @api requests
 * @name StopStreaming
 * @category streaming
 * @since 4.1.0
 */
RpcResponse WSRequestHandler::StopStreaming(const RpcRequest& request) {
	if (obs_frontend_streaming_active() == true) {
		obs_frontend_streaming_stop();
		return request.success();
	} else {
		return request.failed("streaming not active");
	}
}

/**
 * Sets one or more attributes of the current streaming server settings. Any options not passed will remain unchanged. Returns the updated settings in response. If 'type' is different than the current streaming service type, all settings are required. Returns the full settings of the stream (the same as GetStreamSettings).
 *
 * @param {String} `type` The type of streaming service configuration, usually `rtmp_custom` or `rtmp_common`.
 * @param {Object} `settings` The actual settings of the stream.
 * @param {String (optional)} `settings.server` The publish URL.
 * @param {String (optional)} `settings.key` The publish key.
 * @param {boolean (optional)} `settings.use_auth` Indicates whether authentication should be used when connecting to the streaming server.
 * @param {String (optional)} `settings.username` The username for the streaming service.
 * @param {String (optional)} `settings.password` The password for the streaming service.
 * @param {boolean} `save` Persist the settings to disk.
 *
 * @api requests
 * @name SetStreamSettings
 * @category streaming
 * @since 4.1.0
 */
RpcResponse WSRequestHandler::SetStreamSettings(const RpcRequest& request) {
	OBSService service = obs_frontend_get_streaming_service();

	OBSDataAutoRelease requestSettings = obs_data_get_obj(request.parameters(), "settings");
	if (!requestSettings) {
		return request.failed("'settings' are required'");
	}

	bool bUpdate = false;
	if (service) {
		obs_data_t* oldSettings = obs_service_get_settings(service);
		const char* szservice = obs_data_get_string(oldSettings, "service");
		const char* server = obs_data_get_string(oldSettings, "server");
		const char* key = obs_data_get_string(oldSettings, "key");
		obs_data_release(oldSettings);
		
		const char* newservice = obs_data_get_string(requestSettings, "service");
		const char* newserver = obs_data_get_string(requestSettings, "server");
		const char* newkey = obs_data_get_string(requestSettings, "key");

		if ( strcmp(szservice, newservice) !=0 || strcmp(server, newserver) != 0 || strcmp(key , newkey) !=0 )
		{
			bUpdate = true;
		}
	}


	QString serviceType = obs_service_get_type(service);
	QString requestedType = obs_data_get_string(request.parameters(), "type");

	if (requestedType != nullptr && requestedType != serviceType) {
		OBSDataAutoRelease hotkeys = obs_hotkeys_save_service(service);
		service = obs_service_create(
			requestedType.toUtf8(), STREAM_SERVICE_ID, requestSettings, hotkeys);
		obs_frontend_set_streaming_service(service);
	} else {
		// If type isn't changing, we should overlay the settings we got
		// to the existing settings. By doing so, you can send a request that
		// only contains the settings you want to change, instead of having to
		// do a get and then change them

		OBSDataAutoRelease existingSettings = obs_service_get_settings(service);
		OBSDataAutoRelease newSettings = obs_data_create();

		// Apply existing settings
		obs_data_apply(newSettings, existingSettings);
		// Then apply the settings from the request
		obs_data_apply(newSettings, requestSettings);

		obs_service_update(service, newSettings);
	}

	//if save is specified we should immediately save the streaming service
	if (obs_data_get_bool(request.parameters(), "save")) {
		obs_frontend_save_streaming_service();
	}

	//cita:add automic streaming
	/*
	if (bUpdate) {
		if (obs_frontend_streaming_active())
			obs_frontend_streaming_stop();
		QThread::msleep(1000);
		obs_frontend_streaming_active();
		obs_frontend_streaming_start();
	}
	else
	{
		if (!obs_frontend_streaming_active())
			obs_frontend_streaming_start();
	}*/

	OBSService responseService = obs_frontend_get_streaming_service();
	OBSDataAutoRelease serviceSettings = obs_service_get_settings(responseService);
	const char* responseType = obs_service_get_type(responseService);

	OBSDataAutoRelease response = obs_data_create();
	obs_data_set_string(response, "type", responseType);
	obs_data_set_obj(response, "settings", serviceSettings);

	return request.success(response);
}

/**
 * Get the current streaming server settings.
 *
 * @return {String} `type` The type of streaming service configuration. Possible values: 'rtmp_custom' or 'rtmp_common'.
 * @return {Object} `settings` Stream settings object.
 * @return {String} `settings.server` The publish URL.
 * @return {String} `settings.key` The publish key of the stream.
 * @return {boolean} `settings.use_auth` Indicates whether authentication should be used when connecting to the streaming server.
 * @return {String} `settings.username` The username to use when accessing the streaming server. Only present if `use_auth` is `true`.
 * @return {String} `settings.password` The password to use when accessing the streaming server. Only present if `use_auth` is `true`.
 *
 * @api requests
 * @name GetStreamSettings
 * @category streaming
 * @since 4.1.0
 */
RpcResponse WSRequestHandler::GetStreamSettings(const RpcRequest& request) {
	OBSService service = obs_frontend_get_streaming_service();

	const char* serviceType = obs_service_get_type(service);
	OBSDataAutoRelease settings = obs_service_get_settings(service);

	OBSDataAutoRelease response = obs_data_create();
	obs_data_set_string(response, "type", serviceType);
	obs_data_set_obj(response, "settings", settings);

	return request.success(response);
}

/**
 * Save the current streaming server settings to disk.
 *
 * @api requests
 * @name SaveStreamSettings
 * @category streaming
 * @since 4.1.0
 */
RpcResponse WSRequestHandler::SaveStreamSettings(const RpcRequest& request) {
	obs_frontend_save_streaming_service();
	return request.success();
}


/**
 * Send the provided text as embedded CEA-608 caption data.
 *
 * @param {String} `text` Captions text
 *
 * @api requests
 * @name SendCaptions
 * @category streaming
 * @since 4.6.0
 */
RpcResponse WSRequestHandler::SendCaptions(const RpcRequest& request) {
	if (!request.hasField("text")) {
		return request.failed("missing request parameters");
	}

	OBSOutputAutoRelease output = obs_frontend_get_streaming_output();
	if (output) {
		const char* caption = obs_data_get_string(request.parameters(), "text");
		// Send caption text with immediately (0 second delay)
		obs_output_output_caption_text2(output, caption, 0.0);
	}

	return request.success();
}

