#ifndef SYS_SAGE
#define SYS_SAGE

//includes all other headers
#include "Topology.hpp"
#include "DataPath.hpp"
#include "xml_dump.hpp"
#include "parsers/hwloc.hpp"
#include "parsers/caps-numa-benchmark.hpp"
#include "parsers/gpu-topo.hpp"
#include "parsers/cccbench.hpp"
#ifdef PAPI_METRICS
#include "papi/Metrics.hpp"
#endif

#endif //SYS_SAGE
