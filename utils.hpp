#ifndef __UTILS_HPP__
#define __UTILS_HPP__

#include <vector>
#include <string>

#ifdef _WIN32
// undefine some Windows API macros
#undef GetFileTitle
#undef GetFileExtension
#endif

const char* GetFileTitle(const char* filePath);
const char* GetFileExtension(const char* filePath);
const wchar_t* GetFileTitle(const wchar_t* filePath);
const wchar_t* GetFileExtension(const wchar_t* filePath);
void StandardizeDirSeparators(std::string& filePath);
std::string CombinePaths(const std::string& basePath, const std::string& addPath);
std::string FindFile_List(const std::vector<std::string>& fileList, const std::vector<std::string>& pathList);
std::string FindFile_Single(const std::string& fileName, const std::vector<std::string>& pathList);
std::vector<std::string> CombineBaseWithList(const std::string base, const std::vector<std::string>& postfixes);

#endif	// __UTILS_HPP__
