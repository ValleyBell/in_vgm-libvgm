#ifndef __PLAYCFG_HPP__
#define __PLAYCFG_HPP__

#include <stdtype.h>
#include <string>
#include <vector>
#include <array>

struct GeneralOptions
{
	UINT32 smplRate;
	UINT8 smplBits;
	UINT32 pbRate;
	double volume;
	double pbSpeed;
	UINT32 maxLoops;
	
	UINT8 resmplMode;
	UINT8 chipSmplMode;
	UINT32 chipSmplRate;
	
	UINT32 fadeTime;
	UINT32 pauseTime_jingle;
	UINT32 pauseTime_loop;
	
	bool pseudoSurround;
	bool preferJapTag;
	bool showDevCore;
	UINT8 hardStopOld;
	
	bool immediateUpdate;
	bool resetMuting;
	bool noInfoCache;
	std::string titleFormat;
	bool appendFM2413;
	bool trimWhitespace;
	bool stdSeparators;
	bool fidTagFallback;	// Jap/Eng tag fallback for file info dialogue
	UINT32 mediaLibFileType;
	struct
	{
		bool vgm;
		bool s98;
		bool dro;
		bool gym;
	} fileTypes;
};
struct ChipOptions
{
	const char* cfgEntryName;
	UINT8 chipType;
	UINT8 chipInstance;
	UINT8 chipDisable;	// bit mask, bit 0 (01) = main, bit 1 (02) = linked
	UINT32 emuCore;
	UINT32 emuCoreSub;
	UINT8 emuType;		// TODO: remove and use emuCore/emuCoreSub instead
	UINT8 chnCnt[2];	// for now this is only used by panning code
	UINT32 muteMask[2];
	INT16 panMask[2][32];	// scale: -0x100 .. 0 .. +0x100
	UINT32 addOpts;
};
struct PluginConfig
{
	GeneralOptions genOpts;
	std::vector< std::array<ChipOptions, 2> > chipOpts;
};

class PlayerA;


UINT32 EmuTypeNum2CoreFCC(UINT8 chipType, UINT8 emuType, bool* isSubType);

void LoadConfiguration(PluginConfig& pCfg, const char* iniFileName);
void SaveConfiguration(const PluginConfig& pCfg, const char* iniFileName);

void ApplyCfg_General(PlayerA& player, const GeneralOptions& opts, bool noLiveOpts = false);
void ApplyCfg_Chip(PlayerA& player, const GeneralOptions& gOpts, const ChipOptions& cOpts);

#endif	// __PLAYCFG_HPP__
