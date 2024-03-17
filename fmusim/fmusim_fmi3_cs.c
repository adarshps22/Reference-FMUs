#include <stdlib.h>
#include <math.h>
#include <windows.h>
#include <psapi.h>

#include "FMIUtil.h"

#include "fmusim_fmi3_cs.h"
#include "FMI3Clock.h"


#define CALL(f) do { status = f; if (status > FMIOK) goto TERMINATE; } while (0)

double roundToPrecision(double value, int precision) {
    double scale = pow(10.0, precision);
    return round(value * scale) / scale;
}

void printMemoryUsage() {
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        printf("Memory Usage: Current: %lu, Peak: %lu\n", pmc.WorkingSetSize, pmc.PeakWorkingSetSize);
    }
}

LARGE_INTEGER getPerformanceCounter() {
    LARGE_INTEGER li;
    QueryPerformanceCounter(&li);
    return li;
}

double getCounterDifferenceInSeconds(LARGE_INTEGER start, LARGE_INTEGER end) {
    LARGE_INTEGER frequency;
    QueryPerformanceFrequency(&frequency);
    return (double)(end.QuadPart - start.QuadPart) / (double)frequency.QuadPart;
}

static void recordIntermediateValues(
    fmi3InstanceEnvironment instanceEnvironment,
    fmi3Float64  intermediateUpdateTime,
    fmi3Boolean  intermediateVariableSetRequested,
    fmi3Boolean  intermediateVariableGetAllowed,
    fmi3Boolean  intermediateStepFinished,
    fmi3Boolean  canReturnEarly,
    fmi3Boolean* earlyReturnRequested,
    fmi3Float64* earlyReturnTime) {

    FMIInstance* instance = (FMIInstance*)instanceEnvironment;

    FMIRecorder* recorder = (FMIRecorder*)instance->userData;

    if (intermediateVariableGetAllowed) {
        FMISample(instance, intermediateUpdateTime, recorder);
    }

    *earlyReturnRequested = fmi3False;
}

size_t getClocksCount(FMIModelVariable* modelVariable, size_t variableCount) {
    size_t count = 0;
    for (int i = 0; i < variableCount; i++) {
        if (modelVariable[i].type == FMIClockType) {
            count++;
        }
    }
    return count;
}

void extractClocksFromModelVariables(FMIModelVariable* modelVariable, size_t variableCount, ClockCollection* clockCollection){
    size_t count = 0;
    for (int i = 0; i < variableCount; i++) {
        if (modelVariable[i].type == FMIClockType) {
            if (modelVariable[i].clockProperties->supportsFraction == NULL) {
                clockCollection->clocks[count].intervalDecimal = strtod(modelVariable[i].clockProperties->intervalDecimal, NULL);
                clockCollection->clocks[count].shiftDecimal = strtod(modelVariable[i].clockProperties->shiftDecimal, NULL);
                clockCollection->clocks[count].supportsFraction = false;
            }
            else {
                clockCollection->clocks[count].supportsFraction = strcmp(modelVariable[i].clockProperties->supportsFraction, "true");
                if (clockCollection->clocks[count].supportsFraction == true) {
                    clockCollection->clocks[count].resolution = strtoul(modelVariable[i].clockProperties->resolution, NULL, 10);
                    clockCollection->clocks[count].intervalCounter = strtoul(modelVariable[i].clockProperties->intervalCounter, NULL, 10);
                    clockCollection->clocks[count].shiftCounter = strtoul(modelVariable[i].clockProperties->shiftCounter, NULL, 10);
                }
            }
            clockCollection->clocks[count].name = modelVariable[i].name;
            clockCollection->clocks[count].valueReference = modelVariable[i].valueReference;                
            clockCollection->clocks[count].intervalVariability = modelVariable[i].clockProperties->intervalVariability;
        }
    }
}

FMIStatus simulateFMI3CS(FMIInstance* S,
    const FMIModelDescription * modelDescription,
    const char* resourcePath,
    FMIRecorder* recorder,
    const FMUStaticInput * input,
    const FMISimulationSettings * settings) {

    LARGE_INTEGER startTime, endTime;
    LARGE_INTEGER frequency;
    FMIStatus status = FMIOK;

    fmi3Boolean inputEvent = fmi3False;
    fmi3Boolean eventEncountered = fmi3False;
    fmi3Boolean terminateSimulation = fmi3False;
    fmi3Boolean earlyReturn = fmi3False;
    fmi3Float64 lastSuccessfulTime = settings->startTime;
    fmi3Float64 time = settings->startTime;
    fmi3Float64 nextCommunicationPoint = 0.0;
    fmi3Float64 nextRegularPoint = 0.0;
    fmi3Float64 stepSize = 0.0;
    fmi3Float64 nextInputEventTime = INFINITY;
    fmi3Boolean discreteStatesNeedUpdate = fmi3True;
    fmi3Boolean nominalsOfContinuousStatesChanged = fmi3False;
    fmi3Boolean valuesOfContinuousStatesChanged = fmi3False;
    fmi3Boolean nextEventTimeDefined = fmi3False;
    fmi3Float64 nextEventTime = INFINITY;

    fmi3ValueReference* requiredIntermediateVariables = NULL;
    size_t nRequiredIntermediateVariables = 0;
    fmi3IntermediateUpdateCallback intermediateUpdate = NULL;

    ClockCollection* clockCollection = NULL;
    CALL(FMICalloc((void**)&clockCollection, 1, sizeof(ClockCollection)));
    clockCollection->count = getClocksCount(modelDescription->modelVariables, modelDescription->nModelVariables);
    
    CALL(FMICalloc((void**)&clockCollection->clocks, clockCollection->count, sizeof(Clock)));
    extractClocksFromModelVariables(modelDescription->modelVariables, modelDescription->nModelVariables, clockCollection);

    //startTime = getPerformanceCounter();
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&startTime);

    if (settings->recordIntermediateValues) {

        nRequiredIntermediateVariables = recorder->nVariables;

        CALL(FMICalloc((void**)&requiredIntermediateVariables, nRequiredIntermediateVariables, sizeof(fmi3ValueReference)));
        
        for (size_t i = 0; i < nRequiredIntermediateVariables; i++) {
            requiredIntermediateVariables[i] = recorder->variables[i]->valueReference;
        }

        intermediateUpdate = recordIntermediateValues;

        S->userData = recorder;
    }

    CALL(FMI3InstantiateCoSimulation(S,
        modelDescription->instantiationToken,  // instantiationToken
        resourcePath,                          // resourcePath
        fmi3False,                             // visible
        fmi3False,                             // loggingOn
        settings->eventModeUsed,               // eventModeUsed
        settings->earlyReturnAllowed,          // earlyReturnAllowed
        requiredIntermediateVariables,         // requiredIntermediateVariables
        nRequiredIntermediateVariables,        // nRequiredIntermediateVariables
        intermediateUpdate                     // intermediateUpdate
    ));

    free(requiredIntermediateVariables);

    if (settings->initialFMUStateFile) {
        CALL(FMIRestoreFMUStateFromFile(S, settings->initialFMUStateFile));
    }

    CALL(applyStartValues(S, settings));
    CALL(FMIApplyInput(S, input, settings->startTime, true, true, false));

    if (!settings->initialFMUStateFile) {

        CALL(FMI3EnterInitializationMode(S, settings->tolerance > 0, settings->tolerance, settings->startTime, fmi3False, 0));
        CALL(initializeClockArray(S, clockCollection, settings->startTime));
        CALL(FMI3ExitInitializationMode(S));

        if (settings->eventModeUsed) {
            if (fabs(time - clockCollection->modelNextEventTime) < 10e-6) {
                CALL(FMIActivateClocks(S, clockCollection));
            }

            do {

                CALL(FMI3UpdateDiscreteStates(S,
                    &discreteStatesNeedUpdate,
                    &terminateSimulation,
                    &nominalsOfContinuousStatesChanged,
                    &valuesOfContinuousStatesChanged,
                    &nextEventTimeDefined,
                    &nextEventTime));

                if (terminateSimulation) {
                    goto TERMINATE;
                }

            } while (discreteStatesNeedUpdate);

            if (!nextEventTimeDefined) {
                nextEventTime = INFINITY;
            }

            if (fabs(time - clockCollection->modelNextEventTime) < 10e-6) {
                CALL(updateClocksForEventMode(S, clockCollection, time));
            }

            CALL(FMI3EnterStepMode(S));
        }
    }

    size_t nSteps = 0;

    for (;;) {

        nextRegularPoint = settings->startTime + (nSteps + 1) * settings->outputInterval;

        for (;;) {
            double localStepSize;
            CALL(calculateLocalSyncPoint(time, nextRegularPoint, clockCollection, &localStepSize));
            double modifiedRegularPoint = roundToPrecision(time + localStepSize, 9);
            nextCommunicationPoint = modifiedRegularPoint;

            CALL(FMISample(S, time, recorder));

            if (time >= settings->stopTime) {
                break;
            }

            nextInputEventTime = FMINextInputEvent(input, time);

            inputEvent = nextCommunicationPoint >= nextInputEventTime;

            if (inputEvent) {
                nextCommunicationPoint = nextInputEventTime;
            }

            stepSize = nextCommunicationPoint - time;

            CALL(FMIApplyInput(S, input, time,
                !settings->eventModeUsed,  // discrete
                true,                      // continuous
                !settings->eventModeUsed   // afterEvent
            ));

            CALL(FMI3DoStep(S,
                time,                  // currentCommunicationPoint
                stepSize,              // communicationStepSize
                fmi3True,              // noSetFMUStatePriorToCurrentPoint
                &eventEncountered,     // eventEncountered
                &terminateSimulation,  // terminate
                &earlyReturn,          // earlyReturn
                &lastSuccessfulTime    // lastSuccessfulTime
            ));

            if (earlyReturn && !settings->earlyReturnAllowed) {
                FMILogError("The FMU returned early from fmi3DoStep() but early return is not allowed.");
                status = FMIError;
                goto TERMINATE;
            }

            if (terminateSimulation) {
                break;
            }

            if (earlyReturn && lastSuccessfulTime < nextCommunicationPoint) {
                time = lastSuccessfulTime;
            }
            else {
                time = nextCommunicationPoint;
            }

            if (settings->eventModeUsed && (inputEvent || eventEncountered || fabs(time - clockCollection->modelNextEventTime) < 10e-6)) {

                CALL(FMISample(S, time, recorder));

                CALL(FMI3EnterEventMode(S));

                if (fabs(time - clockCollection->modelNextEventTime) < 10e-6) {
                    CALL(FMIActivateClocks(S, clockCollection));
                }

                CALL(FMIApplyInput(S, input, time,
                    true,  // discrete
                    true,  // continous
                    true   // after event
                ));

                if (inputEvent) {
                    CALL(FMIApplyInput(S, input, time,
                        true,  // discrete
                        true,  // continous
                        true   // after event
                    ));
                }

                do {

                    CALL(FMI3UpdateDiscreteStates(S,
                        &discreteStatesNeedUpdate,
                        &terminateSimulation,
                        &nominalsOfContinuousStatesChanged,
                        &valuesOfContinuousStatesChanged,
                        &nextEventTimeDefined,
                        &nextEventTime));

                    if (terminateSimulation) {
                        break;
                    }

                } while (discreteStatesNeedUpdate);

                if (!nextEventTimeDefined) {
                    nextEventTime = INFINITY;
                }

                if (fabs(time - clockCollection->modelNextEventTime) < 10e-6) {
                    CALL(updateClocksForEventMode(S, clockCollection, time));
                }

                CALL(FMI3EnterStepMode(S));
            }

            if (fabs(time - nextRegularPoint) < 10e-6) {
                nSteps++;
                break;
            }
            else {
                nextCommunicationPoint = time + localStepSize;
            }
        }
        if (time >= settings->stopTime || terminateSimulation) {
            break;
        }
    }

    if (settings->finalFMUStateFile) {
        CALL(FMISaveFMUStateToFile(S, settings->finalFMUStateFile));
    }

TERMINATE:

    if (status < FMIError) {

        const FMIStatus terminateStatus = FMI3Terminate(S);

        if (terminateStatus > status) {
            status = terminateStatus;
        }
    }

    if (status != FMIFatal) {
        FMI3FreeInstance(S);
    }

    //endTime = getPerformanceCounter();
    QueryPerformanceCounter(&endTime);
    //printf("Execution Time: %f seconds\n", getCounterDifferenceInSeconds(startTime, endTime));
    printf("Execution Time: %f seconds\n", (double)(endTime.QuadPart - startTime.QuadPart) / frequency.QuadPart);
    printMemoryUsage();

    return status;
}