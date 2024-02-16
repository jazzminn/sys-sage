#include "papi/Utility.hpp"
#include "papi/Statistics.hpp"

#include <iomanip>

using namespace papi;

SYSSAGE_PAPI_DataTable<std::string>& DefaultFreezer::frozen() {
    return table;
}

void DefaultFreezer::defrost() {
    table.headers.clear();
    table.rows.clear();
}

bool DefaultFreezer::data(int sid, long long sts, long long ts, int core, const std::vector<long long>& counters) {
    auto elapsed = ts - sts;
    if ( elapsed > 0 ) {
        std::vector<std::string> columns;
        columns.push_back(std::to_string(elapsed));
        for(auto& c: counters) {
            columns.push_back(std::to_string(c));
        }
        table.rows.emplace_back(columns);
    }
    return true;
}

void DefaultFreezer::info(int eventSet, int core, unsigned long tid, const std::vector<std::string>& eventNames) {
    table.headers.push_back("time (us)");
    for(auto& e: eventNames) {
        table.headers.push_back(e);
    }
}


Printer::Printer(int width) 
    : column_width{width} 
    {}

bool Printer::data(int sid, long long sts, long long ts, int core, const std::vector<long long>& counters) {
    if ( sid != session_id ) {
        std::cout << "Session " << sid << " start timestamp: " << sts << std::endl;
        session_id = sid;
    }
    std::cout 
        << std::setw(column_width) << ts
        << std::setw(column_width) << core;
    for(auto& c: counters) {
        std::cout << std::setw(column_width) << c;
    }
    std::cout << std::endl;
    return true;
}

void Printer::info(int eventSet, int core, unsigned long tid, const std::vector<std::string>& eventNames) {
    std::cout << "EventSet: " << eventSet << std::endl;
    if ( tid > 0 ) {
        std::cout << "Attached TID: " << tid << std::endl;
    }
    if ( core > -1 ) {
        std::cout << "Attached CPU: " << core << std::endl;
    }
    std::cout 
        << std::setw(column_width) << "timestamp"
        << std::setw(column_width) << "core";
    for(auto& e: eventNames) {
        std::cout << std::setw(column_width) << e;
    }
    std::cout << std::endl;
}

SYSSAGE_PAPI_DataTable<std::string>& StatisticsHandler::frozen() {
    if ( !table.headers.empty() ) return table;
    // build statistics table
    auto size = names.size();
    table.headers.push_back("Event");
    table.headers.push_back("Min");
    table.headers.push_back("Max");
    table.headers.push_back("Mean");
    table.headers.push_back("Median");
    for(size_t i=0; i<size; ++i) {
        auto stats = papi::Statistics<long long>::calculate(papi::Statistics<long long>::diff(columns[i]));
        std::vector<std::string> row;
        row.push_back(names[i]);
        row.push_back(std::to_string(stats.min));
        row.push_back(std::to_string(stats.max));
        row.push_back(std::to_string(stats.average));
        row.push_back(std::to_string(stats.median));
        table.rows.emplace_back(row);
    }
    return table;
}

void StatisticsHandler::defrost() {
    table.headers.clear();
    table.rows.clear();
}

bool StatisticsHandler::data(int sid, long long sts, long long ts, int core, const std::vector<long long>& counters) {
    int idx = 0;
    columns[idx++].push_back(ts-sts);
    for(auto& c: counters) {
        columns[idx++].push_back(c);
    }
    return true;
}

void StatisticsHandler::info(int eventSet, int core, unsigned long tid, const std::vector<std::string>& eventNames) {
    names.push_back("Timestamp");
    for(auto& e: eventNames) {
        names.push_back(e);
    }
    columns.resize(eventNames.size() + 1);
}
