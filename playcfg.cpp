#include <string.h>
#include <vector>
#include <string>

#ifdef _MSC_VER
#define stricmp		_stricmp
#define strnicmp	_strnicmp
#else
#define stricmp		strcasecmp
#define strnicmp	strncasecmp
#endif

#include <stdtype.h>
#include <player/playerbase.hpp>
#include <player/vgmplayer.hpp>
#include <player/playera.hpp>
extern "C"
{
#include <emu/SoundDevs.h>
// libvgm-emu headers for configuration
#include <emu/EmuCores.h>
#include <emu/EmuStructs.h>	// for DEVRI_SRMODE_* constants
#include <emu/cores/2612intf.h>
#include <emu/cores/gb.h>
#include <emu/cores/nesintf.h>
#include <emu/cores/okim6258.h>
#include <emu/cores/scsp.h>
#include <emu/cores/c352.h>

#include "ini_func.h"
}
#include "playcfg.hpp"


struct ChipCfgSectDef
{
	UINT8 chipType;
	const char* entryName;
};

//#define BIT_MASK(startBit, bitCnt)	(((1 << bitCnt) - 1) << startBit)


static inline UINT32 MulDivRoundU32(UINT32 val, UINT32 mul, UINT32 div);
static inline UINT32 Str2FCC(const std::string& fcc);
static inline std::string FCC2Str(UINT32 fcc);
static UINT32 ReadIniDef_Integer(const char* section, const char* key, UINT32 default);
static INT16 ReadIniDef_SIntSht(const char* section, const char* key, INT16 default);
static UINT8 ReadIniDef_IntByte(const char* section, const char* key, UINT8 default);
static bool ReadIniDef_Boolean(const char* section, const char* key, bool default);
static float ReadIniDef_Float(const char* section, const char* key, float default);
UINT32 EmuTypeNum2CoreFCC(UINT8 chipType, UINT8 emuType, bool* isSubType);

void LoadConfiguration(PluginConfig& pCfg, const char* iniFileName);
static void LoadCfg_General(GeneralOptions& opts);
static void LoadCfg_ChipSection(ChipOptions& opts, const char* chipName);

void SaveConfiguration(const PluginConfig& pCfg, const char* iniFileName);
static void SaveCfg_General(const GeneralOptions& opts);
static void SaveCfg_ChipSection(const ChipOptions& opts, const char* chipName);

void ApplyCfg_General(PlayerA& player, const GeneralOptions& opts);
void ApplyCfg_Chip(PlayerA& player, const GeneralOptions& gOpts, const ChipOptions& cOpts);


// We use a hardcoded list of sound chips here, to ensure that renaming a sound chip in libvgm
// does not discard the previous configuration.
static const ChipCfgSectDef CFG_CHIP_LIST[] =
{
	{	DEVID_SN76496,	"SN76496"},
	{	DEVID_YM2413,	"YM2413"},
	{	DEVID_YM2612,	"YM2612"},
	{	DEVID_YM2151,	"YM2151"},
	{	DEVID_SEGAPCM,	"SegaPCM"},
	{	DEVID_RF5C68,	"RF5C68"},
	{	DEVID_YM2203,	"YM2203"},
	{	DEVID_YM2608,	"YM2608"},
	{	DEVID_YM2610,	"YM2610"},
	{	DEVID_YM3812,	"YM3812"},
	{	DEVID_YM3526,	"YM3526"},
	{	DEVID_Y8950,	"Y8950"},
	{	DEVID_YMF262,	"YMF262"},
	{	DEVID_YMF278B,	"YMF278B"},
	{	DEVID_YMF271,	"YMF271"},
	{	DEVID_YMZ280B,	"YMZ280B"},
	{	DEVID_32X_PWM,	"PWM"},
	{	DEVID_AY8910 ,	"AY8910"},
	{	DEVID_GB_DMG,	"GameBoy"},
	{	DEVID_NES_APU,	"NES APU"},
	{	DEVID_YMW258,	"YMW258"},
	{	DEVID_uPD7759,	"uPD7759"},
	{	DEVID_OKIM6258,	"OKIM6258"},
	{	DEVID_OKIM6295,	"OKIM6295"},
	{	DEVID_K051649,	"K051649"},
	{	DEVID_K054539,	"K054539"},
	{	DEVID_C6280,	"HuC6280"},
	{	DEVID_C140,		"C140"},
	{	DEVID_C219,		"C219"},
	{	DEVID_K053260,	"K053260"},
	{	DEVID_POKEY,	"Pokey"},
	{	DEVID_QSOUND,	"QSound"},
	{	DEVID_SCSP,		"SCSP"},
	{	DEVID_WSWAN,	"WSwan"},
	{	DEVID_VBOY_VSU,	"VSU"},
	{	DEVID_SAA1099,	"SAA1099"},
	{	DEVID_ES5503,	"ES5503"},
	{	DEVID_ES5506,	"ES5506"},
	{	DEVID_X1_010,	"X1-010"},
	{	DEVID_C352,		"C352"},
	{	DEVID_GA20,		"GA20"},
};
static const size_t CFG_CHIP_COUNT = sizeof(CFG_CHIP_LIST) / sizeof(CFG_CHIP_LIST[0]);


static inline UINT32 MulDivRoundU32(UINT32 val, UINT32 mul, UINT32 div)
{
	return (UINT32)(((UINT64)val * mul + (div / 2)) / div);
}

static inline UINT32 Str2FCC(const std::string& fcc)
{
	UINT8 buf[4];
	strncpy((char*)buf, fcc.c_str(), 4);
	return	(buf[0] << 24) | (buf[1] << 16) |
			(buf[2] <<  8) | (buf[3] <<  0);
}

static inline std::string FCC2Str(UINT32 fcc)
{
	std::string result(4, '\0');
	result[0] = (char)((fcc >> 24) & 0xFF);
	result[1] = (char)((fcc >> 16) & 0xFF);
	result[2] = (char)((fcc >>  8) & 0xFF);
	result[3] = (char)((fcc >>  0) & 0xFF);
	return result;
}

static UINT32 ReadIniDef_Integer(const char* section, const char* key, UINT32 default)
{
	ReadIni_Integer(section, key, &default);
	return default;
}

static INT16 ReadIniDef_SIntSht(const char* section, const char* key, INT16 default)
{
	ReadIni_SIntSht(section, key, &default);
	return default;
}

static UINT8 ReadIniDef_IntByte(const char* section, const char* key, UINT8 default)
{
	ReadIni_IntByte(section, key, &default);
	return default;
}

static bool ReadIniDef_Boolean(const char* section, const char* key, bool default)
{
	ReadIni_Boolean(section, key, &default);
	return default;
}

static float ReadIniDef_Float(const char* section, const char* key, float default)
{
	ReadIni_Float(section, key, &default);
	return default;
}


UINT32 EmuTypeNum2CoreFCC(UINT8 chipType, UINT8 emuType, bool* isSubType)
{
	if (isSubType != NULL)
		*isSubType = false;
	switch(chipType)
	{
	case DEVID_SN76496:
		if (emuType == 0)
			return FCC_MAME;
		else if (emuType == 1)
			return FCC_MAXM;
		break;
	case DEVID_YM2413:
		if (emuType == 0)
			return FCC_EMU_;
		else if (emuType == 1)
			return FCC_MAME;
		else if (emuType == 2)
			return FCC_NUKE;
		break;
	case DEVID_YM2612:
		if (emuType == 0)
			return FCC_GPGX;
		else if (emuType == 1)
			return FCC_NUKE;
		else if (emuType == 2)
			return FCC_GENS;
		break;
	case DEVID_YM2151:
		if (emuType == 0)
			return FCC_MAME;
		else if (emuType == 1)
			return FCC_NUKE;
		break;
	case DEVID_YM2203:
	case DEVID_YM2608:
	case DEVID_YM2610:
		if (isSubType != NULL)
			*isSubType = true;
		if (emuType == 0)
			return FCC_EMU_;
		else if (emuType == 1)
			return FCC_MAME;
		break;
	case DEVID_YM3812:
		if (emuType == 0)
			return FCC_ADLE;
		else if (emuType == 1)
			return FCC_MAME;
		else if (emuType == 2)
			return FCC_NUKE;
		break;
	case DEVID_YMF262:
		if (emuType == 0)
			return FCC_ADLE;
		else if (emuType == 1)
			return FCC_MAME;
		else if (emuType == 2)
			return FCC_NUKE;
		break;
	case DEVID_AY8910:
		if (emuType == 0)
			return FCC_EMU_;
		else if (emuType == 1)
			return FCC_MAME;
		break;
	case DEVID_NES_APU:
		if (emuType == 0)
			return FCC_NSFP;
		else if (emuType == 1)
			return FCC_MAME;
		break;
	case DEVID_C6280:
		if (emuType == 0)
			return FCC_OOTK;
		else if (emuType == 1)
			return FCC_MAME;
		break;
	case DEVID_QSOUND:
		if (emuType == 0)
			return FCC_CTR_;
		else if (emuType == 1)
			return FCC_MAME;
		break;
	case DEVID_SAA1099:
		if (emuType == 0)
			return FCC_VBEL;
		else if (emuType == 1)
			return FCC_MAME;
		break;
	}	// end switch(chipType) for emuCore
	
	return 0x00;
}

void LoadConfiguration(PluginConfig& pCfg, const char* iniFileName)
{
	size_t curChp;
	size_t curCI;	// chip instance
	
	Ini_SetFilePath(iniFileName);
	LoadCfg_General(pCfg.genOpts);
	
	pCfg.chipOpts.resize(CFG_CHIP_COUNT);
	for (curChp = 0; curChp < CFG_CHIP_COUNT; curChp ++)
	{
		const ChipCfgSectDef& cfgChip = CFG_CHIP_LIST[curChp];
		for (curCI = 0; curCI < 2; curCI ++)
		{
			ChipOptions& opts = pCfg.chipOpts[curChp][curCI];
			opts.chipType = cfgChip.chipType;
			opts.chipInstance = (UINT8)curCI;
			LoadCfg_ChipSection(opts, cfgChip.entryName);
		}
	}
	
	return;
}

static void LoadCfg_General(GeneralOptions& opts)
{
	char tempStr[0x80];
	
	opts.immediateUpdate	= ReadIniDef_Boolean("General",		"ImmdtUpdate",	false);
	opts.noInfoCache		= ReadIniDef_Boolean("General",		"NoInfoCache",	false);
	opts.fileTypes.vgm		= ReadIniDef_Boolean("General",		"FormatEnable_VGM",	true);
	opts.fileTypes.s98		= ReadIniDef_Boolean("General",		"FormatEnable_S98",	false);
	opts.fileTypes.dro		= ReadIniDef_Boolean("General",		"FormatEnable_DRO",	false);
	opts.fileTypes.gym		= ReadIniDef_Boolean("General",		"FormatEnable_GYM",	false);
	
	opts.smplRate			= ReadIniDef_Integer("Playback",	"SampleRate",	44100);
	opts.fadeTime			= ReadIniDef_Integer("Playback",	"FadeTime",		5000);
	opts.pauseTime_jingle	= ReadIniDef_Integer("Playback",	"PauseNoLoop",	1000);
	opts.pauseTime_loop		= ReadIniDef_Integer("Playback",	"PauseLoop",	0);
	opts.hardStopOld		= ReadIniDef_IntByte("Playback",	"HardStopOld",	0);
	opts.volume				= ReadIniDef_Float  ("Playback",	"Volume",		1.0);
	opts.maxLoops			= ReadIniDef_Integer("Playback",	"MaxLoops",		2);
	opts.pbRate				= ReadIniDef_Integer("Playback",	"PlaybackRate",	0);
	opts.resmplMode			= ReadIniDef_IntByte("Playback",	"ResamplMode",	0);
	opts.chipSmplMode		= ReadIniDef_IntByte("Playback",	"ChipSmplMode",	0);
	opts.chipSmplRate		= ReadIniDef_Integer("Playback",	"ChipSmplRate",	0);
	opts.pseudoSurround		= ReadIniDef_Boolean("Playback",	"SurroundSnd",	false);
	
	strcpy(tempStr, "%t (%g) - %a");
	ReadIni_String    ("Tags",		"TitleFormat",	tempStr, 0x80);	opts.titleFormat = tempStr;
	opts.preferJapTag		= ReadIniDef_Boolean("Tags",		"UseJapTags",	false);
	opts.appendFM2413		= ReadIniDef_Boolean("Tags",		"AppendFM2413",	false);
	opts.showDevCore		= ReadIniDef_Boolean("Tags",		"ShowChipCore",	false);
	opts.trimWhitespace		= ReadIniDef_Boolean("Tags",		"TrimWhitespc",	true);
	opts.stdSeparators		= ReadIniDef_Boolean("Tags",		"SeparatorStd",	true);
	opts.fidTagFallback		= ReadIniDef_Boolean("Tags",		"TagFallback",	false);
	opts.mediaLibFileType	= ReadIniDef_Integer("Tags",		"MLFileType",	0);
	
	opts.resetMuting		= ReadIniDef_Boolean("Muting",		"Reset",		true);
	
	return;
}

static void LoadCfg_ChipSection(ChipOptions& opts, const char* chipName)
{
	const UINT8 chipType = opts.chipType;
	char tempStr[0x80];
	
	// parse muting options
	memset(&opts.muteMask[0], 0x00, sizeof(opts.muteMask));
	memset(&opts.panMask[0][0], 0x00, sizeof(opts.panMask));
	
	sprintf(tempStr, "%s #%u", chipName, opts.chipInstance);
	opts.muteMask[0]	= ReadIniDef_Integer("Muting",	tempStr,	0);
	sprintf(tempStr, "%s #%u_%u", chipName, opts.chipInstance, 2);
	opts.muteMask[1]	= ReadIniDef_Integer("Muting",	tempStr,	0);
#if 0
	{
		UINT32 ayMute;
		sprintf(tempStr, "%s #%u_%u", chipName, opts.chipInstance, 3);
		ayMute				= ReadIniDef_Integer("Muting",	tempStr,	0);
		
		if (chipType == DEVID_YM2203 || chipType == DEVID_YM2608 || chipType == DEVID_YM2610)
		{
			opts.muteMask[0] |= (opts.muteMask[1] << 6);	// move ADPCM mute mask into primary array
			opts.muteMask[1] = ayMute;
		}
	}
#endif
	{
		// TODO: don't hardcode this
		opts.chnCnt[0] = 0;
		opts.chnCnt[1] = 0;
		if (chipType == DEVID_SN76496)
			opts.chnCnt[0] = 4;
		else if (chipType == DEVID_YM2413)
			opts.chnCnt[0] = 9 + 5;
		else if (chipType == DEVID_AY8910)
			opts.chnCnt[0] = 3;
		else if (chipType == DEVID_YM2203 || chipType == DEVID_YM2608 || chipType == DEVID_YM2610)
			opts.chnCnt[1] = 3;
		
		for (UINT8 subChip = 0; subChip < 2; subChip++)
		{
			int digits = (opts.chnCnt[subChip] < 10) ? 1 : 2;
			for (UINT8 curChn = 0; curChn < opts.chnCnt[subChip]; curChn ++)
			{
				if (subChip == 0)
					sprintf(tempStr, "%s #%u %0*u", chipName, opts.chipInstance, digits, curChn);
				else
					sprintf(tempStr, "%s/%u #%u %0*u", chipName, subChip, opts.chipInstance, digits, curChn);
				opts.panMask[subChip][curChn] = ReadIniDef_SIntSht("Panning",	tempStr,	0);
			}
		}
	}
	
	sprintf(tempStr, "%s #%u All", chipName, opts.chipInstance);
	bool chipMute = ReadIniDef_Boolean("Muting",	tempStr,	false);
	opts.chipDisable = chipMute ? 0xFF : 0x00;
	
	{
		UINT8 emuType = ReadIniDef_IntByte("EmuCore",	chipName,	/*0xFF*/0);
		opts.emuType = emuType;
		if (emuType != 0xFF)
		{
			// select emuCore based on number
			bool subType;
			UINT32 coreFCC = EmuTypeNum2CoreFCC(opts.chipType, emuType, &subType);
			if (subType)
				opts.emuCoreSub = coreFCC;
			else
				opts.emuCore = coreFCC;
		}
	}
	{
		std::string coreStr;
		tempStr[0] = '\0';
		ReadIni_String    ("EmuCore",		chipName,	tempStr, 0x10);	coreStr = tempStr;
		// ignore empty strings - and the ones with only 1 character (old-style number)
		if (coreStr.size() > 1)
			opts.emuCore = Str2FCC(coreStr);
		
		tempStr[0] = '\0';
		ReadIni_String    ("EmuCoreSub",	chipName,	tempStr, 0x10);	coreStr = tempStr;
		if (! coreStr.empty())
			opts.emuCoreSub = Str2FCC(coreStr);
	}
	
	// chip-specific options
	switch(chipType)
	{
	case DEVID_YM2612:
		sprintf(tempStr, "%s PseudoStereo", chipName);
		opts.addOpts  =  ReadIniDef_Integer("ChipOpts", tempStr, 0) ? OPT_YM2612_PSEUDO_STEREO : 0x00;
		sprintf(tempStr, "%s Gens DACHighpass", chipName);
		opts.addOpts |=  ReadIniDef_Integer("ChipOpts", tempStr, 0) ? OPT_YM2612_DAC_HIGHPASS : 0x00;
		sprintf(tempStr, "%s Gens SSG-EG", chipName);
		opts.addOpts |=  ReadIniDef_Integer("ChipOpts", tempStr, 0) ? OPT_YM2612_SSGEG : 0x00;
		sprintf(tempStr, "%s NukedType", chipName);
		opts.addOpts |= (ReadIniDef_Integer("ChipOpts", tempStr, 0) & 0x03) << 4;
		break;
	case DEVID_YM2203:
	case DEVID_YM2608:
	case DEVID_YM2610:
		//sprintf(tempStr, "%s Disable AY", chipName);
		//opts.chipDisable |= ReadIniDef_Integer("ChipOpts", tempStr, 0) ? 0x02 : 0x00;
		break;
	case DEVID_YMF278B:
		//sprintf(tempStr, "%s Disable FM", chipName);
		//opts.chipDisable |= ReadIniDef_Integer("ChipOpts", tempStr, 0) ? 0x02 : 0x00;
		break;
	case DEVID_GB_DMG:
		sprintf(tempStr, "%s BoostWaveChn", chipName);
		opts.addOpts  =  ReadIniDef_Integer("ChipOpts", tempStr, 1) ? OPT_GB_DMG_BOOST_WAVECH : 0x00;
		break;
	case DEVID_NES_APU:
		sprintf(tempStr, "%s Shared Opts", chipName);
		opts.addOpts  = (ReadIniDef_Integer("ChipOpts", tempStr, 0x03) & 0x03) << 0;
		sprintf(tempStr, "%s APU Opts", chipName);
		opts.addOpts |= (ReadIniDef_Integer("ChipOpts", tempStr, 0x01) & 0x01) << 2;
		sprintf(tempStr, "%s DMC Opts", chipName);
		opts.addOpts |= (ReadIniDef_Integer("ChipOpts", tempStr, 0x3B) & 0x3F) << 4;
		sprintf(tempStr, "%s FDS Opts", chipName);
		opts.addOpts |= (ReadIniDef_Integer("ChipOpts", tempStr, 0x00) & 0x03) << 10;
		break;
	case DEVID_OKIM6258:
		sprintf(tempStr, "%s Internal 10bit", chipName);
		opts.addOpts = ReadIniDef_Integer("ChipOpts", tempStr, 0) ? OPT_OKIM6258_FORCE_12BIT : 0x00;
		break;
	case DEVID_SCSP:
		sprintf(tempStr, "%s Bypass DSP", chipName);
		opts.addOpts = ReadIniDef_Integer("ChipOpts", tempStr, 1) ? OPT_SCSP_BYPASS_DSP : 0x00;
		break;
	case DEVID_C352:
		sprintf(tempStr, "%s Disable Rear", chipName);
		opts.addOpts = ReadIniDef_Integer("ChipOpts", tempStr, 0) ? OPT_C352_MUTE_REAR : 0x00;
		break;
	}	// end switch(chipType) for chip-specific options
	
	return;
}


void SaveConfiguration(const PluginConfig& pCfg, const char* iniFileName)
{
	size_t curChp;
	size_t curCI;	// chip instance
	
	Ini_SetFilePath(iniFileName);
	SaveCfg_General(pCfg.genOpts);
	
	for (curChp = 0; curChp < pCfg.chipOpts.size(); curChp ++)
	{
		const ChipCfgSectDef& cfgChip = CFG_CHIP_LIST[curChp];
		for (curCI = 0; curCI < 2; curCI ++)
		{
			const ChipOptions& opts = pCfg.chipOpts[curChp][curCI];
			if (opts.chipType == 0xFF)
				continue;
			SaveCfg_ChipSection(opts, cfgChip.entryName);
		}
	}
	
	return;
}

static void SaveCfg_General(const GeneralOptions& opts)
{
	WriteIni_Boolean("General",		"ImmdtUpdate",	opts.immediateUpdate);
	WriteIni_Boolean("General",		"NoInfoCache",	opts.noInfoCache);
	WriteIni_Boolean("General",		"FormatEnable_VGM",	opts.fileTypes.vgm);
	WriteIni_Boolean("General",		"FormatEnable_S98",	opts.fileTypes.s98);
	WriteIni_Boolean("General",		"FormatEnable_DRO",	opts.fileTypes.dro);
	WriteIni_Boolean("General",		"FormatEnable_GYM",	opts.fileTypes.gym);
	
	WriteIni_Integer("Playback",	"SampleRate",	opts.smplRate);
	WriteIni_Integer("Playback",	"FadeTime",		opts.fadeTime);
	WriteIni_Integer("Playback",	"PauseNoLoop",	opts.pauseTime_jingle);
	WriteIni_Integer("Playback",	"PauseLoop",	opts.pauseTime_loop);
	WriteIni_Integer("Playback",	"HardStopOld",	opts.hardStopOld);
	WriteIni_Float  ("Playback",	"Volume",		(float)opts.volume);
	WriteIni_Integer("Playback",	"MaxLoops",		opts.maxLoops);
	WriteIni_Integer("Playback",	"PlaybackRate",	opts.pbRate);
	WriteIni_Integer("Playback",	"ResamplMode",	opts.resmplMode);
	WriteIni_Integer("Playback",	"ChipSmplMode",	opts.chipSmplMode);
	WriteIni_Integer("Playback",	"ChipSmplRate",	opts.chipSmplRate);
	WriteIni_Boolean("Playback",	"SurroundSnd",	opts.pseudoSurround);
	
	WriteIni_String ("Tags",		"TitleFormat",	opts.titleFormat.c_str());
	WriteIni_Boolean("Tags",		"UseJapTags",	opts.preferJapTag);
	WriteIni_Boolean("Tags",		"AppendFM2413",	opts.appendFM2413);
	WriteIni_Boolean("Tags",		"ShowChipCore",	opts.showDevCore);
	WriteIni_Boolean("Tags",		"TrimWhitespc",	opts.trimWhitespace);
	WriteIni_Boolean("Tags",		"SeparatorStd",	opts.stdSeparators);
	WriteIni_Boolean("Tags",		"TagFallback",	opts.fidTagFallback);
	WriteIni_XInteger("Tags",		"MLFileType",	opts.mediaLibFileType);
	
	WriteIni_Boolean("Muting",		"Reset",		opts.resetMuting);
	
	return;
}

static void SaveCfg_ChipSection(const ChipOptions& opts, const char* chipName)
{
	const UINT8 chipType = opts.chipType;
	char tempStr[0x80];
	
	WriteIni_Integer("EmuCore",	chipName,	opts.emuType);
	//WriteIni_String ("EmuCore",		chipName,	FCC2Str(opts.emuCore).c_str());
	//WriteIni_String ("EmuCoreSub",	chipName,	FCC2Str(opts.emuCoreSub).c_str());
	
	sprintf(tempStr, "%s #%u All", chipName, opts.chipInstance);
	WriteIni_Boolean("Muting",	tempStr,	!!opts.chipDisable);
	
	sprintf(tempStr, "%s #%u", chipName, opts.chipInstance);
	WriteIni_XInteger("Muting",	tempStr,	opts.muteMask[0]);
	if (chipType == DEVID_YM2203 || chipType == DEVID_YM2608 || chipType == DEVID_YM2610 ||
		chipType == DEVID_YMF278B)
	{
		sprintf(tempStr, "%s #%u_%u", chipName, opts.chipInstance, 2);
		WriteIni_XInteger("Muting",	tempStr,	opts.muteMask[1]);
	}
	
	{
		for (UINT8 subChip = 0; subChip < 2; subChip++)
		{
			int digits = (opts.chnCnt[subChip] < 10) ? 1 : 2;
			for (UINT8 curChn = 0; curChn < opts.chnCnt[subChip]; curChn ++)
			{
				if (subChip == 0)
					sprintf(tempStr, "%s #%u %0*u", chipName, opts.chipInstance, digits, curChn);
				else
					sprintf(tempStr, "%s/%u #%u %0*u", chipName, subChip, opts.chipInstance, digits, curChn);
				WriteIni_SInteger("Panning",	tempStr,	opts.panMask[subChip][curChn]);
			}
		}
	}
	
	if (opts.chipInstance > 0)
		return;	// the remaining options are global
	
	// chip-specific options
	switch(chipType)
	{
	case DEVID_YM2612:
		sprintf(tempStr, "%s PseudoStereo", chipName);
		WriteIni_XInteger("ChipOpts", tempStr, !!(opts.addOpts & OPT_YM2612_PSEUDO_STEREO));
		sprintf(tempStr, "%s Gens DACHighpass", chipName);
		WriteIni_XInteger("ChipOpts", tempStr, !!(opts.addOpts & OPT_YM2612_DAC_HIGHPASS));
		sprintf(tempStr, "%s Gens SSG-EG", chipName);
		WriteIni_XInteger("ChipOpts", tempStr, !!(opts.addOpts & OPT_YM2612_SSGEG));
		sprintf(tempStr, "%s NukedType", chipName);
		WriteIni_XInteger("ChipOpts", tempStr, (opts.addOpts >> 4) & 0x03);
		break;
	case DEVID_YM2203:
	case DEVID_YM2608:
	case DEVID_YM2610:
		//sprintf(tempStr, "%s Disable AY", chipName);
		//WriteIni_XInteger("ChipOpts", tempStr, !!(opts.chipDisable & 0x02));
		break;
	case DEVID_YMF278B:
		//sprintf(tempStr, "%s Disable FM", chipName);
		//WriteIni_XInteger("ChipOpts", tempStr, !!(opts.chipDisable & 0x02));
		break;
	case DEVID_GB_DMG:
		sprintf(tempStr, "%s BoostWaveChn", chipName);
		WriteIni_XInteger("ChipOpts", tempStr, !!(opts.addOpts & OPT_GB_DMG_BOOST_WAVECH));
		break;
	case DEVID_NES_APU:
		sprintf(tempStr, "%s Shared Opts", chipName);
		WriteIni_XInteger("ChipOpts", tempStr, (opts.addOpts >> 0) & 0x03);
		sprintf(tempStr, "%s APU Opts", chipName);
		WriteIni_XInteger("ChipOpts", tempStr, (opts.addOpts >> 2) & 0x01);
		sprintf(tempStr, "%s DMC Opts", chipName);
		WriteIni_XInteger("ChipOpts", tempStr, (opts.addOpts >> 4) & 0x3F);
		sprintf(tempStr, "%s FDS Opts", chipName);
		WriteIni_XInteger("ChipOpts", tempStr, (opts.addOpts >> 10) & 0x03);
		break;
	case DEVID_OKIM6258:
		sprintf(tempStr, "%s Internal 10bit", chipName);
		WriteIni_XInteger("ChipOpts", tempStr, !!(opts.addOpts & OPT_OKIM6258_FORCE_12BIT));
		break;
	case DEVID_SCSP:
		sprintf(tempStr, "%s Bypass DSP", chipName);
		WriteIni_XInteger("ChipOpts", tempStr, !!(opts.addOpts & OPT_SCSP_BYPASS_DSP));
		break;
	case DEVID_C352:
		sprintf(tempStr, "%s Disable Rear", chipName);
		WriteIni_XInteger("ChipOpts", tempStr, !!(opts.addOpts & OPT_C352_MUTE_REAR));
		break;
	}	// end switch(chipType) for chip-specific options
	
	return;
}


void ApplyCfg_General(PlayerA& player, const GeneralOptions& opts, bool noLiveOpts)
{
	const std::vector<PlayerBase*>& plrs = player.GetRegisteredPlayers();
	size_t curPlr;
	UINT8 retVal;
	PlayerA::Config pwCfg;
	
	pwCfg = player.GetConfiguration();
	pwCfg.chnInvert = opts.pseudoSurround ? 0x02 : 0x00;
	pwCfg.loopCount = opts.maxLoops;
	if (! noLiveOpts)
	{
		pwCfg.masterVol = (INT32)(0x10000 * opts.volume + 0.5);
		pwCfg.fadeSmpls = MulDivRoundU32(opts.fadeTime, opts.smplRate, 1000);
		pwCfg.endSilenceSmpls = MulDivRoundU32(opts.pauseTime_jingle, opts.smplRate, 1000);
	}
	pwCfg.pbSpeed = 1.0;
	player.SetConfiguration(pwCfg);
	
	for (curPlr = 0; curPlr < plrs.size(); curPlr ++)
	{
		PlayerBase* pBase = plrs[curPlr];
		switch(pBase->GetPlayerType())
		{
		case FCC_VGM:
		{
			VGMPlayer* vgmplay = dynamic_cast<VGMPlayer*>(pBase);
			VGM_PLAY_OPTIONS vpOpts;
			retVal = vgmplay->GetPlayerOptions(vpOpts);
			if (retVal)
				break;
			
			vpOpts.playbackHz = opts.pbRate;
			vpOpts.hardStopOld = opts.hardStopOld;
			vgmplay->SetPlayerOptions(vpOpts);
		}
			break;
		}
	}
	
	return;
}

static UINT8 ConvertChipSmplModeOption(UINT8 devID, UINT8 option)
{
	switch(option)
	{
	case 0x00:	// native
		return DEVRI_SRMODE_NATIVE;
	case 0x01:	// highest sampling rate (native/custom)
		return DEVRI_SRMODE_HIGHEST;
	case 0x02:	// custom sample rate
		return DEVRI_SRMODE_CUSTOM;
	case 0x03:	// native for FM, highest for others
		{
			bool isFM = (devID == DEVID_YM3526 || devID == DEVID_Y8950 || devID == DEVID_YM3812 ||
				devID == DEVID_YM2413 || devID == DEVID_YMF262 || devID == DEVID_YM2151 ||
				devID == DEVID_YM2203 || devID == DEVID_YM2608 || devID == DEVID_YM2610 || devID == DEVID_YM2612);
			return isFM ? DEVRI_SRMODE_NATIVE : DEVRI_SRMODE_HIGHEST;
		}
	default:
		return 0x00;
	}
}

void ApplyCfg_Chip(PlayerA& player, const GeneralOptions& gOpts, const ChipOptions& cOpts)
{
	const std::vector<PlayerBase*>& plrs = player.GetRegisteredPlayers();
	size_t curPlr;
	UINT8 retVal;
	UINT32 devID;
	
	if (cOpts.chipInstance != 0xFF)
		devID = PLR_DEV_ID(cOpts.chipType, cOpts.chipInstance);
	else
		devID = PLR_DEV_ID(cOpts.chipType, 0x00);
	for (curPlr = 0; curPlr < plrs.size(); curPlr ++)
	{
		PlayerBase* pBase = plrs[curPlr];
		PLR_DEV_OPTS devOpts;
		UINT8 curInst;
		UINT8 curChn;
		
		retVal = pBase->GetDeviceOptions(devID, devOpts);
		if (retVal)
			continue;	// this player doesn't support this chip
		
		devOpts.emuCore[0] = cOpts.emuCore;
		devOpts.emuCore[1] = cOpts.emuCoreSub;
		devOpts.srMode = ConvertChipSmplModeOption(cOpts.chipType, gOpts.chipSmplMode);
		devOpts.resmplMode = gOpts.resmplMode;
		devOpts.smplRate = gOpts.chipSmplRate;
		devOpts.coreOpts = cOpts.addOpts;
		devOpts.muteOpts.disable = cOpts.chipDisable;
		for (curInst = 0; curInst < 2; curInst ++)
		{
			devOpts.muteOpts.chnMute[curInst] = cOpts.muteMask[curInst];
			for (curChn = 0; curChn < 32; curChn ++)
				devOpts.panOpts.chnPan[curInst][curChn] = cOpts.panMask[curInst][curChn];
		}
		//printf("Player %s: Setting chip options for chip 0x%02X, instance 0x%02X\n",
		//		pBase->GetPlayerName(), cOpts.chipType, cOpts.chipInstance);
		if (cOpts.chipInstance != 0xFF)
		{
			pBase->SetDeviceOptions(devID, devOpts);
		}
		else
		{
			for (curInst = 0; curInst < 2; curInst ++)
				pBase->SetDeviceOptions(PLR_DEV_ID(cOpts.chipType, curInst), devOpts);
		}
	}
	
	return;
}
