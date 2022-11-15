#include "TTLTools.h"
#define LOGICDEBUGPREFIX "[TTLToolsCondition]  "
#include "TTLToolsDebug.h"

using namespace TTLTools;

// Private constants.

// This timestamp could happen, but we need _something_ as the default.
#define LOGIC_TIMESTAMP_BOGUS (-1)


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

    desiredFeature = ConditionConfig::levelHigh;

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

    // NOTE - We're clearing trigger processing state and pending output state but keeping the record of previous input.
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
    prevAcknowledgedLevel = !(config.outputActiveHigh);

    // Clear trigger processing state.
    waitingForTrig = true;
    prevTrigTime = LOGIC_TIMESTAMP_BOGUS;
}


// Input processing. This schedules future output in response to input events.
// NOTE - This strips tags, since there isn't a 1:1 mapping between input and output events.
void ConditionProcessor::handleInput(int64 inputTime, bool inputLevel, int inputTag)
{
// FIXME - Diagnostics. Spammy!
//L_PRINT("CondProc got input " << (inputLevel ? 1 : 0) << " at time " << inputTime << ".");

#if LOGICDEBUG_BYPASSCONDITION
    // Just be a FIFO for testing purposes.
    if (inputLevel != prevInputLevel)
        enqueueOutput(inputTime, inputLevel, 0);
#else

    if (waitingForTrig)
    {
    // See if we have a trigger event.

    bool sawEdge = (prevInputLevel != inputLevel);

    // Check for unstable input.
    if (sawTrig && sawEdge)
    {
        if ((inputTime - prevTrigTime)
    }
    if (prevInputLevel != inputLevel)
    {
        switch (config.desiredFeature)
        {
            case levelHigh:
        }
    }

    // Update the "last input seen" record.
    resetInput(inputTime, inputLevel, inputTag);
    }
    else
    {
        // We
    }

    // FIXME - handleInput NYI.

#endif

    // Update the "last input seen" record.
    resetInput(inputTime, inputLevel, inputTag);
}


// Input processing. This advances the internal time to the specified timestamp.
void ConditionProcessor::advanceToTime(int64 newTime)
{
    // Add a dummy input event so that we generate output events up to the desired time.
    if (newTime > prevInputTime)
        handleInput(newTime, prevInputLevel, prevInputTag);
}


// This is the end of the file.
