#ifndef __TAGFORMATTER__
#define __TAGFORMATTER__

class FileInfoStorage;

std::string FormatVGMTag(const char* format, const FileInfoStorage& fis);
std::string TrimWhitespaces(const std::string& text);
std::string EnforceCRLF(const std::string& text);

#endif	// __TAGFORMATTER__
