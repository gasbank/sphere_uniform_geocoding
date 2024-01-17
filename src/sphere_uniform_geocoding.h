#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#if _WIN32
#include <windows.h>
#else
#include <pthread.h>
#include <unistd.h>
#endif

#if _WIN32
#define FFI_PLUGIN_EXPORT __declspec(dllexport)
#else
#define FFI_PLUGIN_EXPORT
#endif


typedef struct
{
    double x;
    double y;
    double z;
} Vector3;

typedef struct
{
    int neighborSegId[12];
    int count;
} NeighborSegIdList;

typedef struct
{
    int segGroup;
    int localSegIndex;
} SegGroupAndLocalSegIndex;

typedef struct {
    double lat;
    double lng;
} GpsCoords;

typedef struct
{
    GpsCoords points[3];
} SegmentCornersInLatLng;

// A very short-lived native function.
//
// For very short-lived functions, it is fine to call them on the main isolate.
// They will block the Dart execution while running the native function, so
// only do this for native functions which are guaranteed to be short-lived.
FFI_PLUGIN_EXPORT intptr_t sum(intptr_t a, intptr_t b);

FFI_PLUGIN_EXPORT int CalculateSegmentIndexFromLatLng(int n, double lat, double lng);
FFI_PLUGIN_EXPORT double CalculateSegmentCenterLat(int n, int segmentIndex);
FFI_PLUGIN_EXPORT double CalculateSegmentCenterLng(int n, int segmentIndex);
FFI_PLUGIN_EXPORT Vector3 CalculateSegmentCenter(int n, int segmentIndex);
FFI_PLUGIN_EXPORT NeighborSegIdList GetNeighborsOfSegmentIndex(int n, int segmentIndex);
FFI_PLUGIN_EXPORT int ConvertToSegmentIndex2(int n, int segmentGroupIndex, int localSegmentIndex);
FFI_PLUGIN_EXPORT SegGroupAndLocalSegIndex SplitSegIndexToSegGroupAndLocalSegmentIndex(int n, int segmentIndex);
FFI_PLUGIN_EXPORT SegmentCornersInLatLng CalculateSegmentCornersInLatLng(int n, int segmentIndex);

// A longer lived native function, which occupies the thread calling it.
//
// Do not call these kind of native functions in the main isolate. They will
// block Dart execution. This will cause dropped frames in Flutter applications.
// Instead, call these native functions on a separate isolate.
FFI_PLUGIN_EXPORT intptr_t sum_long_running(intptr_t a, intptr_t b);
