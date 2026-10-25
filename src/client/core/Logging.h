#ifndef SHOULDER_CLIENT_CORE_LOGGING_H
#define SHOULDER_CLIENT_CORE_LOGGING_H

namespace shoulder {

// Writes to %TEMP%\\shoulder-surfing-reloaded.log
void LogF(const char* format, ...);

}  // namespace shoulder

#endif
