//
// Created by vm on 25.10.6.
//

#ifndef MESH_CODE_SENSORIMPL_H
#define MESH_CODE_SENSORIMPL_H

#include "hw.h"
#include "macros.h"

#define CH(X)  sensor_dataChannels[cntr++]=X;if(ch==X)
#define QUALIFIER(X) ||X==hw_id()
#define NODE(...) if(0 FOR_EACH(QUALIFIER,__VA_ARGS__) )


uint16_t get_data(uint8_t ch) {
    uint8_t cntr = 0;
#define USE

#include "sensorConf.h"

    return 0;
}

#undef NODE
#undef CH
#undef QUALIFIER

#define QUALIFIER(X) ||X==hw_id()
#define NODE(...) if(0 FOR_EACH(QUALIFIER,__VA_ARGS__) ){return true;}
#define CH(X) if(false)

bool is_sensor() {
#define USE

#include "sensorConf.h"

    return false;

}
// dummy include to enable syntax highlight
#undef USE

#include "sensorConf.h"


#endif //MESH_CODE_SENSORIMPL_H
