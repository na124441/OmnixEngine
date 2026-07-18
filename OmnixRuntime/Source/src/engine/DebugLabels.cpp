#include "src/engine/DebugLabels.h"

PFN_vkCmdBeginDebugUtilsLabelEXT  DebugLabel::vkCmdBeginDebugUtilsLabelEXT = nullptr;
PFN_vkCmdEndDebugUtilsLabelEXT    DebugLabel::vkCmdEndDebugUtilsLabelEXT   = nullptr;
PFN_vkCmdInsertDebugUtilsLabelEXT DebugLabel::vkCmdInsertDebugUtilsLabelEXT = nullptr;
bool DebugLabel::enabled = false;
