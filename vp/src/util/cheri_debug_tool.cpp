#include "cheri_debug_tool.h"

namespace cheriv9::rv64 {

CheriDebugTool::CheriDebugTool() {}

CheriDebugTool::~CheriDebugTool() {}

CheriDebugTool& CheriDebugTool::instance() {
	static CheriDebugTool instance;
	return instance;
}

void CheriDebugTool::EnableCapExceptionHandling(bool enabled) {
	cap_exception_handling_enabled = enabled;
}

bool CheriDebugTool::IsCapExceptionHandlingEnabled() const {
	return cap_exception_handling_enabled;
}

}  // namespace cheriv9::rv64