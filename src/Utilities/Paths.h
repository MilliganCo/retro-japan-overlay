#pragma once

#include <string>

namespace rjo {

std::wstring GetAppDataRoot();
std::wstring GetConfigPath();
std::wstring GetLogDirectory();
std::wstring GetDefaultConfigTemplatePath();
bool EnsureAppDirectories();

} // namespace rjo
