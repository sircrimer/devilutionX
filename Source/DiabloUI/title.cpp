#include "DiabloUI/diabloui.h"
#include "control.h"
#include "controls/menu_controls.h"
#include "utils/language.h"

namespace devilution {
namespace {

std::vector<std::unique_ptr<UiItemBase>> vecTitleScreen;

void TitleLoad()
{
	if (gbIsHellfire) {
		LoadBackgroundArt("ui_art\\hf_logo1.pcx", 16);
		LoadArt("ui_art\\hf_titlew.pcx", &ArtBackgroundWidescreen);
	} else {
		LoadBackgroundArt("ui_art\\title.pcx");
		LoadMaskedArt("ui_art\\logo.pcx", &ArtLogos[LOGO_BIG], 15);
	}
}

void TitleFree()
{
	ArtBackground.Unload();
	ArtBackgroundWidescreen.Unload();
	ArtLogos[LOGO_BIG].Unload();

	vecTitleScreen.clear();
}

} // namespace

void UiTitleDialog()
{
	if (gbIsHellfire) {
		SDL_Rect rect = { 0, UI_OFFSET_Y, 0, 0 };
		vecTitleScreen.push_back(std::make_unique<UiImage>(&ArtBackgroundWidescreen, /*bAnimated=*/true, /*iFrame=*/0, rect, UIS_CENTER));
		vecTitleScreen.push_back(std::make_unique<UiImage>(&ArtBackground, /*bAnimated=*/true, /*iFrame=*/0, rect, UIS_CENTER));
	} else {
		UiAddBackground(&vecTitleScreen);
		UiAddLogo(&vecTitleScreen, LOGO_BIG, 182);

		SDL_Rect rect = { (Sint16)(PANEL_LEFT + 49), (Sint16)(UI_OFFSET_Y + 410), 550, 26 };
		vecTitleScreen.push_back(std::make_unique<UiArtText>(_("Copyright © 1996-2001 Blizzard Entertainment"), rect, UIS_MED | UIS_CENTER));
	}
	TitleLoad();

	bool endMenu = false;
	Uint32 timeOut = SDL_GetTicks() + 7000;

	SDL_Event event;
	while (!endMenu && SDL_GetTicks() < timeOut) {
		UiRenderItems(vecTitleScreen);
		UiFadeIn();

		while (SDL_PollEvent(&event) != 0) {
			if (GetMenuAction(event) != MenuAction_NONE) {
				endMenu = true;
				break;
			}
			switch (event.type) {
			case SDL_KEYDOWN:
			case SDL_MOUSEBUTTONDOWN:
				endMenu = true;
				break;
			}
			UiHandleEvents(&event);
		}
	}

	TitleFree();
}

void UiSetSpawned(bool bSpawned)
{
	gbSpawned = bSpawned;
}

} // namespace devilution
