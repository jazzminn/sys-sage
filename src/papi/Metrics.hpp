#ifndef METRICS
#define METRICS

#include "defines.hpp"

#ifdef PAPI_METRICS

#include <vector>

#include "Topology.hpp"

#define SYSSAGE_PAPI_ECHANGED -100  /**< EventSet exists but parameters have changed */

// Timestamp function of PAPI Metrics
// the variable define TIMESTAMP_FUNCTION may override the default
// timestamp function, the function prototype must be
//   uint64_t get_timestamp()
// 
#ifndef TIMESTAMP_FUNCTION
#define TIMESTAMP_FUNCTION PAPI_get_real_usec
#endif

//
// data collection API
//

/**
 * Starts and registers a PAPI eventset with storage in sys-sage PAPI. The eventset
 * must be a valid and fully configured PAPI eventset, which is not
 * yet started. It is not allowed to change the eventSet after it has been
 * registered.
 * @param eventSet PAPI event set
 * @return PAPI_OK (0) on success or an error code
 */
int SYSSAGE_PAPI_start(int eventSet);

/**
 * Reads counters and stores the values in the sys-sage PAPI storage. The eventSet
 * must have been registered with \ref SYSSAGE_PAPI_start(int)
 * @param eventSet PAPI event set
 * @return PAPI_OK (0) on success or an error code
 */
int SYSSAGE_PAPI_read(int eventSet);

/**
 * Reads and resets counters and stores the values in the sys-sage PAPI storage. The eventSet
 * must have been registered with \ref SYSSAGE_PAPI_start(int)
 * @param eventSet PAPI event set
 * @return PAPI_OK (0) on success or an error code
 */
int SYSSAGE_PAPI_accum(int eventSet);

/**
 * Stops eventSet and saves counters and stores the values in the sys-sage PAPI storage. The eventSet
 * must have been registered with \ref SYSSAGE_PAPI_start(int)
 * @param eventSet PAPI event set
 * @return PAPI_OK (0) on success or an error code
 */
int SYSSAGE_PAPI_stop(int eventSet);

/**
 * Destroys the eventSet and deletes the stored values from the sys-sage PAPI storage. The eventSet
 * must have been registered with \ref SYSSAGE_PAPI_start(int)
 * @param eventSet pointer to PAPI event set variable
 * @return PAPI_OK (0) on success or an error code
 */
int SYSSAGE_PAPI_destroy_eventset(int* eventSet);

//
// data collection API
// sys-sage binding
//

/**
 * Stops eventSet \ref SYSSAGE_PAPI_stop(int) and assigns the stored values to a given component
 * @param eventSet PAPI event set
 * @param component sys-sage Component, to which stored values should be assigned to
 * @return PAPI_OK (0) on success or an error code
 */
int SYSSAGE_PAPI_stop(int eventSet, Component* component);

/**
 * Stops eventSet \ref SYSSAGE_PAPI_stop(int) and assigns the stored values to a component automatically
 * @param eventSet PAPI event set
 * @param topology sys-sage topology
 * @param[out] boundComponent receives the sys-sage Component, to which stored values are automatically bound to
 * @return PAPI_OK (0) on success or an error code
 */
int SYSSAGE_PAPI_stop(int eventSet, Topology* topology, Component** boundComponent);

/**
 * Stops eventSet \ref SYSSAGE_PAPI_stop(int) and assigns the stored values to multiple components automatically
 * @param eventSet PAPI event set
 * @param topology sys-sage topology
 * @param boundComponent vector will get the sys-sage Components, to which stored values are automatically bound to
 * @return PAPI_OK (0) on success or an error code
 */
int SYSSAGE_PAPI_stop(int eventSet, Topology* topology, std::vector<Component*>& boundComponents);

// low level/internal methods for connecting an eventset to a component

/**
 * Binds eventSet to a given sys-sage Component
 * @param eventSet PAPI event set
 * @param component sys-sage Component, to which stored values are bound to
 * @return PAPI_OK (0) on success or an error code
 */
int SYSSAGE_PAPI_bind(int eventSet, Component* component);

/**
 * Removes binding
 * @param eventSet PAPI event set
 * @param component sys-sage Components, to which stored values were bound to
 * @return PAPI_OK (0) on success or an error code
 */
int SYSSAGE_PAPI_unbind(int eventSet, Component* component);

/**
 * Binds eventSet to a given sys-sage Component automatically
 * @param eventSet PAPI event set
 * @param topology sys-sage topology
 * @param[out] boundComponent receives sys-sage Component, to which stored values are automatically bound to
 * @return PAPI_OK (0) on success or an error code
 */
int SYSSAGE_PAPI_automatic_bind(int eventSet, Topology* topology, Component** boundComponent);

/**
 * Binds events of an evnetSet to a multiple sys-sage Components automatically
 * @param eventSet PAPI event set
 * @param topology sys-sage topology
 * @param boundComponent vector will get sys-sage Components, to which stored values are automatically bound to
 * @return PAPI_OK (0) on success or an error code
 */
int SYSSAGE_PAPI_automatic_bind(int eventSet, Topology* topology, std::vector<Component*>& boundComponents);

// 
// Utility functions
//

/**
 * Prints the eventSet properties and the stored measurements
 * @param eventSet PAPI event set
 * @return PAPI_OK (0) on success or an error code
 */
int SYSSAGE_PAPI_print(int eventSet);

/**
 * Exports all collected metrics to a topology XML
 * @param topology sys-sage topology
 * @param path name of the export file
 * @return PAPI_OK (0) on success or an error code
 */
int SYSSAGE_PAPI_export_xml(Component* topology, const std::string& path);

/// @brief Retrieves the vector of PAPI event names from the environment variable SYS_SAGE_METRICS
std::vector<std::string> SYSSAGE_PAPI_EventsFromEnvironment();

/**
 * Adds arbitrary values to be stored with papi data.
 * The number of values must match the eventset num_events otherwise an 
 * error code is returned
 * @param eventSet PAPI event set
 * @param values manually extracted counter values
 * @return PAPI_OK (0) on success or an error code
 */
int SYSSAGE_PAPI_add_values(int eventSet, const std::vector<long long>& values);

// 
// Generic Data access
// using the visitor pattern
//
/**
 * 
 * Interface for accessing sys-sage PAPI data
 */
class SYSSAGE_PAPI_Visitor {
    public:
    /**
     * Will be called for each reading of the measurements
     * @param sessionId session ID
     * @param sessionStartTs session start time stamp
     * @param countersTs counter reading time stamp
     * @param core counter reading logical core
     * @param counters counter readings
     * @return if false the reading will be aborted
     */
    virtual bool data(int sessionId, long long sessionStartTs, long long countersTs, int core, const std::vector<long long>& counters) = 0;
    
    /**
     * Will be called once before calling \ref data()
     * @param eventSet PAPI event set
     * @param core CPU attached to the eventset (-1 if not)
     * @param tid tid/pid attached to the eventset (0 if not)
     * @param eventNames event names
     */
    virtual void info(int eventSet, int core, unsigned long tid, const std::vector<std::string>& eventNames) = 0;
};

/**
 * Visitor object executed on the eventSet measuremnet data
 * @param eventSet PAPI event set
 * @param visitor reference to a visitor object \ref SYSSAGE_PAPI_Visitor
 * @return PAPI_OK (0) on success or an error code
 */
int SYSSAGE_PAPI_visit(int eventSet, SYSSAGE_PAPI_Visitor& visitor);

/**
 * General tabular data storage object
 */
template<typename T>
struct SYSSAGE_PAPI_DataTable {
    std::vector<std::string> headers;
    std::vector<std::vector<T>> rows;
};

/**
 * Interface for serializing evetSet measurements \ref SYSSAGE_PAPI_freeze
 */
struct SYSSAGE_PAPI_Freezer : public SYSSAGE_PAPI_Visitor {
    /**
     * Returns the serialized data
     * @return serialized data table
     */
    virtual SYSSAGE_PAPI_DataTable<std::string>& frozen() = 0;
    
    /**
     * Resets the freezer object
     */
    virtual void defrost() = 0;
};

/**
 * Serializes live eventSet data bound to components by using the freezer parameter
 * @param topology sys-sage topology
 * @param freezer freezer object \ref SYSSAGE_PAPI_Freezer
 * @return PAPI_OK (0) on success or an error code
 */
int SYSSAGE_PAPI_freeze(Component* topology, SYSSAGE_PAPI_Freezer& freezer);

/**
 * Serializes live eventSet data bound to components by using the default freezer 
 * @param topology sys-sage topology
 * @return PAPI_OK (0) on success or an error code
 */
int SYSSAGE_PAPI_freeze(Component* topology);

/**
 * Removes all sys-sage PAPI references from the topology
 * @param topology sys-sage topology
 * @return PAPI_OK (0) on success or an error code
 */
int SYSSAGE_PAPI_cleanup(Component* topology);

#endif //PAPI_METRICS

#endif //METRICS