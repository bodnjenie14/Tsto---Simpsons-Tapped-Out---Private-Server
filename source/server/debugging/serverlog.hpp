#pragma once

namespace logger
{
	enum LogLevel
	{
		LOG_LEVEL_DEBUG = 0,
		LOG_LEVEL_INFO = 1,
		LOG_LEVEL_WARN = 2,
		LOG_LEVEL_ERROR = 3,
		LOG_LEVEL_CRITICAL = 4,
		LOG_LEVEL_INCOMING = 5,
		LOG_LEVEL_RESPONSE = 6,
		LOG_LEVEL_PLAYER_ID = 7,
	};

	enum LogLabel
	{
		LOG_LABEL_INITIALIZER = 0,
		LOG_LABEL_BLACKBOX = 1,
		LOG_LABEL_SOCKET_TCP = 2,
		LOG_LABEL_SOCKET_UDP = 3,
		LOG_LABEL_SERVER_HTTP = 4,
		LOG_LABEL_TRACKING = 5,
		LOG_LABEL_GAME = 6,  // Add new tracking labels
		LOG_LABEL_LOBBY = 7,
		LOG_LABEL_AUTH = 8,
		LOG_LABEL_PIN = 9,
		LOG_LABEL_USER = 10,
		LOG_LABEL_LAND = 11,
		LOG_LABEL_SESSION = 12,
		LOG_LABEL_FILESERVER = 13,
		LOG_LABEL_EVENTS = 14,
		LOG_LABEL_DISCORD = 15,
		LOG_LABEL_UPDATE = 16,
		LOG_LABEL_DATABASE = 17,
		LOG_LABEL_INDEPNDENT = -1
		
	};

	void write(const char* file, std::string str);
	void write(LogLevel level, LogLabel label, const char* fmt, ...);

	void log_packet_buffer(const char* stub, const char* buffer, size_t length);
}
