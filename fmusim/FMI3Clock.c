#include "FMI3Clock.h"
#include "FMI3.h"

#define CALL(f) do { FMIStatus status = f; if (status != FMIOK) return status; } while (false)

// Helper function to calculate the Greatest Common Divisor (GCD)
unsigned long long gcd(unsigned long long a, unsigned long long b) {
    while (b != 0) {
        unsigned long long t = b;
        b = a % b;
        a = t;
    }
    return a;
}

// Function to calculate local synchronization point
double calculateLocalSyncPoint(double currentTime, double modelStep, ClockCollection* clockCollection) {
    unsigned long long hcf = gcd((unsigned long long)(currentTime * 1000000), (unsigned long long)(modelStep * 1000000));
    for (size_t i = 0; i < clockCollection->count; ++i) {
        if (clockCollection->clocks[i].intervalVariability != FMIIVTriggered) {
            hcf = gcd(hcf, (unsigned long long)(clockCollection->clocks[i].nextTick * 1000000));
        }
    }
    double syncPoint = ((double)hcf) / 1000000;
    return syncPoint;
}

FMIStatus initializeClockArray(FMIInstance* instance, ClockCollection* clockCollection, double startTime) {
    for (size_t i = 0; i < clockCollection->count; ++i) {
        if (clockCollection->clocks[i].intervalVariability == FMIIVConstant) {
            const fmi3ValueReference valueReferences[1] = { clockCollection->clocks[i].valueReference };
            size_t nValueReferences = 1;
            fmi3Float64 intervalDecimals[1];
            fmi3Float64 shiftDecimals[1];
            fmi3IntervalQualifier qualifiers[1];

            CALL(FMI3GetIntervalDecimal(instance, valueReferences, nValueReferences, intervalDecimals, qualifiers));
            CALL(FMI3GetShiftDecimal(instance, valueReferences, nValueReferences, shiftDecimals, qualifiers));

            if (intervalDecimals[0] != clockCollection->clocks[i].intervalDecimal || shiftDecimals[0] != clockCollection->clocks[i].shiftDecimal) {
                return FMIError;
            }

            clockCollection->clocks[i].nextTick = startTime + clockCollection->clocks[i].shiftDecimal;
        }
    }

    return FMIOK;
}

// Updates clocks in event mode
// This function should update all clocks as per the event mode logic
// Specific implementation details to be added based on requirements
FMIStatus updateClocksForEventMode(ClockCollection* clockCollection, double time) {
    for (size_t i = 0; i < clockCollection->count; ++i) {
        if (clockCollection->clocks[i].intervalVariability == FMIIVConstant) {
            if (fabs(time - clockCollection->clocks[i].nextTick) < 0.0000001) {
                clockCollection->clocks[i].nextTick += clockCollection->clocks[i].intervalDecimal;
            }
        }
    }

    return FMIOK;
}

// Retrieves the next clock event time
// Returns the time for the next clock event based on the clocks array
// Implementation depends on the specifics of how clock events are determined
double getNextClockEventTime(ClockCollection* clockCollection) {
    return clockCollection->modelNextEventTime;
}