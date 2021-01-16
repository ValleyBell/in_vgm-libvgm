#if defined(APLHA)

#define VER_EXTRA	" alpha"
#define VER_DATE	" ("__DATE__" "__TIME__")"

#elif defined(BETA)

#define VER_EXTRA	" beta"
#define VER_DATE	" ("__DATE__" "__TIME__")"

#else

#define VER_EXTRA	""
#define VER_DATE	""

#endif

#define VGMPLAY_VER_STR	"0.50.1"
#define VGM_VER_STR		"1.71b"
#define INVGM_VERSION		VGMPLAY_VER_STR VER_EXTRA
#define INVGM_TITLE			"VGM Input Plugin v" INVGM_VERSION
#define INVGM_TITLE_FULL	"VGM Input Plugin v" INVGM_VERSION VER_DATE
