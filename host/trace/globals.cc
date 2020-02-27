#include "globals.h"

const uint32_t FlowControlFF = 100000;
Sift::Mode current_mode = Sift::ModeIcount;
bool any_thread_in_detail = false;
std::shared_mutex control_mtx;
