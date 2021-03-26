#include "EPICS_types.h"
#include <cadef.h>
#include <string.h>
#include <iostream>
static epicsTimeStamp tsStart;
static int nConn = 0;           /* Number of connected PVs */
static int nRead = 0;           /* Number of channels that were read */


static void event_handler (evargs args)
{
    pv* ppv = (pv*)args.usr;

    ppv->status = args.status;
    if (args.status == ECA_NORMAL)
    {
        ppv->dbrType = args.type;
        ppv->value   = calloc(1, dbr_size_n(args.type, args.count));
        memcpy(ppv->value, args.dbr, dbr_size_n(args.type, args.count));
        ppv->nElems = args.count;
        nRead++;
        dbr_double_t* tmp = (dbr_double_t*)dbr_value_ptr(ppv->value, ppv->dbrType);
        dbr_double_t value = tmp[0];
        std::cout << "Value is: " << value << std::endl;

    }
}

int main(){
  auto result = ca_context_create(ca_enable_preemptive_callback);
  if (result != ECA_NORMAL) {
    std::cerr << "CA error " << ca_message(result) << " occurred while trying "
             "to start channel access" << std::endl;
     return 1;
  }
  pv* mypv = (pv*)calloc(1, sizeof(pv));
  std::string pvName("test:ai1");
  mypv->name = (char*)pvName.c_str();
  epicsTimeGetCurrent(&tsStart);
  result = ca_create_channel(mypv->name, 0, &mypv, DEFAULT_CA_PRIORITY, &mypv->chid);
  if (result != ECA_NORMAL) {
      std::cerr <<  "CA error " << ca_message(result) << " occurred while trying "
              "to create channel '" << mypv->name << "'." << std::endl;
      return 1;
  }
  result = ca_pend_io(1);
  if(result == ECA_TIMEOUT){
    std::cerr << "Channel time out for PV: " << pvName << std::endl;
    return 1;
  }

  if(result != ECA_NORMAL){
    std::cerr << "CA error " << ca_message(result) << " occurred while trying to create channel " << mypv->name << std::endl;
  }
  mypv->status = result;

  unsigned long nElems         = ca_element_count(mypv->chid);
  mypv->dbfType = ca_field_type(mypv->chid);
  mypv->dbrType = dbf_type_to_DBR_TIME(mypv->dbfType);
  if (ca_state(mypv->chid) == cs_conn){
    nConn++;
    mypv->onceConnected = 1;
    mypv->nElems = nElems;
    mypv->value = calloc(1, dbr_size_n(mypv->dbrType, mypv->nElems));
    if (!mypv->value) {
        fprintf(stderr,"Memory allocation failed\n");
        return 1;
    }
    evid* id = new evid();
    ca_create_subscription(mypv->dbrType,
                           mypv->nElems,
                           mypv->chid,
                           DBE_VALUE,
                           event_handler,
                           &mypv[0],
                           id
                           );
//    result = ca_array_get_callback(mypv->dbrType,
//                          mypv->nElems,
//                          mypv->chid,
//                          event_handler,
//                          &mypv[0]);
    result = ca_flush_io();
//    if (result == ECA_TIMEOUT)
//      std::cerr <<  "Read operation timed out: some PV data was not read." << std::endl;

  } else {
    std::cerr << "No connection..." << std::endl;
    return 1;
  }
  while (true) {
    ca_pend_event(1.0);
  }
//  std::cout << "Str: " << dbr2str(mypv->value, mypv->dbrType) << std::endl;
  ca_context_destroy();
  return 0;
}