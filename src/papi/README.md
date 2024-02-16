# sys-sage PAPI integration

## Installation

### Installation with CMAKE

#### Basic dependencies

- cmake (3.22 or newer)
- libxml2 (tested with 2.9.13)
- PAPI library (tested with 7.1.0)
- PAPI header files

#### Building from sources

```bash
git clone https://github.com/jazzminn/sys-sage.git
cd sys-sage
git checkout papi_addon
mkdir build && cd build
cmake .. -DPAPI_METRICS=ON -DCPUINFO=ON
make 
```

## Examples

### examples/papi-metrics-storage-only.cpp   

Basic PAPI eventSet data storage with sys-sage PAPI.

Usage:

```bash
    ./papi-metrics-storage-only
``` 

### examples/papi-metrics-storage-only-cpu.cpp

CPU attached eventSet data storage with sys-sage PAPI.

Usage:

```bash
    ./papi-metrics-storage-only-cpu
``` 

### examples/papi-metrics-storage-only-tid.cpp

Child process attached eventSet data storage with sys-sage PAPI.

Usage:

```bash
    ./papi-metrics-storage-only-tid
``` 

### Creating hwloc data

The examples below require the topo.xml with hwloc information.

```bash
    lstopo-no-graphics --of xml > topo.xml
``` 

### examples/papi-metrics-topology.cpp

Basic PAPI eventSet data storage with automatic binding to sys-sage component.

Usage:

```bash
    ./papi-metrics-topology topo.xml [live_xml] [datatable_xml]
``` 

The live_xml and datatable_xml output file defaults are sys-sage_papi-metrics-live.xml and
sys-sage_papi-metrics-table.xml. 

The live_xml contains the "live" measurement data, the datatable_xml includes a snapshot of the metrics.

### examples/papi-metrics-green-score.cpp   

It executes the specified application and runs a monitoring loop, which examines the state of the started application and
for each thread starts an attached PAPI eventSet. With automatic binding, it determines the active hardware thread
and collects CPU information along with PAPI metrics.

Usage:

```bash
    ./papi-metrics-green-score topo.xml path-to-application [application parameters]
``` 

### examples/papi-metrics-runner.cpp  

It executes the specified application and collects PAPI metrics at the termination of the application. It prints and exports the metrics to sys-sage_papi-metrics.xml.

Usage:

```bash
    ./papi-metrics-runner topo.xml path-to-application [application parameters]
``` 

### examples/papi-metrics-sampler.cpp   

It executes the specified application and samples PAPI metrics until the application terminates. It prints and exports the metrics to sys-sage_papi-metrics.xml.

Usage:

```bash
    ./papi-metrics-sampler topo.xml path-to-application [application parameters]
``` 


## Benchmarks

### examples/papi-metrics-benchmarking.cpp  

The benchmark collects
the elapsed time of the function calls in a start-read-stop sequence and repeats the
measurement for 100 times.

Usage:

```bash
    ./papi-metrics-benchmark [event_count] [repeat_count]
```    

The default number of events measured is 9 and the repeat count is 100.