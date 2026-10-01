#include "../../src/events/mod_event.h"
#include <cassert>
int main()
{
	using namespace skyshim;
	ModEventRegistry r;
	r.Register("SKICP_modSelected", 1, "OnModSelect");
	r.Register("SKICP_modSelected", 1, "OnModSelect2");  // replaces
	r.Register("SKICP_modSelected", 2, "OnModSelect");
	assert(r.Snapshot("SKICP_modSelected").size() == 2);
	assert(r.Snapshot("SKICP_modSelected")[0].callback == "OnModSelect2");
	r.Unregister("SKICP_modSelected", 1);
	assert(r.Snapshot("SKICP_modSelected").size() == 1);
	assert(r.Snapshot("none").empty());
	IndexStore s; s.Store("a", { 1, 2 });
	assert(s.Load("a").size() == 2 && s.Load("b").empty());
}
