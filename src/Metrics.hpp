#ifndef METRICS
#define METRICS

#include "defines.hpp"

#ifdef PAPI_METRICS

#include <functional>
#include <vector>

#include "Topology.hpp"

/**
 * PAPI_METRICS Add-On in Sys-Sage
 * Concept:
 * PAPI can be used to observe various characterestics of
 * a complex computing environment. Sys-Sage can be used to
 * describe the topology and data paths of such systems.
 * This add-on allows binding PAPI Metrics to
 * arbitrary components of a sys-sage topology in order to use
 * it for system tuning and application profiling.
 * 
 * The flexible 'attrib' extension of Components are used
 * to keep track of associated PAPI event counters. It can be used
 * to store event counters received from PAPI by calling functions
 * PAPI_stop, PAPI_read or PAPI_accum.
 * The counter readings are stored together with a relative timestamp
 * and the number of the actual core, where the eventset is executed.
 * If the core is irrelevant for the eventset, like uncore events,
 * then the core number will be ignored (-1).
 * 
 * It also allows exporting the Metrics in XML format, together
 * with the PAPI Events. IN this case additional method calls
 * are required to set up the event information.
 * 
 * Automatic Component binding can be established per Event,
 * based on the predefined (or custom) rules.
 * 
 * Additional output handlers are provided by the library or the
 * user may create custom output handlers.
 * 
 * The output method accepts an optional filter object which
 * may create running averages or other aggregate values.
 * 
 * Function signature prototype temaplate:
 * 
 * int SYSSAGE_PAPI_xxx(papi_params, syssage_params)
 */


//
// data collection API
// a) no sys-sage Component
//

/**
 * Registers a PAPI eventset for storage in sys-sage. The eventset
 * must be a valid and fully configured PAPI eventset, which is not
 * yet started.
 * The event names are queried and recorded. The start timestamp is saved.
 * The eventset are started with PAPI_start(int)
 */
int SYSSAGE_PAPI_start(int eventSet);

int SYSSAGE_PAPI_accum(int eventSet);
int SYSSAGE_PAPI_read(int eventSet);
int SYSSAGE_PAPI_stop(int eventSet);

/// destroy PAPI eventset with PAPI_destroy and frees counter storage
int SYSSAGE_PAPI_destroy(int eventSet);

// low level/internal
// optional: one can add arbitrary values to be stored with papi data
// the number of values must match the eventset num_events!
int SYSSAGE_PAPI_add_values(int eventSet, const std::vector<long long>& values);

class OutputHandler;
class CounterFilter;

// only possible if the eventset is stopped
int SYSSAGE_PAPI_out(int eventSet, OutputHandler& handler, CounterFilter* filter);

//
// data collection API
// b) with sys-sage Component
//
int SYSSAGE_PAPI_stop(int eventSet, Component* component);
int SYSSAGE_PAPI_stop(int eventSet, Topology* topology, Component** boundComponent);

// low level/internal methods for connecting an eventset to a component
int SYSSAGE_PAPI_bind(int eventSet, Component* component);
int SYSSAGE_PAPI_automatic_bind(int eventSet, Topology* topology, Component** boundComponent);


/// @brief Metrics attribute handler
int PAPI_attribHandler(std::string key, void* value, std::string* ret_value_str);

/// @brief Metrics attribute handler for XML export
int PAPI_attribXmlHandler(std::string key, void* value, xmlNodePtr n);

/// @brief Retrieves the vector of PAPI event names from the environment variable SYS_SAGE_METRICS
std::vector<std::string> PAPI_EventsFromEnvironment();

#endif //PAPI_METRICS

#endif //METRICS