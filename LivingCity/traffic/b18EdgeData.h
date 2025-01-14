/************************************************************************************************
*
*		LC Project - B18 Edge data
*
*		@author igaciad
*
************************************************************************************************/

#ifndef LC_B18_EDGE_DATA_H
#define LC_B18_EDGE_DATA_H

#include "stdint.h"

#ifndef ushort
#define ushort uint16_t
#endif
#ifndef uint
#define uint uint32_t
#endif
#ifndef uchar
#define uchar uint8_t
#endif

const int kMaxMapWidthM = 1024;
const uint kMaskOutEdge = 0x000000;
const uint kMaskInEdge = 0x800000;
const uint kMaskLaneMap = 0x007FFFFF;

namespace LC {

struct B18EdgeData {
  ushort numLines;
  uint nextInters;
  float length;
  float maxSpeedMperSec;  // Maximum speed of the edge
  uint nextIntersMapped;
  float curr_cum_vel = 0; // Current cumulative velocity of the edge
  float curr_iter_num_cars = 0; // Current number of cars in the edge
};

struct B18IntersectionData {
  ushort state;
  ushort stateLine;
  ushort totalInOutEdges; // total number of in and out edges
  uint edge[24];// up to six arms intersection
  float nextEvent;
};
}

#endif // LC_B18_EDGE_DATA_H
