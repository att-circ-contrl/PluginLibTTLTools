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
    resetState();
}


ConditionConfig ConditionProcessor::getConfig()
{
    return config;
}


// State reset. This clears input and output and resets condition state.
void ConditionProcessor::resetState()
{
    LogicFIFO::resetState();
    inputBuffer.resetState();

    // Adjust idle output to reflect configuration.
    prevAcknowledgedLevel = !(config.outputActiveHigh);

    // Clear trigger processing state.
    waitingForTrig = true;
    prevTrigTime = LOGIC_TIMESTAMP_BOGUS;
}


// This overwrites our record of the previous input levels without causing an event update.
// NOTE - This does not clear the input FIFO. Use resetState() for that.
// This is used for initialization.
void ConditionProcessor::resetInput(int64 resetTime, bool newInput, int newTag)
{
    inputBuffer.resetInput(resetTime, newInput, newTag);
    LogicFIFO::resetInput(resetTime, newInput, newTag);
}


// Input processing. This schedules future output in response to input events.
// NOTE - This strips tags, since there isn't a 1:1 mapping between input and output events.
// NOTE - Since we need to know the future to do deglitching, this enqueues events into an input buffer and leaves processing to advanceToTime().
void ConditionProcessor::handleInput(int64 inputTime, bool inputLevel, int inputTag)
{
// FIXME - Diagnostics. Spammy!
//L_PRINT("handleInput() got input " << (inputLevel ? 1 : 0) << " at time " << inputTime << ".");

   inputBuffer.handleInput(inputTime, inputLevel, inputTag);
}


// Input processing. This advances the internal time to the specified timestamp.
void ConditionProcessor::advanceToTime(int64 newTime)
{
#if LOGICDEBUG_BYPASSCONDITION
    // Just be a FIFO for testing purposes.
    while (inputBuffer.hasPendingOutput())
    {
        LogicFIFO::handleInput( inputBuffer.getNextOutputTime(), inputBuffer.getNextOutputLevel(), inputBuffer.getNextOutputTag() );
        inputBuffer.acknowledgeOutput();
    }
#else

    // FIXME - advanceToTime NYI.

    // Update the "last input seen" record.
//    resetInput(inputTime, inputLevel, inputTag);

#endif
}


// This is the end of the file.
