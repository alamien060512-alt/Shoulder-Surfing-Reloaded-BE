#ifndef SHOULDER_CLIENT_CORE_LOGGING_H
#define SHOULDER_CLIENT_CORE_LOGGING_H

namespace shoulder {

inline constexpr const char* kLogPath = "C:\\Users\\Public\\ShoulderSurfing.log";

void LogF(const char* format, ...);

}  // namespace shoulder

#endif
