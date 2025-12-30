#ifndef STRING_UTILS_H
#define STRING_UTILS_H

#include <cstdio>
#include <string>

#include <WString.h>  // Arduino String

// Safe string copy helper - prevents buffer overflow and always null-terminates
// Uses snprintf which is safer than strncpy (always null-terminates)
template <size_t N> void safeCopyString(char (&dest)[N], const char* src) {
  snprintf(dest, N, "%s", src);
}

template <size_t N> void safeCopyString(char (&dest)[N], const std::string& src) {
  snprintf(dest, N, "%s", src.c_str());
}

// Overload for Arduino String (for compatibility, but prefer std::string)
template <size_t N> void safeCopyString(char (&dest)[N], const String& src) {
  snprintf(dest, N, "%s", src.c_str());
}

#endif  // STRING_UTILS_H
