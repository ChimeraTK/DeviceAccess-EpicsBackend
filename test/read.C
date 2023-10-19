// SPDX-FileCopyrightText: Helmholtz-Zentrum Dresden-Rossendorf, FWKE, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#include <cadef.h>
#include <iostream>
#include <string>

#include "../include/EPICSTypes.h"
static epicsTimeStamp tsStart;
int main() {
  auto result = ca_context_create(ca_disable_preemptive_callback);
  if(result != ECA_NORMAL) {
    std::cerr << "CA error " << ca_message(result)
              << " occurred while trying "
                 "to start channel access"
              << std::endl;
    return 1;
  }
  pv* mypv = (pv*)calloc(1, sizeof(pv));
  std::string pvName("test:compressExample");
  mypv->name = (char*)pvName.c_str();
  epicsTimeGetCurrent(&tsStart);
  result = ca_create_channel(mypv->name, 0, &mypv, DEFAULT_CA_PRIORITY, &mypv->chid);
  if(result != ECA_NORMAL) {
    std::cerr << "CA error " << ca_message(result)
              << " occurred while trying "
                 "to create channel '"
              << mypv->name << "'." << std::endl;
    return 1;
  }
  result = ca_pend_io(1);
  if(result == ECA_TIMEOUT) {
    std::cerr << "Channel time out for PV: " << pvName << std::endl;
    return 1;
  }

  if(result != ECA_NORMAL) {
    std::cerr << "CA error " << ca_message(result) << " occurred while trying to create channel " << mypv->name
              << std::endl;
  }
  mypv->status = result;

  unsigned long nElems = ca_element_count(mypv->chid);
  mypv->dbfType = ca_field_type(mypv->chid);
  mypv->dbrType = dbf_type_to_DBR_TIME(mypv->dbfType);
  if(ca_state(mypv->chid) == cs_conn) {
    mypv->onceConnected = 1;
    mypv->nElems = nElems;
    mypv->value = calloc(1, dbr_size_n(mypv->dbrType, mypv->nElems));
    if(!mypv->value) {
      fprintf(stderr, "Memory allocation failed\n");
      return 1;
    }
    result = ca_array_get(mypv->dbrType, mypv->nElems, mypv->chid, mypv->value);
    result = ca_pend_io(1);
    if(result == ECA_TIMEOUT) std::cerr << "Read operation timed out: some PV data was not read." << std::endl;
  }
  else {
    std::cerr << "No connection..." << std::endl;
    return 1;
  }
  for(size_t i = 0; i < mypv->nElems; i++) {
    dbr_time_double* tmp = (dbr_time_double*)mypv->value;
    dbr_time_double valueWithTime = tmp[i];
    std::cout << "Value is: " << tmp[i].value << " Time is: " << valueWithTime.stamp.secPastEpoch << "."
              << valueWithTime.stamp.nsec << std::endl;
    dbr_double_t* tmp1 = (dbr_double_t*)dbr_value_ptr(mypv->value, mypv->dbrType);
    dbr_double_t value = tmp1[i];
    std::cout << "Value is: " << value << std::endl;
  }
  ca_clear_channel(mypv->chid);
  ca_context_destroy();
  return 0;
}
