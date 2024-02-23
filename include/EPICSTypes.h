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

/* Structure representing one PV (= channel) */
typedef struct {
  char* name;
  chanId chid;
  long dbfType;
  long dbrType;
  unsigned long nElems; ///< True length of data in value
  void* value;
} pv;
