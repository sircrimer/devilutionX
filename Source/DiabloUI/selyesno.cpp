#include "selyesno.h"

#include "DiabloUI/diabloui.h"
#include "DiabloUI/text.h"
#include "control.h"
#include "utils/language.h"

namespace devilution {
namespace {

bool selyesno_endMenu;
bool selyesno_value;
char selyesno_confirmationMessage[256];

std::vector<std::unique_ptr<UiListItem>> vecSelYesNoDialogItems;
std::vector<std::unique_ptr<UiItemBase>> vecSelYesNoDialog;

#define MESSAGE_WIDTH 280

void SelyesnoFree()
{
	ArtBackground.Unload();

	vecSelYesNoDialogItems.clear();

	vecSelYesNoDialog.clear();
}

void SelyesnoSelect(int value)
{
	selyesno_value = vecSelYesNoDialogItems[value]->m_value == 0;
	selyesno_endMenu = true;
}

void SelyesnoEsc()
{
	selyesno_value = false;
	selyesno_endMenu = true;
}

} // namespace

bool UiSelHeroYesNoDialog(const char *title, const char *body)
{
	LoadBackgroundArt("ui_art\\black.pcx");
	UiAddBackground(&vecSelYesNoDialog);
	UiAddLogo(&vecSelYesNoDialog);

	SDL_Rect rect1 = { (Sint16)(PANEL_LEFT + 24), (Sint16)(UI_OFFSET_Y + 161), 590, 35 };
	vecSelYesNoDialog.push_back(std::make_unique<UiArtText>(title, rect1, UIS_CENTER | UIS_BIG));

	SDL_Rect rect2 = { (Sint16)(PANEL_LEFT + 120), (Sint16)(UI_OFFSET_Y + 236), MESSAGE_WIDTH, 168 };
	vecSelYesNoDialog.push_back(std::make_unique<UiArtText>(selyesno_confirmationMessage, rect2, UIS_MED));

	vecSelYesNoDialogItems.push_back(std::make_unique<UiListItem>(_("Yes"), 0));
	vecSelYesNoDialogItems.push_back(std::make_unique<UiListItem>(_("No"), 1));
	vecSelYesNoDialog.push_back(std::make_unique<UiList>(vecSelYesNoDialogItems, PANEL_LEFT + 230, (UI_OFFSET_Y + 390), 180, 35, UIS_CENTER | UIS_BIG | UIS_GOLD));

	strncpy(selyesno_confirmationMessage, body, sizeof(selyesno_confirmationMessage) - 1);
	WordWrapArtStr(selyesno_confirmationMessage, MESSAGE_WIDTH);

	UiInitList(vecSelYesNoDialogItems.size(), nullptr, SelyesnoSelect, SelyesnoEsc, vecSelYesNoDialog, true, nullptr);

	selyesno_value = true;
	selyesno_endMenu = false;
	while (!selyesno_endMenu) {
		UiClearScreen();
		UiRenderItems(vecSelYesNoDialog);
		UiPollAndRender();
	}

	SelyesnoFree();

	return selyesno_value;
}
} // namespace devilution
