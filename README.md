# DeviceAccess-EPICS-Backend

This backend allows to access EPICS process variables via EPICS channel access.
It was tested with EPICS 3.15.7, but in principle channel access is also available with EPICS 7.

## Usage

In order to use this backend add device to your device mapping file using the following syntax:
    
    Test (epics:?map=epics.map)
    
The specified mapping file `epics.map` includes the names of the EPICS channels to be accessed in combination with an register path to be used in ChimeraTK:

    #epics.map
    epics_data/testArray test:compressExample
    
In the above example the EPICS channel `test:compressExample` will be mapped to the ChimeraTK register path `epics_data/testArray`.
Using the submodule structure is posible but not necessary. E.g. one could also just assign the register path `testArray`.
    
## Technical details

### Testing

For testing one can build a simple IOC using EPICS tools. See [EPICS guide](https://epics.anl.gov/base/R3-15/5-docs/AppDevGuide/GettingStarted.html) section 2.2.


### Installation

If you have not installed EPICS in a standard install directory pass the EPICS path to cmake using:
`-DEPICS_BASE_DIR=PATH_TO_EPICS`.

### To do

* Use the UnifiedBackendTest to implement unit tests
  * This requires to use a EPICS IOC
  * Should EPICS be build within the DeviceAccess-EPICS-Backend or should an interface package be set up
* With EPICS 7 a new C++ interface is introduced &rarr; pvaccess
  * Since channel access will still be available should DeviceAccess-EPICS-Backend still use it?  
