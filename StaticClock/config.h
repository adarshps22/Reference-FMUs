#ifndef config_h
#define config_h

// define class name and unique id
#define MODEL_IDENTIFIER StaticClock
#define INSTANTIATION_TOKEN "{1EDAAE1A-3FAB-48A3-BACE-B6C2782583E6}"

#define CO_SIMULATION

#define HAS_CONTINUOUS_STATES

#define GET_INT32
#define GET_CLOCK
#define GET_INTERVAL
#define GET_SHIFT

#define SET_CLOCK
#define EVENT_UPDATE

#define FIXED_SOLVER_STEP 0.2
#define DEFAULT_STOP_TIME 10

typedef enum {
    vr_time = 0, 
    vr_step = 1,
    vr_counter = 2,
    vr_inClock1 = 1001
} ValueReference;

typedef struct {
    int counter;	
	double step;
    _Bool inClock1;
    double inClock1Interval;
    double inClock1Shift;
} ModelData;

#endif /* config_h */
