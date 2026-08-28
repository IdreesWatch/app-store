#include <stdio.h>

#include "m_argv.h"

#include "doomgeneric.h"

#ifdef ESP_PLATFORM
#include "esp_heap_caps.h"
#endif

pixel_t* DG_ScreenBuffer = NULL;

void M_FindResponseFile(void);
void D_DoomMain (void);


void doomgeneric_Create(int argc, char **argv)
{
    myargc = argc;
    myargv = argv;

	M_FindResponseFile();

#ifdef ESP_PLATFORM
	/* A relaunch must not retain the previous 320x200 PSRAM framebuffer. */
	if (DG_ScreenBuffer) {
		heap_caps_free(DG_ScreenBuffer);
		DG_ScreenBuffer = NULL;
	}
#endif

#ifdef ESP_PLATFORM
	// Use PSRAM on ESP32 - this buffer is DOOMGENERIC_RESX*DOOMGENERIC_RESY*4 bytes
	DG_ScreenBuffer = heap_caps_malloc(DOOMGENERIC_RESX * DOOMGENERIC_RESY * 4, MALLOC_CAP_SPIRAM);
	if (!DG_ScreenBuffer) {
		// Fallback to internal RAM if PSRAM fails
		DG_ScreenBuffer = heap_caps_malloc(DOOMGENERIC_RESX * DOOMGENERIC_RESY * 4, MALLOC_CAP_INTERNAL);
	}
#else
	DG_ScreenBuffer = malloc(DOOMGENERIC_RESX * DOOMGENERIC_RESY * 4);
#endif

	if (!DG_ScreenBuffer) {
		printf("FATAL: Failed to allocate DG_ScreenBuffer!\n");
		return;
	}

	DG_Init();

#ifdef ESP_PLATFORM
	void stratus_init_doom_info(void);
    stratus_init_doom_info();
#endif

	D_DoomMain ();
}

void doomgeneric_FreeScreenBuffer(void)
{
#ifdef ESP_PLATFORM
	if (DG_ScreenBuffer) {
		heap_caps_free(DG_ScreenBuffer);
		DG_ScreenBuffer = NULL;
	}
#else
	free(DG_ScreenBuffer);
	DG_ScreenBuffer = NULL;
#endif
}

