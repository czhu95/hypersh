#include "globals.h"

const uint32_t FlowControlFF = 100000;
const uint32_t FlowControl = 50;
Sift::Mode current_mode = Sift::ModeIcount;
bool any_thread_in_detail = false;
std::shared_mutex control_mtx;
uint64_t roi_cr3 = 0UL;
std::vector<std::unordered_map<uint64_t, uint64_t>> block_cnt;
