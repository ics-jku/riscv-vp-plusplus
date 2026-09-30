#ifndef CHERI_DEBUG_TOOL_H
#define CHERI_DEBUG_TOOL_H
#include <cstdint>
#include <string>
namespace cheriv9::rv64 {
class CheriDebugTool {
   private:
	bool cap_exception_handling_enabled = true;
	CheriDebugTool();
	~CheriDebugTool();
	CheriDebugTool(const CheriDebugTool&) = delete;
	CheriDebugTool& operator=(const CheriDebugTool&) = delete;

   public:
	static CheriDebugTool& instance();
	void EnableCapExceptionHandling(bool enabled);
	bool IsCapExceptionHandlingEnabled() const;
};

}  // namespace cheriv9::rv64
#endif