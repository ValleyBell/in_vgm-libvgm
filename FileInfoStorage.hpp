#ifndef __FILEINFOSTORAGE_HPP__
#define __FILEINFOSTORAGE_HPP__

#include <wchar.h>
#include <string>
#include <vector>
#include <map>

#include <stdtype.h>
#include <utils/DataLoader.h>
#include <player/playerbase.hpp>	// for PLR_DEV_INFO
#include <player/playera.hpp>


class FileInfoStorage
{
public:
	FileInfoStorage(PlayerA* playerPtr);
	~FileInfoStorage();
	PlayerA* GetPlayer(void);
	UINT8 LoadSong(const std::string& fileName, bool forceReload = false);
	UINT8 LoadSong(const std::wstring& fileName, bool forceReload = false);
	void SetFileName(const std::string& fileName);	// for use with "external" _player
	void SetFileName(const std::wstring& fileName);	// for use with "external" _player
	void ClearSongInfo(void);
	void ReadSongInfo(void);	// read song info from _player
	const char* GetTag(const char* key) const;
	const char* GetTag(const char** keys) const;
	const char* GetTag(const std::string& key) const;
	const char* GetTag(const std::vector<std::string>& keys) const;
private:
	UINT8 LoadSongCommon(void);
	void GenerateChipList(void);
	void EnumerateTags(void);
	void SanitiseTags(void);
	
public:
	std::string _fileNameU;	// currently playing file [UTF-8] (used for getting info on the current file)
	std::string _fileNameA;	// file name in current ANSI codepage
	std::wstring _fileNameW;	// file name as UTF-16
	PlayerA* _player;		// either point to external PlayerA (-> playing file) or _myPlr (-> file scanning)
	PlayerA _myPlr;
	DATA_LOADER* _dLoad;
	
	// file info
	struct SongInfo
	{
		UINT32 fileSize;
		UINT32 dataStartPos;
		UINT32 dataEndPos;
		bool looping;
		bool hasTags;
		bool usesYM2413;
		double songLen1;	// length for playing once
		double songLenL;	// length with all loops
		double loopLen;	// loop length
		double bitRate;
		double volGain;
		
		PLR_SONG_INFO psi;
		std::string verStr;
		std::vector<PLR_DEV_INFO> chipList;
		std::vector<std::string> chipLstStr;
		std::map<std::string, std::string> tags;
	} _songInfo;
};

#endif	// __FILEINFOSTORAGE_HPP__
