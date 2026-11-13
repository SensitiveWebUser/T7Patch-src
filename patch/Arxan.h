#pragma once

#include <string>

namespace arxan_bypass
{
    bool install(std::string& message);
    void maintain();
    // True while maintain() still has its one re-apply to do (the game's ready signal is not set
    // yet). The config watcher keeps calling maintain() past Unload() until this is false.
    bool maintenance_pending();
    bool active();
}
