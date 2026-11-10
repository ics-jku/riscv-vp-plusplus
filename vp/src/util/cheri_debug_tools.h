#ifndef CHERI_DEBUG_TOOLS_H
#define CHERI_DEBUG_TOOLS_H
#include <cstdint>
#include <string>
namespace rv64y {
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

}  // namespace rv64y
#endif