#ifndef METRICS
#define METRICS

#include "defines.hpp"

#ifdef PAPI_METRICS

#include <vector>
#include <functional>

#include "Topology.hpp"

#define SYSSAGE_PAPI_ECHANGED -100  /**< EventSet exists but parameters have changed */

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

//
// data collection API
// sys-sage binding
//
int SYSSAGE_PAPI_stop(int eventSet, Component* component);
int SYSSAGE_PAPI_stop(int eventSet, Topology* topology, Component** boundComponent);
int SYSSAGE_PAPI_stop(int eventSet, Topology* topology, std::vector<Component*>& boundComponents);

// low level/internal methods for connecting an eventset to a component
int SYSSAGE_PAPI_bind(int eventSet, Component* component);
int SYSSAGE_PAPI_unbind(int eventSet, Component* component);
int SYSSAGE_PAPI_automatic_bind(int eventSet, Topology* topology, Component** boundComponent);
int SYSSAGE_PAPI_automatic_bind(int eventSet, Topology* topology, std::vector<Component*>& boundComponents);

// 
// Utility functions
//
int SYSSAGE_PAPI_print(int eventSet);
int SYSSAGE_PAPI_csv(int eventSet, const std::string& path);

int SYSSAGE_PAPI_export_xml(Component* topology, const std::string& path);

/// @brief Retrieves the vector of PAPI event names from the environment variable SYS_SAGE_METRICS
std::vector<std::string> SYSSAGE_PAPI_EventsFromEnvironment();

// optional: one can add arbitrary values to be stored with papi data
// the number of values must match the eventset num_events otherwise an 
// error code is returned
int SYSSAGE_PAPI_add_values(int eventSet, const std::vector<long long>& values);

// 
// Generic Data access
// using the visitor pattern
//
class SYSSAGE_PAPI_Visitor {
    public:
    virtual bool data(int sessionId, long long sessionStartTs, long long countersTs, int core, const std::vector<long long>& counters) = 0;
    virtual void info(int eventSet, int core, unsigned long tid, const std::vector<std::string>& eventNames) = 0;
};

int SYSSAGE_PAPI_visit(int eventSet, SYSSAGE_PAPI_Visitor& visitor);

template<typename T>
struct SYSSAGE_PAPI_DataTable {
    std::vector<std::string> headers;
    std::vector<std::vector<T>> rows;
};

template<typename T>
struct SYSSAGE_PAPI_Freezer : public SYSSAGE_PAPI_Visitor {
    virtual SYSSAGE_PAPI_DataTable<T>& frozen() = 0;
    virtual void defrost() = 0;
};

int SYSSAGE_PAPI_freeze(Component* topology, SYSSAGE_PAPI_Freezer<std::string>& freezer);
int SYSSAGE_PAPI_freeze(Component* topology);

int SYSSAGE_PAPI_cleanup(Component* topology);

#endif //PAPI_METRICS

#endif //METRICS