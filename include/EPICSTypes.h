// SPDX-FileCopyrightText: Helmholtz-Zentrum Dresden-Rossendorf, FWKE, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
/*
 * EPICS_types.h
 *
 *  Created on: Jan 22, 2021
 *      Author: Klaus Zenker (HZDR)
 */

#include <cadef.h>
#include <epicsTime.h>

static constexpr int default_ca_priority = 0;
static constexpr float default_ca_timeout = 30.0;

/* Type of timestamp */
typedef enum { absolute, relative, incremental, incrementalByChan } TimeT;

/* Output formats for integer data types */
typedef enum { dec, bin, oct, hex } IntFormatT;

/* Structure representing one PV (= channel) */
typedef struct {
  char* name;
  chanId chid;
  long dbfType;
  long dbrType;
  unsigned long nElems;   // True length of data in value
  unsigned long reqElems; // Requested length of data
  int status;
  void* value;
  epicsTimeStamp tsPreviousC;
  epicsTimeStamp tsPreviousS;
  char firstStampPrinted;
  char onceConnected;
} pv;
