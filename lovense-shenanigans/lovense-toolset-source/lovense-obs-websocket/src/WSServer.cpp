/*
obs-websocket
Copyright (C) 2016-2017	Stéphane Lepin <stephane.lepin@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program. If not, see <https://www.gnu.org/licenses/>
*/

#include <chrono>
#include <thread>


#if 0
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QMessageBox>
#endif

#include <obs-frontend-api.h>
#include <util/platform.h>

#include "WSServer.h"
#include "obs-websocket.h"
#include "Config.h"
#include "Utils.h"
#include "protocol/OBSRemoteProtocol.h"
#include "lovense_obs_proc.hpp"

#ifndef DISABLE_QT
QT_USE_NAMESPACE
#endif

using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;

WSServer::WSServer()
#ifndef DISABLE_QT
	: QObject(nullptr),
	  _connections()
#else
	: _connections()
	, _threadPool(1)
#endif
{
	_server.get_alog().clear_channels(websocketpp::log::alevel::frame_header | websocketpp::log::alevel::frame_payload | websocketpp::log::alevel::control);
	_server.init_asio();
#ifndef _WIN32
	_server.set_reuse_addr(true);
#endif

	_server.set_open_handler(bind(&WSServer::onOpen, this, ::_1));
	_server.set_close_handler(bind(&WSServer::onClose, this, ::_1));
	_server.set_message_handler(bind(&WSServer::onMessage, this, ::_1, ::_2));
}

WSServer::~WSServer()
{
	stop();
}

void WSServer::NotifyScriptUpdated()
{
	RpcEvent event(QString("ScriptUpdated"), std::optional<uint64_t>(), std::optional<uint64_t>());
	this->broadcast(event);
	//this->broadcast(RpcProtocol::NotifyUpdated());
}

void WSServer::serverRunner()
{
	blog(LOG_INFO, "IO thread started.");
	try {
		_server.run();
	} catch (websocketpp::exception const & e) {
		blog(LOG_ERROR, "websocketpp instance returned an error: %s", e.what());
	} catch (const std::exception & e) {
		blog(LOG_ERROR, "websocketpp instance returned an error: %s", e.what());
	} catch (...) {
		blog(LOG_ERROR, "websocketpp instance returned an error");
	}
	blog(LOG_INFO, "IO thread exited.");
}

void WSServer::start(quint16 port, bool lockToIPv4)
{
	if (_server.is_listening() && (port == _serverPort && _lockToIPv4 == lockToIPv4)) {
		blog(LOG_INFO, "WSServer::start: server already on this port and protocol mode. no restart needed");
		return;
	}

	if (_server.is_listening()) {
		stop();
	}

	_server.reset();

	_serverPort = port;
	_lockToIPv4 = lockToIPv4;

	websocketpp::lib::error_code errorCode;
	if (lockToIPv4) {
		blog(LOG_INFO, "WSServer::start: Locked to IPv4 bindings");
		_server.listen(websocketpp::lib::asio::ip::tcp::v4(), _serverPort, errorCode);
	} else {
		blog(LOG_INFO, "WSServer::start: Not locked to IPv4 bindings");
		_server.listen(_serverPort, errorCode);
	}

	//lovense
	setupProcHandler();

	lovense::proc::call_BiLogWebsocketStart(!errorCode, errorCode.message().c_str());


	if (errorCode) {
		std::string errorCodeMessage = errorCode.message();
		blog(LOG_INFO, "server: listen failed: %s", errorCodeMessage.c_str());
		/*

			obs_frontend_push_ui_translation(obs_module_get_string);
			QString errorTitle = tr("OBSWebsocket.Server.StartFailed.Title");
			QString errorMessage = tr("OBSWebsocket.Server.StartFailed.Message").arg(_serverPort).arg(errorCodeMessage.c_str());
			obs_frontend_pop_ui_translation();

			QMainWindow* mainWindow = reinterpret_cast<QMainWindow*>(obs_frontend_get_main_window());
			QMessageBox::warning(mainWindow, errorTitle, errorMessage);
		*/
		return;
	}

	_server.start_accept();

	_serverThread = std::thread(&WSServer::serverRunner, this);

	blog(LOG_INFO, "server started successfully on port %d", _serverPort);
}

void WSServer::stop()
{
	if (!_server.is_listening()) {
		return;
	}

	// Stop listening and handle active connections
	_server.stop_listening();
	for (connection_hdl hdl : _connections) {
		websocketpp::lib::error_code errorCode;
		_server.pause_reading(hdl, errorCode);
		if (errorCode) {
			blog(LOG_ERROR, "Error: %s", errorCode.message().c_str());
			continue;
		}

		_server.close(hdl, websocketpp::close::status::going_away, "Server stopping", errorCode);
		if (errorCode) {
			blog(LOG_ERROR, "Error: %s", errorCode.message().c_str());
			continue;
		}
	}

#ifndef DISABLE_QT
	_threadPool.waitForDone();
#else
	_threadPool.Stop();
#endif

	while (_connections.size() > 0) {
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}

	_serverThread.join();

	blog(LOG_INFO, "server stopped successfully");
}

void WSServer::broadcast(const RpcEvent& event)
{
	std::string message = OBSRemoteProtocol::encodeEvent(event);

	auto config = GetConfig();
	if (config && config->DebugEnabled) {
		//blog(LOG_INFO, "Update << '%s'", message.c_str());
	}
#ifdef _DEBUG
	blog(LOG_INFO, "broadcast event: << '%s'", message.c_str());
#endif

#ifndef DISABLE_QT
	QMutexLocker locker(&_clMutex);
#else
	std::lock_guard lk(locker);
#endif	
	for (connection_hdl hdl : _connections) {
		if (config && config->AuthRequired) {
			bool authenticated = _connectionProperties[hdl].isAuthenticated();
			if (!authenticated) {
				continue;
			}
		}

		websocketpp::lib::error_code errorCode;
		_server.send(hdl, message, websocketpp::frame::opcode::text, errorCode);

		if (errorCode) {
			std::string errorCodeMessage = errorCode.message();
			blog(LOG_INFO, "server(broadcast): send failed: %s",
				errorCodeMessage.c_str());
		}
	}
}

bool WSServer::isListening()
{
	return _server.is_listening();
}

void WSServer::setupProcHandler()
{
	//add lovense
	proc_handler_t* handle = obs_get_proc_handler();
	if (handle)
	{
		proc_handler_add(handle, "void NotifyScriptUpdated()", [](void* data, calldata_t* callData) {
				WSServer* server = (WSServer*)data;
				if (server)
				{
					server->NotifyScriptUpdated();
				}
		}, this);
	}
}

void WSServer::onOpen(connection_hdl hdl)
{
#ifndef DISABLE_QT
	QMutexLocker locker(&_clMutex);
#else
	locker.lock();
#endif
	_connections.insert(hdl);

	locker.unlock();

	//QString clientIp = getRemoteEndpoint(hdl);
	//notifyConnection(clientIp);
	//blog(LOG_INFO, "new client connection from %s", clientIp.toUtf8().constData());
}


extern void proc_close_alldialogs();
void WSServer::onMessage(connection_hdl hdl, server::message_ptr message)
{
	auto opcode = message->get_opcode();
	if (opcode != websocketpp::frame::opcode::text) {
		return;
	}

#ifndef DISABLE_QT
	QFuture<void> future = QtConcurrent::run(&_threadPool, [=]() {
#else
	_threadPool.enqueue([=]() {
#endif
		std::string payload = message->get_payload();

#ifndef DISABLE_QT
		QMutexLocker locker(&_clMutex);
#else
		locker.lock();
#endif
		ConnectionProperties& connProperties = _connectionProperties[hdl];// Protocol encryption status
		locker.unlock();

		auto config = GetConfig();
		if (config && config->DebugEnabled) {
			blog(LOG_INFO, "Request >> '%s'", payload.c_str());
		}
		bool shouldColseDialog = true;
		OBSDataAutoRelease data = obs_data_create_from_json(payload.c_str());
		if (data) {
			if (obs_data_has_user_value(data, "request_type")) {
				const char* methodName = obs_data_get_string(data, "request_type");
				if (methodName && strcmp(methodName, "TakeSourceScreenshot") == 0) {
					shouldColseDialog = false;
				}
			}
		}
		if (shouldColseDialog)
		{
			//TODO:cita add: Close dialog box before processing message
			proc_close_alldialogs();
		}

		WSRequestHandler requestHandler(connProperties);
		std::string response = OBSRemoteProtocol::processMessage(requestHandler, payload);

		if (config && config->DebugEnabled) {
			blog(LOG_INFO, "Response << '%s'", response.c_str());
		}

		websocketpp::lib::error_code errorCode;
		_server.send(hdl, response, websocketpp::frame::opcode::text, errorCode);

		if (errorCode) {
			std::string errorCodeMessage = errorCode.message();
			blog(LOG_INFO, "server(response): send failed: %s",
				errorCodeMessage.c_str());
		}
	});
}

void WSServer::onClose(connection_hdl hdl)
{
#ifndef DISABLE_QT
	QMutexLocker locker(&_clMutex);
#else
	locker.lock();
#endif
	_connections.erase(hdl);
	_connectionProperties.erase(hdl);
	locker.unlock();

	auto conn = _server.get_con_from_hdl(hdl);
	auto localCloseCode = conn->get_local_close_code();
	auto localCloseReason = conn->get_local_close_reason();
	QString clientIp = getRemoteEndpoint(hdl);

#ifndef DISABLE_QT
	blog(LOG_INFO, "Websocket connection with client '%s' closed (disconnected). Code is %d, reason is: '%s'", clientIp.toUtf8().constData(), localCloseCode, localCloseReason.c_str());
#else
	blog(LOG_INFO, "Websocket connection with client '%s' closed (disconnected). Code is %d, reason is: '%s'", clientIp.toUtf8(), localCloseCode, localCloseReason.c_str());
#endif

	if (localCloseCode != websocketpp::close::status::going_away && _server.is_listening()) {
		//cita: Suppress disconnect notification
	}
}

QString WSServer::getRemoteEndpoint(connection_hdl hdl)
{
	auto conn = _server.get_con_from_hdl(hdl);
	return QString::fromStdString(conn->get_remote_endpoint());
}

void WSServer::notifyConnection(QString clientIp)
{
#if 0
	obs_frontend_push_ui_translation(obs_module_get_string);
	QString title = tr("OBSWebsocket.NotifyConnect.Title");
	QString msg = tr("OBSWebsocket.NotifyConnect.Message").arg(clientIp);
	obs_frontend_pop_ui_translation();

	Utils::SysTrayNotify(msg, QSystemTrayIcon::Information, title);
#endif
}

void WSServer::notifyDisconnection(QString clientIp)
{
#if 0
	obs_frontend_push_ui_translation(obs_module_get_string);
	QString title = tr("OBSWebsocket.NotifyDisconnect.Title");
	QString msg = tr("OBSWebsocket.NotifyDisconnect.Message").arg(clientIp);
	obs_frontend_pop_ui_translation();

	Utils::SysTrayNotify(msg, QSystemTrayIcon::Information, title);
#endif
}
