#include "DiabloUI/support_lines.h"
#include "utils/language.h"

namespace devilution {

const char *const SupportLines[] = {
	"",
	N_("GOG.com maintains a web site at https://www.gog.com/forum/diablo"),
	N_("Follow the links to visit the discussion boards associated with Diablo."),
	N_("and the Hellfire expansion."),
	"",
	N_("DevilutionX is maintained by Diasurgical, issues and bugs can be reported"),
	N_("at this address: https://github.com/diasurgical/devilutionX"),
	N_("To help us better serve you, please be sure to include the version number,"),
	N_("operating system, and the nature of the problem."),
	"",
	"",
	N_("Disclaimer:"),
	N_("  DevilutionX is not supported or maintained by Blizzard Entertainment,"),
	N_("  nor GOG.com. Neither Blizzard Entertainment or GOG.com has not tested"),
	N_("  or certified the quality or compatibility of DevilutionX. All inquiries"),
	N_("  regarding DevilutionX should be directed to Diasurgical, not to Blizzard"),
	N_("  Entertainment or GOG.com."),
	"",
	"",
};

const std::size_t SupportLinesSize = sizeof(SupportLines) / sizeof(SupportLines[0]);

} // namespace devilution
