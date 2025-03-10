#pragma once

#include <cache_config.hpp>

#if USE_TINY_LFU_FLAG

#include <small_page_advanced.hpp>
namespace cache { using SmallPage = SmallPageAdvanced; }

#else

#include <small_page_basic.hpp>
namespace cache { using SmallPage = SmallPageBasic; }

#endif
