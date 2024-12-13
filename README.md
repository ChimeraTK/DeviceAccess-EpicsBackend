# DeviceAccess-EPICS-Backend

[![License](https://img.shields.io/badge/license-LGPLv3-blue.svg)](https://www.gnu.org/licenses/lgpl-3.0.html)
[![DOI](https://rodare.hzdr.de/badge/DOI/10.14278/rodare.3333.svg)](https://doi.org/10.14278/rodare.3333)
![Supported Platforms][api-platforms]

[api-platforms]: https://img.shields.io/badge/platforms-linux%20-blue.svg "Supported Platforms"

This backend allows to access EPICS process variables via EPICS channel access.
It was tested with EPICS R7.0.5.

## Usage

In order to use this backend add device to your device mapping file using the following syntax:
    
    Test (epics:?map=epics.map)
    
The specified mapping file `epics.map` includes the names of the EPICS channels to be accessed in combination with an register path to be used in ChimeraTK:

    #epics.map
    epics_data/testArray test:compressExample
    
In the above example the EPICS channel `test:compressExample` will be mapped to the ChimeraTK register path `epics_data/testArray`.
Using the submodule structure is posible but not necessary. E.g. one could also just assign the register path `testArray`.
    
### Installation

If you have not installed EPICS in a standard install directory pass the EPICS path to cmake using:
`-DEPICS_BASE_DIR=PATH_TO_EPICS`.
