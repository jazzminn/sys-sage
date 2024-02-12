#ifndef METRICS
#define METRICS

#include "defines.hpp"

#ifdef PAPI_METRICS

#include <vector>
#include <functional>

#include "Topology.hpp"

#define SYSSAGE_PAPI_ECHANGED -100  /**< EventSet exists but parameters have changed */

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
int SYSSAGE_PAPI_destroy_eventset(int* eventSet);

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

struct SYSSAGE_PAPI_Freezer : public SYSSAGE_PAPI_Visitor {
    virtual SYSSAGE_PAPI_DataTable<std::string>& frozen() = 0;
    virtual void defrost() = 0;
};

int SYSSAGE_PAPI_freeze(Component* topology, SYSSAGE_PAPI_Freezer& freezer);
int SYSSAGE_PAPI_freeze(Component* topology);

int SYSSAGE_PAPI_cleanup(Component* topology);

#endif //PAPI_METRICS

#endif //METRICS