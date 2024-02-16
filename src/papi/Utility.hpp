#pragma once

#include "papi/Metrics.hpp"
#include <iostream>
#include <iomanip>

namespace papi {
    /// @brief Default freezer handler. Converts values to a simple string table
    class DefaultFreezer: public SYSSAGE_PAPI_Freezer {
        SYSSAGE_PAPI_DataTable<std::string> table;
    public:
        SYSSAGE_PAPI_DataTable<std::string>& frozen();
        void defrost();

        bool data(int sid, long long sts, long long ts, int core, const std::vector<long long>& counters);
        void info(int eventSet, int core, unsigned long tid, const std::vector<std::string>& eventNames);
    };

    /// @brief Prints eventSet measurements to STDOUT
    class Printer : public SYSSAGE_PAPI_Visitor {
        int column_width = 16;
        int session_id = -1;
    public:
        Printer(int width = 16);

        bool data(int sid, long long sts, long long ts, int core, const std::vector<long long>& counters);
        void info(int eventSet, int core, unsigned long tid, const std::vector<std::string>& eventNames);

        template<typename T>
        static void print(const SYSSAGE_PAPI_DataTable<T>& table, std::ostream& os = std::cout, int column_width = 16);
    };

    /// @brief Calculates statistics of measured values
    class StatisticsHandler : public SYSSAGE_PAPI_Freezer {
        SYSSAGE_PAPI_DataTable<std::string> table;
        std::vector<std::string> names;
        std::vector<std::vector<long long>> columns;
    public:
        SYSSAGE_PAPI_DataTable<std::string>& frozen();
        void defrost();

        bool data(int sid, long long sts, long long ts, int core, const std::vector<long long>& counters);
        void info(int eventSet, int core, unsigned long tid, const std::vector<std::string>& eventNames);   
    };
    template<typename T>
    void Printer::print(const SYSSAGE_PAPI_DataTable<T>& table, std::ostream& os, int column_width) {
        for(auto& h: table.headers) {
            os << std::setw(column_width) << h;
        }
        std::cout << std::endl;
        for(auto& row: table.rows) {
            for(auto v: row) {
                os << std::setw(column_width) << v;
            }
            os << std::endl;
        }
    }

}