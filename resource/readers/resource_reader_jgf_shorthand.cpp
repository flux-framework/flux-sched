extern "C" {
#if HAVE_CONFIG_H
#include "config.h"
#endif
#include <flux/idset.h>
}

#include <map>
#include <unordered_set>
#include <unistd.h>
#include <regex>
#include <jansson.h>
#include "resource/readers/resource_reader_jgf_shorthand.hpp"
#include "resource/store/resource_graph_store.hpp"
#include "resource/planner/c/planner.h"

using namespace Flux;
using namespace Flux::resource_model;

resource_reader_jgf_shorthand_t::~resource_reader_jgf_shorthand_t ()
{
}
