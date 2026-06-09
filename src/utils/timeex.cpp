#include "timeex.hpp"

namespace cpp_streamer
{
	static int64_t s_now_ms = 0;

	void UpdateNowMilliSec(int64_t now_ms) {
		s_now_ms = now_ms;
	}
	int64_t GetNowMilliSec() {
		if (s_now_ms <= 0) {
			return now_millisec();
		}
		return s_now_ms;
	}

	std::string get_now_str_for_filename() {
		char dscr_sz[80] = {0};
#ifdef _WIN32
		SYSTEMTIME st;
		GetLocalTime(&st);
		sprintf_s(dscr_sz, sizeof(dscr_sz),
			"%04d.%02d.%02d.%02d.%02d.%02d",
			st.wYear,
			st.wMonth,
			st.wDay,
			st.wHour,
			st.wMinute,
			st.wSecond
		);
#else
		std::time_t t = std::time(nullptr);
		auto tm = std::localtime(&t);
		sprintf(dscr_sz, "%04d.%02d.%02d.%02d.%02d.%02d",
			tm->tm_year+1900, tm->tm_mon + 1, tm->tm_mday, 
			tm->tm_hour, tm->tm_min, tm->tm_sec);
#endif
		return std::string(dscr_sz);
	}
}