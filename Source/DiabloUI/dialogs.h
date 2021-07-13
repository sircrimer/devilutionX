#pragma once

#include <cstddef>

#include "DiabloUI/ui_item.h"

namespace devilution {

void UiErrorOkDialog(const char *text, const std::vector<std::unique_ptr<UiItemBase>> &renderBehind);
void UiErrorOkDialog(const char *text, const char *caption, const std::vector<std::unique_ptr<UiItemBase>> &renderBehind);
void UiOkDialog(const char *text, const char *caption, bool error, const std::vector<std::unique_ptr<UiItemBase>> &renderBehind);

} // namespace devilution
