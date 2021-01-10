#include <stddef.h>
#include <string.h>
#include <string>
#include <stdio.h>
#include <vector>

#include "utils.hpp"


static const char* GetLastDirSeparator(const char* filePath);
//const char* GetFileTitle(const char* filePath);
//const char* GetFileExtension(const char* filePath);
static const wchar_t* GetLastDirSeparator(const wchar_t* filePath);
//const wchar_t* GetFileTitle(const wchar_t* filePath);
//const char* GetFileExtension(const wchar_t* filePath);
//void StandardizeDirSeparators(std::string& filePath);
static bool IsAbsolutePath(const char* filePath);
//std::string CombinePaths(const std::string& basePath, const std::string& addPath);
//std::string FindFile_List(const std::vector<std::string>& fileList, const std::vector<std::string>& pathList);
//std::string FindFile_Single(const std::string& fileName, const std::vector<std::string>& pathList);
//std::vector<std::string> CombineBaseWithList(const std::string base, const std::vector<std::string>& postfixes);


static const char* GetLastDirSeparator(const char* filePath)
{
	const char* sepPos1;
	const char* sepPos2;
	
	if (strncmp(filePath, "\\\\", 2))
		filePath += 2;	// skip Windows network prefix
	sepPos1 = strrchr(filePath, '/');
	sepPos2 = strrchr(filePath, '\\');
	if (sepPos1 == NULL)
		return sepPos2;
	else if (sepPos2 == NULL)
		return sepPos1;
	else
		return (sepPos1 < sepPos2) ? sepPos2 : sepPos1;
}

const char* GetFileTitle(const char* filePath)
{
	const char* dirSepPos;
	
	dirSepPos = GetLastDirSeparator(filePath);
	return (dirSepPos != NULL) ? &dirSepPos[1] : filePath;
}

const char* GetFileExtension(const char* filePath)
{
	const char* dirSepPos;
	const char* extDotPos;
	
	dirSepPos = GetLastDirSeparator(filePath);
	if (dirSepPos == NULL)
		dirSepPos = filePath;
	extDotPos = strrchr(dirSepPos, '.');
	return (extDotPos == NULL) ? NULL : (extDotPos + 1);
}

static const wchar_t* GetLastDirSeparator(const wchar_t* filePath)
{
	const wchar_t* sepPos1;
	const wchar_t* sepPos2;
	
	if (wcsncmp(filePath, L"\\\\", 2))
		filePath += 2;	// skip Windows network prefix
	sepPos1 = wcsrchr(filePath, L'/');
	sepPos2 = wcsrchr(filePath, L'\\');
	if (sepPos1 == NULL)
		return sepPos2;
	else if (sepPos2 == NULL)
		return sepPos1;
	else
		return (sepPos1 < sepPos2) ? sepPos2 : sepPos1;
}

const wchar_t* GetFileTitle(const wchar_t* filePath)
{
	const wchar_t* dirSepPos;
	
	dirSepPos = GetLastDirSeparator(filePath);
	return (dirSepPos != NULL) ? &dirSepPos[1] : filePath;
}

const wchar_t* GetFileExtension(const wchar_t* filePath)
{
	const wchar_t* dirSepPos;
	const wchar_t* extDotPos;
	
	dirSepPos = GetLastDirSeparator(filePath);
	if (dirSepPos == NULL)
		dirSepPos = filePath;
	extDotPos = wcsrchr(dirSepPos, L'.');
	return (extDotPos == NULL) ? NULL : (extDotPos + 1);
}

void StandardizeDirSeparators(std::string& filePath)
{
	size_t curChr;
	
	curChr = 0;
	if (! filePath.compare(curChr, 2, "\\\\"))
		curChr += 2;	// skip Windows network prefix
	for (; curChr < filePath.length(); curChr ++)
	{
		if (filePath[curChr] == '\\')
			filePath[curChr] = '/';
	}
	
	return;
}

static bool IsAbsolutePath(const char* filePath)
{
	if (filePath[0] == '\0')
		return false;	// empty string
	if (filePath[0] == '/' || filePath[0] == '\\')
		return true;	// absolute UNIX path / device-relative Windows path
	if (filePath[1] == ':')
	{
		if ((filePath[0] >= 'A' && filePath[0] <= 'Z') ||
			(filePath[0] >= 'a' && filePath[0] <= 'z'))
			return true;	// Windows device path: C:\path
	}
	if (! strncmp(filePath, "\\\\", 2))
		return true;	// Windows network path: \\computername\path
	return false;
}

std::string CombinePaths(const std::string& basePath, const std::string& addPath)
{
	if (basePath.empty() || IsAbsolutePath(addPath.c_str()))
		return addPath;
	char lastChr = basePath[basePath.length() - 1];
	if (lastChr == '/' || lastChr == '\\')
		return basePath + addPath;
	else
		return basePath + '/' + addPath;
}

std::string FindFile_List(const std::vector<std::string>& fileList, const std::vector<std::string>& pathList)
{
	std::vector<std::string>::const_reverse_iterator pathIt;
	std::vector<std::string>::const_reverse_iterator fileIt;
	std::string fullName;
	FILE* hFile;
	
	for (pathIt = pathList.rbegin(); pathIt != pathList.rend(); ++pathIt)
	{
		for (fileIt = fileList.rbegin(); fileIt != fileList.rend(); ++fileIt)
		{
			fullName = CombinePaths(*pathIt, *fileIt);
			
			hFile = fopen(fullName.c_str(), "r");
			if (hFile != NULL)
			{
				fclose(hFile);
				return fullName;
			}
		}
	}
	
	return "";
}

std::string FindFile_Single(const std::string& fileName, const std::vector<std::string>& pathList)
{
	std::vector<std::string> fileList;
	
	fileList.push_back(fileName);
	return FindFile_List(fileList, pathList);
}

std::vector<std::string> CombineBaseWithList(const std::string base, const std::vector<std::string>& postfixes)
{
	std::vector<std::string> result;
	std::vector<std::string>::const_iterator pfIt;
	
	for (pfIt = postfixes.begin(); pfIt != postfixes.end(); ++pfIt)
		result.push_back(base + *pfIt);
	
	return result;
}
