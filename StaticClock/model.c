#include <float.h>  // for DBL_EPSILON
#include <math.h>   // for fabs()
#include "config.h"
#include "model.h"


void setStartValues(ModelInstance *comp) {
    M(counter) = 1;
	M(step) = 1;
    M(inClock1Interval) = 1.0;
    M(inClock1Shift) = 0.0;
}

Status calculateValues(ModelInstance *comp) {
    UNUSED(comp);
    return OK;
}

Status getContinuousStates(ModelInstance* comp, double x[], size_t nx) {

    UNUSED(nx);

    M(step) += 1;
    x[0] = M(step);
    return OK;
}

Status setContinuousStates(ModelInstance* comp, const double x[], size_t nx) {
    UNUSED(comp);
    UNUSED(nx);
    UNUSED(x);
    return OK;
}

size_t getNumberOfContinuousStates(ModelInstance* comp) {
    UNUSED(comp);
    return 1;
}

Status getDerivatives(ModelInstance* comp, double dx[], size_t nx) {
    UNUSED(comp);
    UNUSED(dx);
    UNUSED(nx);
    return OK;
}

Status getFloat64(ModelInstance* comp, ValueReference vr, double values[], size_t nValues, size_t* index) {

    ASSERT_NVALUES(1);

    switch (vr) {
    case vr_time:
        values[(*index)++] = comp->time;
        return OK;
    case vr_step:
        values[(*index)++] = M(step);
        return OK;
    default:
        logError(comp, "Get Float64 is not allowed for value reference %u.", vr);
        return Error;
    }
}

Status getInt32(ModelInstance* comp, ValueReference vr, int32_t values[], size_t nValues, size_t* index) {

    ASSERT_NVALUES(1);
    //calculateValues(comp);

    switch (vr) {
        case vr_counter:
            values[(*index)++] = M(counter);
            return OK;
        default:
            logError(comp, "Get Int32 is not allowed for value reference %u.", vr);
            return Error;
    }
}

Status getClock(ModelInstance* comp, ValueReference vr, _Bool* value) {
    switch (vr) {
    case vr_inClock1:
        *value = M(inClock1);
        return OK;
    default:
        logError(comp, "Get Clock is not allowed for value reference %u.", vr);
        return Error;
    }
}

Status setClock(ModelInstance* comp, ValueReference vr, const _Bool* value) {
    switch (vr) {
    case vr_inClock1:
        M(inClock1) = *value;
        return OK;
    default:
        logError(comp, "Get Clock is not allowed for value reference %u.", vr);
        return Error;
    }
}

Status getInterval(ModelInstance* comp, ValueReference vr, double* interval, int* qualifier) {
        switch (vr) {
        case vr_inClock1:
            *interval = M(inClock1Interval);
            *qualifier = 0;
            return OK;
        default:
            logError(comp, "Get Interval is not allowed for value reference %u.", vr);
            return Error;
        }
}

Status getShift(ModelInstance* comp, ValueReference vr, double* shift) {
    switch (vr) {
    case vr_inClock1:
        *shift = M(inClock1Shift);
        return OK;
    default:
        logError(comp, "Get Shift is not allowed for value reference %u.", vr);
        return Error;
    }
}

Status eventUpdate(ModelInstance *comp) {

    if (M(inClock1) == true) {
        M(counter)++;
        M(inClock1) = false;
    }

    comp->valuesOfContinuousStatesChanged   = false;
    comp->nominalsOfContinuousStatesChanged = false;
    comp->terminateSimulation               = M(counter) >= 10;
    comp->nextEventTimeDefined              = false;

    return OK;
}
