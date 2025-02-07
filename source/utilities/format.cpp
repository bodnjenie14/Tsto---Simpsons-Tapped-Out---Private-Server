#include "format.hpp"

#define M_SECONDS 60
#define H_SECONDS 3600
#define D_SECONDS 86400

namespace utils::format
{
	const char* get_data_size_str(size_t bytes)
	{
		if (bytes < 1000) {
			return string::va("%d B", bytes);
		}
		else if (bytes < 1000000) {
			auto kb = bytes / 1000;
			return string::va("%d KB", kb);
		}
		else {
			auto mb = static_cast<double>(bytes) / 1000000;
			return string::va("%.2f MB", mb);
		}
	}

	const char* build_timelapse_str(uint32_t seconds)
	{
		int days = seconds / 60 / 60 / 24;
		int hours = (seconds / 60 / 60) % 24;
		int minutes = (seconds / 60) % 60;
		seconds = seconds % 60;

		std::string result;
		if (days) {
			return string::va("%dD %dH %dM %dS", days, hours, minutes, seconds);
		}
		else if (hours) {
			return string::va("%dH %dM %dS", hours, minutes, seconds);
		}
		else if (minutes) {
			return string::va("%dM %dS", minutes, seconds);
		}
		else {
			return string::va("%dS", seconds);
		}
	}

	const char* format_timelapse_informal(unsigned int seconds)
	{
		const char* result = "N/A";

		int days = seconds / 60 / 60 / 24;
    	int hours = (seconds / 60 / 60) % 24;
    	int minutes = (seconds / 60) % 60;

		if (seconds < M_SECONDS) {
			result = string::va("%d seconds ago", seconds);
		}
		else if (seconds < H_SECONDS) {
			result = (minutes > 1) 
				? string::va("%d minutes ago", minutes) : "last minute";
		}
		else if (seconds < D_SECONDS) {
			result = (hours > 1)
				? string::va("%d hours ago", hours) : "last hour";
		}
		else {
			result = (days > 1)
				? string::va("%d days ago", days) : "last day";
		}

		return result;
	}
}
