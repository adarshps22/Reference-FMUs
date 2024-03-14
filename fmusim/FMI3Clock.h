#ifndef CLOCK_MANAGER_H
#define CLOCK_MANAGER_H

#include <stdint.h>
#include <stdbool.h>

#include "FMIModelDescription.h"

// Structure for Clock
typedef struct {
    double intervalDecimal;                         // Interval in decimal
    double shiftDecimal;                            // Shift in decimal
    bool supportsFraction;                          // Indicates if the clock supports fraction
    unsigned int resolution;                        // Clock resolution
    unsigned int intervalCounter;                   // Interval counter
    unsigned int shiftCounter;                      // Shift counter
    char* name;                                     // Name of the clock
    unsigned int valueReference;                    // Index of the clock
    double nextTick;                                // Next clock tick time
    FMIIntervalVariability intervalVariability;     // Variability of the interval
} Clock;

// Structure for managing a collection of Clocks and the next event time
typedef struct {
    Clock* clocks;                   // Pointer to an array of Clocks
    size_t count;                    // Number of clocks in the array
    double modelNextEventTime;      // The next event time for the entire system or subset of clocks
} ClockCollection;

// Calculates the local synchronization point for clocks
// Takes time, mode step, and an array of Clock structures along with their count
double calculateLocalSyncPoint(double currentTime, double modelStep, ClockCollection* clocks);

// Initializes an array of Clock structures
// Takes an array of Clocks and their count to perform initialization
FMIStatus initializeClockArray(FMIInstance* instance, ClockCollection* clockCollection, double startTime);

// Updates clocks in event mode
// This function should update all clocks as per the event mode logic
// Specific implementation details to be added based on requirements
FMIStatus updateClocksForEventMode(ClockCollection* clockCollection);

// Retrieves the next clock event time
// Returns the time for the next clock event based on the clocks array
// Implementation depends on the specifics of how clock events are determined
double getNextClockEventTime(ClockCollection* clockCollection);

#endif // CLOCK_MANAGER_H