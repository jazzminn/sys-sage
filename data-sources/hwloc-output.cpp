#include <iostream>
#include <string>
#include <fstream>
#include <sstream>
#include <hwloc.h>


using namespace std;


/*! \file */
/// @private
string get_hwloc_topology_xml_string();

/**
Binary (entrypoint) for generating hwloc topology XML output (to current directory)
\n usage: ./hwloc-output [output_filename]
@param filename of the output file (default: hwloc_topology.xml)
*/

int main(int argc, char* argv[])
{
    
    string filename;
    if (argc < 2) {
        filename = "hwloc_topology.xml";
    }
    else {
       filename = argv[1];
    }
    string xml_output = get_hwloc_topology_xml_string();

    if (!xml_output.empty()) {
        ofstream outfile;
        outfile.open(filename);
        outfile << xml_output;
        outfile.close();
        //cout << "Hwloc XML output exported to " << filename << endl;
    }
    else {
        cerr << "Failed to generate hwloc topology XML output" << endl;
    }

    return 0;
}

string get_hwloc_topology_xml_string() {
    int err;
    unsigned long flags = 0; // don't show anything special
    hwloc_topology_t topology;

    err = hwloc_topology_init(&topology);
    if (err) {
        cerr << "hwloc: Failed to initialize" << endl;
        return "";
    }
    err = hwloc_topology_set_flags(topology, flags);
    if (err) {
        cerr << "hwloc: Failed to set flags" << endl;
        hwloc_topology_destroy(topology);
        return "";
    }
    err = hwloc_topology_load(topology);
    if (err) {
        cerr << "hwloc: Failed to load topology" << endl;
        hwloc_topology_destroy(topology);
        return "";
    }
    //TODO replace with hwloc_topology_export_xmlbuffer?
    stringstream xml_output_stream;
    err = hwloc_topology_export_xml(topology, xml_output_stream.str().c_str(), flags);
    if (err) {
        cerr << "hwloc: Failed to export xml" << endl;
        hwloc_topology_destroy(topology);
        return "";
    }

    hwloc_topology_destroy(topology);
    string xml_output = xml_output_stream.str();
    return xml_output;
}
