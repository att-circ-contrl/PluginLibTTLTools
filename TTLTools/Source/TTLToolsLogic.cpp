#include "TTLToolsCircBuf.h"
#include "TTLToolsLogic.h"

using namespace TTLTools;

// Diagnostic tattle macros.

// Tattle enable/disable switch.
#define LOGICWANTDEBUG 1

// Conditional code block.
#if LOGICWANTDEBUG
#define L_DEBUG(x) do { x } while(false);
#else
#define L_DEBUG(x) {}
#endif

// Debug tattle output.
// Flushing should already happen with std::endl, but force it anyways.
#define L_PRINT(x) L_DEBUG(std::cout << "[Logic]  " << x << std::endl << std::flush;)


//
// Configuration for processing conditions on one signal.


// Constructor.
ConditionConfig::ConditionConfig()
{
    // Initialize to safe defaults.
    clear();
}


// Default destructor is fine.


// This sets a known-sane configuration state.
void ConditionConfig::clear()
{
    // Set sane defaults.

    desiredFeature = FeatureType::levelHigh;

    delayMinSamps = 0;
    delayMaxSamps = 0;
    sustainSamps = 10;
    deadTimeSamps = 100;
    deglitchSamps = 0;

    outputActiveHigh = true;
}


// This forces configuration parameters to be valid and self-consistent.
void ConditionConfig::forceSanity()
{
    if ( (desiredFeature < ConditionConfig::levelHigh) || (desiredFeature > ConditionConfig::edgeFalling) )
        desiredFeature = ConditionConfig::levelHigh;

    if (delayMinSamps < 0)
        delayMinSamps = 0;

    if (delayMaxSamps < delayMinSamps)
        delayMaxSamps = delayMinSamps;

    if (sustainSamps < 1)
        sustainSamps = 1;

    if (deadTimeSamps < 0)
        deadTimeSamps = 0;

    if (deglitchSamps < 0)
        deglitchSamps = 0;
}



//
// Base class for output buffer handling.


// Constructor
LogicFIFO::LogicFIFO()
{
    resetState();
}


// State manipulation.

// State reset. This clears pending output and sets past output to false.
void LogicFIFO::resetState()
{
    pendingOutputTimes.clear();
    pendingOutputLevels.clear();
    prevAcknowledgedOutput = false;
}


// This overwrites our record of the previous input levels without causing an event update.
// This is used for initialization.
void LogicFIFO::resetInput(int64 resetTime, bool newInput)
{
    prevInputTime = resetTime;
    prevInputLevel = newInput;
}


// Input processing. For the FIFO, input events are just copied to the output.
void LogicFIFO::handleInput(int64 inputTime, bool inputLevel)
{
// FIXME - Diagnostics. Spammy!
//L_PRINT("FIFO got input " << (inputLevel ? 1 : 0) << " at time " << inputTime << ".");

    // Update the "last input seen" record.
    resetInput(inputTime, inputLevel);

    // Copy this event to the output buffer.
    enqueueOutput(inputTime, inputLevel);
}


// Input processing. This advances the internal time to the specified timestamp.
void LogicFIFO::advanceToTime(int64 newTime)
{
    // Nothing to do for the base class.
}


// State accessors.

bool LogicFIFO::hasPendingOutput()
{
    return (pendingOutputTimes.count() > 0);
}


int64 LogicFIFO::getNextOutputTime()
{
    // NOTE - This will return a safe value (0) if we don't have output.
    return pendingOutputTimes.snoop();
}


bool LogicFIFO::getNextOutputLevel()
{
    // NOTE - This will return a safe value (false) if we don't have output.
    return pendingOutputLevels.snoop();
}


// This removes the next queued output event, after we've read it.
void LogicFIFO::acknowledgeOutput()
{
    // Save whatever the last output was.
    // This will return a safe value (false) if we don't have pending output.
    prevAcknowledgedOutput = pendingOutputLevels.snoop();

    // Discard return values.
    pendingOutputTimes.dequeue();
    pendingOutputLevels.dequeue();
}


bool LogicFIFO::getLastInput()
{
    return prevInputLevel;
}


bool LogicFIFO::getLastAcknowledgedOutput()
{
    return prevAcknowledgedOutput;
}


// Copy-by-value accessor. This is used for splitting output.
// The idea is that we don't need to know the type of a derived class to get a basic FIFO with a copy of that object's output.

LogicFIFO* LogicFIFO::getCopyByValue()
{
    LogicFIFO* result = new LogicFIFO;

    // All of our internal data fields, including buffers, can be copied by value.

    result->pendingOutputTimes = pendingOutputTimes;
    result->pendingOutputLevels = pendingOutputLevels;

    result->prevInputTime = prevInputTime;
    result->prevInputLevel = prevInputLevel;
    result->prevAcknowledgedOutput = prevAcknowledgedOutput;

    return result;
}


// Protected accessors.

void LogicFIFO::enqueueOutput(int64 newTime, bool newLevel)
{
    pendingOutputTimes.enqueue(newTime);
    pendingOutputLevels.enqueue(newLevel);
}



//
// Condition processing for one TTL signal.


// Constructor.
ConditionProcessor::ConditionProcessor()
{
    // The constructor should already have done this, but do it anyways.
    config.clear();

    // Initialize. Use a dummy timestamp and input level.
    resetInput(0, false);
    resetState();
}


// Configuration accessors.

void ConditionProcessor::setConfig(ConditionConfig &newConfig)
{
    config = newConfig;

    // NOTE - We're clearing pending output state but keeping the record of previous input.
    resetState();
}


ConditionConfig ConditionProcessor::getConfig()
{
    return config;
}


// State reset. This clears active events after a configuration change.
void ConditionProcessor::resetState()
{
    LogicFIFO::resetState();

    // Adjust idle output to reflect configuration.
    prevAcknowledgedOutput = !(config.outputActiveHigh);
}


// Input processing. This schedules future output in response to input events.
void ConditionProcessor::handleInput(int64 inputTime, bool inputLevel)
{
// FIXME - Diagnostics. Spammy!
//L_PRINT("CondProc got input " << (inputLevel ? 1 : 0) << " at time " << inputTime << ".");

// FIXME - handleInput NYI.
// Just be a FIFO for testing purposes.
enqueueOutput(inputTime, inputLevel);

    // Update the "last input seen" record.
    resetInput(inputTime, inputLevel);
}


// Input processing. This advances the internal time to the specified timestamp.
void ConditionProcessor::advanceToTime(int64 newTime)
{
    // Add a dummy input event so that we generate output events up to the desired time.
    if (newTime > prevInputTime)
        handleInput(newTime, prevInputLevel);
}



//
// Merging of multiple condition processor outputs.

// This works by pulling, to avoid needing input buffers.


// Constructor.
LogicMerger::LogicMerger()
{
    mergeMode = mergeAnd;
    clearInputList();

    // We have no inputs yet, so there's no need to call resetState() here.
    // The parent constructor already reset output state, and virtual tables aren't yet initialized.
}


// Accessors.

void LogicMerger::clearInputList()
{
    inputList.clear();
}


void LogicMerger::addInput(LogicFIFO* newInput)
{
    inputList.add(newInput);
}


void LogicMerger::setMergeMode(LogicMerger::MergerType newMode)
{
    mergeMode = newMode;
}


void LogicMerger::resetState()
{
    LogicFIFO::resetState();

    for (int inIdx = 0; inIdx < inputList.size(); inIdx++)
        if (NULL != inputList[inIdx])
            (inputList[inIdx])->resetState();
}


void LogicMerger::processPendingInput()
{
    int64 earliestTime;
    bool hadInput;
    bool thisOutput;

    // Scan over all inputs, pick the oldest, and process it.
    do
    {
        // Identify the oldest pending timestamp.
        hadInput = false;
        earliestTime = -1;
        for (int inIdx = 0; inIdx < inputList.size(); inIdx++)
            if (NULL != inputList[inIdx])
                if (inputList[inIdx]->hasPendingOutput())
                {
                    int64 thisTime = inputList[inIdx]->getNextOutputTime();
                    if ((!hadInput) || (thisTime < earliestTime))
                        earliestTime = thisTime;
                    hadInput = true;
                }

        if (hadInput)
        {
            // Acknowledge events that match the earliest timestamp.
            // NOTE - Tolerate the case where a single source has several pending inputs with that timestamp.
            for (int inIdx = 0; inIdx < inputList.size(); inIdx++)
                if (NULL != inputList[inIdx])
                {
                    bool wasEarly = true;
                    while ( wasEarly && (inputList[inIdx]->hasPendingOutput()) )
                        if (inputList[inIdx]->getNextOutputTime() <= earliestTime)
                            inputList[inIdx]->acknowledgeOutput();
                        else
                            wasEarly = false;
                }

            // Get the logical-AND or logical-OR of all acknowledged outputs.
            thisOutput = (mergeMode == mergeAnd);
            for (int inIdx = 0; inIdx < inputList.size(); inIdx++)
                if (NULL != inputList[inIdx])
                {
                    bool thisLevel = inputList[inIdx]->getLastAcknowledgedOutput();
                    switch (mergeMode)
                    {
                    case mergeAnd:
                        thisOutput = thisOutput && thisLevel;
                        break;
                    case mergeOr:
                        thisOutput = thisOutput || thisLevel;
                        break;
                    default:
                        break;
                    }
                }

            // Emit this output.
            enqueueOutput(earliestTime, thisOutput);
        }
    }
    while (hadInput);
}


// This is the end of the file.
