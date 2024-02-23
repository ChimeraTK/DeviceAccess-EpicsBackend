// SPDX-FileCopyrightText: Helmholtz-Zentrum Dresden-Rossendorf, FWKE, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#include "EPICSTypes.h"

#include <cadef.h>
#include <iostream>
#include <string>
static epicsTimeStamp tsStart;

template<typename T>
void print(size_t i, void* valueRaw, long type) {
  T* tmp1 = (T*)dbr_value_ptr(valueRaw, type);
  if constexpr(std::is_array<T>::value) {
    //    T value = tmp1;
    std::cout << "Value is: " << (char*)tmp1 << std::endl;
  }
  else {
    T value = tmp1[i];
    std::cout << "Value is: " << value << std::endl;
  }
}

template<typename T>
void printTime(size_t i, void* valueRaw) {
  T* tmp = (T*)valueRaw;
  T valueWithTime = tmp[i];
  std::cout << "Value is: " << tmp[i].value << " Time is: " << valueWithTime.stamp.secPastEpoch << "."
            << valueWithTime.stamp.nsec << std::endl;
}

int main(int argc, char* argv[]) {
  if(argc != 2) {
    std::cout << "Usage: testRead [pvname]" << std::endl;
    return 1;
  }
  std::string pvName(argv[1]);
  std::cout << "Reding pv: " << pvName.c_str() << std::endl;
  auto result = ca_context_create(ca_disable_preemptive_callback);
  if(result != ECA_NORMAL) {
    std::cerr << "CA error " << ca_message(result)
              << " occurred while trying "
                 "to start channel access"
              << std::endl;
    return 1;
  }
  pv* mypv = (pv*)calloc(1, sizeof(pv));
  mypv->name = (char*)pvName.c_str();
  epicsTimeGetCurrent(&tsStart);
  result = ca_create_channel(mypv->name, 0, &mypv, default_ca_priority, &mypv->chid);
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

  unsigned long nElems = ca_element_count(mypv->chid);
  mypv->dbfType = ca_field_type(mypv->chid);
  mypv->dbrType = dbf_type_to_DBR_TIME(mypv->dbfType);
  if(ca_state(mypv->chid) == cs_conn) {
    mypv->nElems = nElems;
    mypv->value = calloc(1, dbr_size_n(mypv->dbrType, mypv->nElems));
    if(!mypv->value) {
      fprintf(stderr, "Memory allocation failed\n");
      return 1;
    }
    if(nElems == 1)
      result = ca_get(mypv->dbrType, mypv->chid, mypv->value);
    else
      result = ca_array_get(mypv->dbrType, mypv->nElems, mypv->chid, mypv->value);
    result = ca_pend_io(1);
    if(result == ECA_TIMEOUT) std::cerr << "Read operation timed out: some PV data was not read." << std::endl;
  }
  else {
    std::cerr << "No connection..." << std::endl;
    return 1;
  }
  unsigned base_type = mypv->dbfType % (LAST_TYPE + 1);
  for(size_t i = 0; i < mypv->nElems; i++) {
    switch(base_type) {
      case DBR_STRING:
        print<dbr_string_t>(i, mypv->value, mypv->dbrType);
        if(i == 0) printTime<dbr_time_string>(i, mypv->value);
        break;
      case DBR_FLOAT:
        print<dbr_float_t>(i, mypv->value, mypv->dbrType);
        if(i == 0) printTime<dbr_time_float>(i, mypv->value);
        break;
      case DBR_DOUBLE:
        print<dbr_double_t>(i, mypv->value, mypv->dbrType);
        if(i == 0) printTime<dbr_time_double>(i, mypv->value);
        break;
      case DBR_CHAR:
        print<dbr_char_t>(i, mypv->value, mypv->dbrType);
        if(i == 0) printTime<dbr_time_char>(i, mypv->value);
        break;
      case DBR_SHORT:
        print<dbr_short_t>(i, mypv->value, mypv->dbrType);
        if(i == 0) printTime<dbr_time_short>(i, mypv->value);
        break;
      case DBR_LONG:
        print<dbr_long_t>(i, mypv->value, mypv->dbrType);
        if(i == 0) printTime<dbr_time_long>(i, mypv->value);
        break;
      case DBR_ENUM:
        print<dbr_enum_t>(i, mypv->value, mypv->dbrType);
        if(i == 0) printTime<dbr_time_enum>(i, mypv->value);
        //
        //      	if(dbr_type_is_GR(mypv->dbfType)) {
        //          print<dbr_enum_t>(i, mypv->value, mypv->dbrType);
        //          if(i == 0) printTime<dbr_time_enum>(i, mypv->value);
        //        }
        //        else if(dbr_type_is_CTRL(mypv->dbfType)) {
        //          print<dbr_enum_t>(i, mypv->value, mypv->dbrType);
        //          if(i == 0) printTime<dbr_time_enum>(i, mypv->value);
        //        }
        //        else {
        //          print<int>(i, mypv->value, mypv->dbrType);
        //          if(i == 0) printTime<dbr_time_enum>(i, mypv->value);
        //        }
        break;
      default:
        throw std::runtime_error(std::string("Type ") + std::to_string(mypv->dbfType) + " not implemented.");
        break;
    }

    //    dbr_time_double* tmp = (dbr_time_double*)mypv->value;
    //    dbr_time_double valueWithTime = tmp[i];
    //    std::cout << "Value is: " << tmp[i].value << " Time is: " << valueWithTime.stamp.secPastEpoch << "."
    //              << valueWithTime.stamp.nsec << std::endl;
    //    dbr_double_t* tmp1 = (dbr_double_t*)dbr_value_ptr(mypv->value, mypv->dbrType);
    //    dbr_double_t value = tmp1[i];
    //    std::cout << "Value is: " << value << std::endl;
  }
  ca_clear_channel(mypv->chid);
  ca_context_destroy();
  return 0;
}
