/*
 * EPICS_types.h
 *
 *  Created on: Jan 22, 2021
 *      Author: Klaus Zenker (HZDR)
 */

#ifndef INCLUDE_EPICS_TYPES_H_
#define INCLUDE_EPICS_TYPES_H_

#include <epicsTime.h>
#include <cadef.h>

#define DEFAULT_CA_PRIORITY 0  /* Default CA priority */
#define DEFAULT_TIMEOUT 1.0     /* Default CA timeout */

/* Type of timestamp */
typedef enum { absolute, relative, incremental, incrementalByChan } TimeT;

/* Output formats for integer data types */
typedef enum { dec, bin, oct, hex } IntFormatT;

/* Structure representing one PV (= channel) */
typedef struct
{
    char* name;
    chanId  chid;
    long  dbfType;
    long  dbrType;
    unsigned long nElems;       // True length of data in value
    unsigned long reqElems;     // Requested length of data
    int status;
    void* value;
    epicsTimeStamp tsPreviousC;
    epicsTimeStamp tsPreviousS;
    char firstStampPrinted;
    char onceConnected;
} pv;


#endif /* INCLUDE_EPICS_TYPES_H_ */
