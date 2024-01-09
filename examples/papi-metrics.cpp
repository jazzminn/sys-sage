#include <iostream>
#include <filesystem>
#include <unistd.h>
#include <tuple>
#include <thread>

#include "sys-sage.hpp"

/// @brief Simple parallel load to be analyzed
/// @param cores number of threads to start
static long loop_size = 1000000000;

void make_load(int cores) {
    std::thread threads[cores];
    long values[cores];
    for(int i=0; i<cores; i++) {
        threads[i] = std::thread([i, &values] {
            values[i] = 0;
            for(long l = 0; l < loop_size; l++) {
                values[i] += l;
            }
        });
    }
    for(int i=0; i<cores; i++) {
        threads[i].join();
    }
}



void usage(char* argv0)
{
    std::cerr << "usage: " << argv0 << " hwloc_xml_path [xml output path/name]" << std::endl;
    return;
}

int main(int argc, char *argv[])
{
    int rv;
    string topoPath;
    string output_name = "sys-sage_papi-metrics.xml";
    
    if(argc == 2){
        topoPath = argv[1];
    } else if(argc == 3){
        topoPath = argv[1];
        output_name = argv[2];
    } else {
        usage(argv[0]);
        return 1;
    }

    Topology* topo = new Topology();
    Node* n = new Node(topo, 1);

    if(parseHwlocOutput(n, topoPath) != 0) { //adds topo to a next node
        usage(argv[0]);
        return 1;
    }

    // Standard Sys-Sage Metrics Usage
    // - build a congfiguration object
    //Measurement::Configuration configuration{{"PAPI_TOT_CYC", "PAPI_TOT_INS"}};
    // Alternatives for configuration, file, environment, command line
    //Measurement::Configuration configuration = Measurement::Configuration::read(filename);
    Measurement::Configuration configuration = Measurement::Configuration::fromEnvironment();
    //Measurement::Configuration configuration = Measurement::Configuration::build(argc, argv);

    // Instrument code
    rv = Measurement::begin("parallel_region", &configuration, topo);
    if ( rv != STATUS_OK ) {
        std::cerr << "Failed to begin measurement" << std::endl;
        return EXIT_FAILURE;
    }

    make_load(4);
    
    rv = Measurement::end("parallel_region");
    if ( rv != STATUS_OK ) {
        std::cerr << "Failed to begin measurement" << std::endl;
        return EXIT_FAILURE;
    }


    exportToXml(topo, output_name, Measurement::attribHandler, Measurement::attribXmlHandler);
    delete topo;
    delete n;
    return 0;
}
