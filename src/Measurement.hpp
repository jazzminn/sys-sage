#ifndef MEASUREMENT
#define MEASUREMENT

#include "defines.hpp"
#include "Topology.hpp"

#ifdef PAPI_METRICS

#include <string>
#include <vector>

#define STATUS_OK 0
#define MEASUREMENT_ERROR -1000
#define MEASUREMENT_ERROR_REGION_EXISTS -1001
#define MEASUREMENT_ERROR_REGION_NOT_EXIST -1002
#define MEASUREMENT_ERROR_NO_CONFIG -1003
#define MEASUREMENT_ERROR_CANNOT_CREATE -1004
#define MEASUREMENT_ERROR_NOT_IMPLEMENTED -1005
#define MEASUREMENT_ERROR_INVALID_EVENTNAME -1006
#define MEASUREMENT_ERROR_INVALID_TOPOLOGY -1007
#define MEASUREMENT_ERROR_INVALID_CONFIG -1008
#define MEASUREMENT_ERROR_TID_NOT_FOUND -1009

/// @brief Top level interface for performance measurement
class Measurement {
    public:

    struct ComponentInfo {
        int componentType;
        // additional attributes
        int level = 0;
        bool override(const std::string& option);
    };

    /// @brief Returns the preferred sys-sage component of a measurement event
    /// @param eventName name of the PAPI event
    /// @return the component type
    static ComponentInfo getComponentForEvent(const std::string& eventName);
    
    /// @brief Metrics attribute handler
    static int attribHandler(std::string key, void* value, std::string* ret_value_str);

    /// @brief Metrics attribute handler for XML export
    static int attribXmlHandler(std::string key, void* value, xmlNodePtr n);


    /**
     * @brief Configuration of sys-sage measurement
     * Concept:
     * - given the list of events (PAPI_TOT_INS, perf::INSTRUCTIONS, ...)
     * - 
     */
    struct Configuration {
        enum class Mode {
            anyCpu, // current process current cpu
            threadCpu, // attach to the list of TIDs and use thread affinity
            allCpu, // current process all cpu of the topology
            system // all process all cpu of the topology (granularity system)
        };
        struct Event {
            std::string name;
            std::string option;
        };

        // factory methods
        /// e.g.: export SYS_SAGE_METRICS="PAPI_TOT_CYC,perf::INSTRUCTIONS,perf::PERF_COUNT_SW_CPU_CLOCK:u=0"
        /// or manual override mapping:
        /// PAPI_TOT_CYC[core], perf::INSTRUCTIONS[node], perf::PERF_COUNT_SW_CPU_CLOCK:u=0[l3]
        static Configuration fromEnvironment();
        static Configuration fromFile(const std::string& configFileName);
        static Configuration fromCommandline(int argc, char* argv[]);

        Configuration();
        Configuration(const std::vector<std::string>& eventList);
        bool add(const std::string& event);
        bool add(const std::string& event, const std::string& option);
        
        std::vector<Event> events;
        std::vector<int> threads;

        Mode mode{Mode::anyCpu};
        
        bool multiplex{false};
        bool systemGranularity{false};
    };

    /// @brief Initialize the measurement, allocate internal measurement object
    /// @param region identifier of measurement
    /// @param configuration configuration object
    /// @param component topology tree
    /// @return status code see STATUS_OK, MEASUREMENT_ERROR, PAPI_*
    static int init(const std::string& region, Configuration* configuration, Component* component);

    /// @brief Delete internal measurement objects and metrics data
    /// @param region identifier of measurement
    /// @return status code
    static int deinit(const std::string& region);

    /// @brief Start measurement
    /// @param region identifier of measurement
    /// @return status code
    static int start(const std::string& region);

    /// @brief Read counters, store results together with timestamp
    /// @param region identifier of measurement
    /// @return status code
    static int read(const std::string& region);

    /// @brief Stop measurement, get counters, store results together with timestamp
    /// @param region identifier of measurement
    /// @return status code
    static int stop(const std::string& region);

    /// @brief Save measurement result to the component
    /// @param region identifier of measurement
    /// @return status code
    static int save(const std::string& region);

    static int counters(const std::string& region, int tid, std::vector<long long>& counters);

    // convenience methods for code instrumentation (~PAPI High Level interface)

    /// @brief Begin calls init and start
    static int begin(const std::string& region, Configuration* configuration, Component* component);
    static int begin(const std::string& region, Configuration* configuration);
    static int begin(const std::string& region, Component* component);
    static int begin(const std::string& region);

    /// @brief End calls stop and save the results to the component and deinit the region
    static int end(const std::string& region);
    
};

#endif
#endif
