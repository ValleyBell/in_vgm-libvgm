#include <wchar.h>
#include <string>
#include <vector>
#include <map>

#include <windows.h>	// for charset conversion

#include <stdtype.h>
#include <utils/DataLoader.h>
#include <utils/FileLoader.h>
#include <player/playerbase.hpp>
#include <player/vgmplayer.hpp>
#include <player/s98player.hpp>
#include <player/droplayer.hpp>
#include <player/playera.hpp>
#include <emu/SoundDevs.h>
#include <emu/SoundEmu.h>	// for SndEmu_GetDevName()

#include "playcfg.hpp"
extern PluginConfig pluginCfg;
#include "TagFormatter.hpp"
#include "FileInfoStorage.hpp"


FileInfoStorage::FileInfoStorage(PlayerA* playerPtr)
{
	if (playerPtr != NULL)
	{
		_player = playerPtr;
	}
	else
	{
		_myPlr.RegisterPlayerEngine(new VGMPlayer);
		_myPlr.RegisterPlayerEngine(new S98Player);
		_myPlr.RegisterPlayerEngine(new DROPlayer);
		_player = &_myPlr;
	}
	_dLoad = NULL;
	return;
}

FileInfoStorage::~FileInfoStorage()
{
	if (_player == &_myPlr)
	{
		if (_dLoad != NULL)
			DataLoader_Deinit(_dLoad);
	}
	return;
}

PlayerA* FileInfoStorage::GetPlayer(void)
{
	return _player;
}

void FileInfoStorage::SetFileName(const std::string& fileName)
{
	_fileNameA = fileName;
	
	int bufSize = MultiByteToWideChar(CP_ACP, 0, _fileNameA.c_str(), _fileNameA.length(), NULL, 0);
	if (bufSize <= 0)
	{
		_fileNameW.clear();
		return;
	}
	_fileNameW.resize(bufSize);
	MultiByteToWideChar(CP_ACP, 0, _fileNameA.c_str(), _fileNameA.length(), &_fileNameW[0], bufSize);
	return;
}

void FileInfoStorage::SetFileName(const std::wstring& fileName)
{
	_fileNameW = fileName;
	
	int bufSize = WideCharToMultiByte(CP_ACP, 0, _fileNameW.c_str(), _fileNameW.length(), NULL, 0, NULL, NULL);
	if (bufSize <= 0)
	{
		_fileNameA.clear();
		return;
	}
	_fileNameA.resize(bufSize);
	WideCharToMultiByte(CP_ACP, 0, _fileNameW.c_str(), _fileNameW.length(), &_fileNameA[0], bufSize, NULL, NULL);
	return;
}

UINT8 FileInfoStorage::LoadSong(const std::string& fileName, bool forceReload)
{
	if (! forceReload && fileName == _fileNameA)
	{
		// TODO: check file timestamp
		return 0x00;
	}
	
	SetFileName(fileName);
	_dLoad = FileLoader_Init(fileName.c_str());
	return LoadSongCommon();
}

UINT8 FileInfoStorage::LoadSong(const std::wstring& fileName, bool forceReload)
{
	if (! forceReload && fileName == _fileNameW)
	{
		// TODO: check file timestamp
		return 0x00;
	}
	
	SetFileName(fileName);
	_dLoad = FileLoader_InitW(fileName.c_str());
	return LoadSongCommon();
}

UINT8 FileInfoStorage::LoadSongCommon(void)
{
	UINT8 retVal;
	
	ClearSongInfo();
	
	DataLoader_SetPreloadBytes(_dLoad, 0x100);
	retVal = DataLoader_Load(_dLoad);
	if (retVal)
	{
		DataLoader_CancelLoading(_dLoad);
		DataLoader_Deinit(_dLoad);	 _dLoad = NULL;
		return 0xFF;	// file not found
	}
	retVal = _player->LoadFile(_dLoad);
	if (retVal)
	{
		DataLoader_CancelLoading(_dLoad);
		DataLoader_Deinit(_dLoad);	 _dLoad = NULL;
		return 0xFE;	// file invalid
	}
	
	ReadSongInfo();
	
	_player->UnloadFile();
	DataLoader_CancelLoading(_dLoad);
	DataLoader_Deinit(_dLoad);	 _dLoad = NULL;
	return 0x00;
}

void FileInfoStorage::ClearSongInfo(void)
{
	_songInfo.fileSize = 0;
	_songInfo.songLen1 = 0.0;
	_songInfo.songLenL = 0.0;
	_songInfo.loopLen = 0.0;
	_songInfo.chipList.clear();
	_songInfo.chipLstStr.clear();
	_songInfo.tags.clear();
	
	return;
}

void FileInfoStorage::ReadSongInfo(void)
{
	PlayerBase* filePlr = _player->GetPlayer();
	char verStr[0x20];
	
	if (filePlr == NULL)
	{
		ClearSongInfo();
		return;
	}
	
	filePlr->GetSongInfo(_songInfo.psi);
	_songInfo.fileSize = _player->GetFileSize();
	_songInfo.dataStartPos = 0x00;
	_songInfo.dataEndPos = _songInfo.fileSize;
	_songInfo.looping = (_songInfo.psi.loopTick != (UINT32)-1);
	_songInfo.hasTags = false;
	_songInfo.songLen1 = _player->GetTotalTime(0);
	_songInfo.songLenL = _player->GetTotalTime(1);
	_songInfo.loopLen = _player->GetLoopTime();
	_songInfo.volGain = 1.0;
	if (filePlr->GetPlayerType() == FCC_VGM)
	{
		VGMPlayer* vgmplay = dynamic_cast<VGMPlayer*>(filePlr);
		const VGM_HEADER* vgmhdr = vgmplay->GetFileHeader();
		
		sprintf(verStr, "VGM %X.%02X", (vgmhdr->fileVer >> 8) & 0xFF, (vgmhdr->fileVer >> 0) & 0xFF);
		_songInfo.dataStartPos = vgmhdr->dataOfs;
		_songInfo.dataEndPos = vgmhdr->dataEnd;
		_songInfo.volGain = pow(2.0, vgmhdr->volumeGain / (double)0x100);
	}
	else if (filePlr->GetPlayerType() == FCC_S98)
	{
		S98Player* s98play = dynamic_cast<S98Player*>(filePlr);
		const S98_HEADER* s98hdr = s98play->GetFileHeader();
		
		sprintf(verStr, "S98 v%u", s98hdr->fileVer);
	}
	else if (filePlr->GetPlayerType() == FCC_DRO)
	{
		DROPlayer* droplay = dynamic_cast<DROPlayer*>(filePlr);
		const DRO_HEADER* drohdr = droplay->GetFileHeader();
		
		sprintf(verStr, "DRO v%u", drohdr->verMajor);	// DRO has a "verMinor" field, but it's always 0
	}
	_songInfo.verStr = verStr;
	if (_songInfo.songLen1 <= 0.01)
	{
		_songInfo.bitRate = 0;
	}
	else
	{
		// Bit/Sec = (songBytes * 8) / songTime
		UINT32 dataSize = _songInfo.dataEndPos - _songInfo.dataStartPos;
		_songInfo.bitRate = (int)(dataSize / _songInfo.songLen1 * 8 + 0.5);
	}
	
	filePlr->GetSongDeviceInfo(_songInfo.chipList);
	GenerateChipList();
	
	_songInfo.usesYM2413 = false;
	for (size_t curDev = 0; curDev < _songInfo.chipList.size(); curDev ++)
	{
		const PLR_DEV_INFO& pdi = _songInfo.chipList[curDev];
		if (pdi.type == DEVID_YM2413)
		{
			_songInfo.usesYM2413 = true;
			break;
		}
	}
	
	EnumerateTags();
	SanitiseTags();
	
	return;
}

void FileInfoStorage::GenerateChipList(void)
{
	for (size_t curDev = 0; curDev < _songInfo.chipList.size(); curDev ++)
	{
		const PLR_DEV_INFO& pdi = _songInfo.chipList[curDev];
		const char* chipName;
		unsigned int devCnt = 1;
		
		chipName = SndEmu_GetDevName(pdi.type, 0x01, pdi.devCfg);
		for (devCnt = 1; curDev + 1 < _songInfo.chipList.size(); curDev ++, devCnt ++)
		{
			const PLR_DEV_INFO& pdi1 = _songInfo.chipList[curDev + 1];
			const char* chipName1 = SndEmu_GetDevName(pdi1.type, 0x01, pdi1.devCfg);
			bool sameChip = (chipName == chipName1);	// we assume static pointers to chip names here
			if (! sameChip)
				break;
		}
		if (pdi.type == DEVID_SN76496)
		{
			if (pdi.devCfg->flags && devCnt > 1)
				devCnt /= 2;	// the T6W28 consists of two "half" chips in VGMs
		}
		if (devCnt > 1)
		{
			char numStr[0x10];
			sprintf(numStr, "%ux", devCnt);
			_songInfo.chipLstStr.push_back(std::string(numStr) + chipName);
		}
		else
		{
			_songInfo.chipLstStr.push_back(chipName);
		}
	}
	
	return;
}

void FileInfoStorage::EnumerateTags(void)
{
	PlayerBase* filePlr = _player->GetPlayer();
	const char* const* tagList = filePlr->GetTags();
	
	_songInfo.tags.clear();
	_songInfo.hasTags = (tagList != NULL);
	if (tagList == NULL)
		return;
	
	for (const char* const* t = tagList; *t != NULL; t += 2)
	{
		if (t[1][0] == '\0')
			continue;	// skip empty tags
		_songInfo.tags[t[0]] = t[1];
	}
	
	return;
}

void FileInfoStorage::SanitiseTags(void)
{
	std::map<std::string, std::string>& tags = _songInfo.tags;
	std::map<std::string, std::string>::iterator it;
	
	if (pluginCfg.genOpts.trimWhitespace)
	{
		for (it = tags.begin(); it != tags.end(); ++it)
		{
			if (it->first == "COMMENT")
				continue;	// don't trim whitespaces in the "Notes" tag
			it->second = TrimWhitespaces(it->second);
		}
	}
	
	it = tags.find("COMMENT");
	if (it != tags.end())
		it->second = EnforceCRLF(it->second);
	
	if (pluginCfg.genOpts.stdSeparators)
	{
		for (it = _songInfo.tags.begin(); it != _songInfo.tags.end(); ++it)
		{
			if (it->first.substr(0, 6) == "ARTIST")
				/*it->second = FixSeparators(it->second)*/;
		}
	}
	
	return;
}

const char* FileInfoStorage::GetTag(const char* key) const
{
	std::map<std::string, std::string>::const_iterator it = _songInfo.tags.find(key);
	return (it == _songInfo.tags.end()) ? NULL : it->second.c_str();
}

const char* FileInfoStorage::GetTag(const char** keys) const
{
	const char** kit;
	
	for (kit = keys; *kit != NULL; kit ++)
	{
		std::map<std::string, std::string>::const_iterator vit = _songInfo.tags.find(*kit);
		if (vit != _songInfo.tags.end())
			return vit->second.c_str();
	}
	
	return NULL;
}

const char* FileInfoStorage::GetTag(const std::string& key) const
{
	std::map<std::string, std::string>::const_iterator it = _songInfo.tags.find(key);
	return (it == _songInfo.tags.end()) ? NULL : it->second.c_str();
}

const char* FileInfoStorage::GetTag(const std::vector<std::string>& keys) const
{
	std::vector<std::string>::const_iterator kit;
	
	for (kit = keys.begin(); kit != keys.end(); ++kit)
	{
		std::map<std::string, std::string>::const_iterator vit = _songInfo.tags.find(*kit);
		if (vit != _songInfo.tags.end())
			return vit->second.c_str();
	}
	
	return NULL;
}
