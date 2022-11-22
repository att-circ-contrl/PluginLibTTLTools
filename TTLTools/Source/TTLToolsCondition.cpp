#include "TTLTools.h"
#define LOGICDEBUGPREFIX "[TTLToolsCond] "
#define LOGICDEBUGIDVARIABLE debugID
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
    switch (desiredFeature)
    {
    case ConditionConfig::levelLow: break;
    case ConditionConfig::edgeRising: break;
    case ConditionConfig::edgeFalling: break;
    default:
        desiredFeature = ConditionConfig::levelHigh;
        break;
    }

    if (sustainSamps < 1)
        sustainSamps = 1;

    if (deglitchSamps < 0)
        deglitchSamps = 0;

    // We can't have a delay shorter than the "input stable for" time without being able to see the future.
    if (delayMinSamps < deglitchSamps)
        delayMinSamps = deglitchSamps;

    if (delayMaxSamps < delayMinSamps)
        delayMaxSamps = delayMinSamps;

    // Re-trigger interval has to be at least (delay + sustain) to avoid overlapping pulses.
    if (deadTimeSamps < (delayMaxSamps + sustainSamps))
        deadTimeSamps = delayMaxSamps + sustainSamps;
}



//
// Condition processing for one TTL signal.


// Constructor.
ConditionProcessor::ConditionProcessor()
{
    // The constructor should already have done this, but do it anyways.
    config.clear();

    // Initialize. Use a dummy timestamp and input level.
    setPrevInput(LOGIC_TIMESTAMP_BOGUS, false);
    clearBuffer();
    resetTrigger();
}


// Configuration accessors.

void ConditionProcessor::setConfig(ConditionConfig &newConfig)
{
    config = newConfig;
    clearBuffer();
    resetTrigger();
}


ConditionConfig ConditionProcessor::getConfig()
{
    return config;
}


// Buffer reset. This clears queued output and sets past output to the "not asserted" level.
void ConditionProcessor::clearBuffer()
{
    LogicFIFO::clearBuffer();

    // Adjust idle output to reflect configuration.
    prevAcknowledgedLevel = !(config.outputActiveHigh);
}


// Condition-processing history reset.
void ConditionProcessor::resetTrigger()
{
    // Clear trigger processing state.
    nextStableTime = LOGIC_TIMESTAMP_BOGUS;
    nextReadyTime = LOGIC_TIMESTAMP_BOGUS;
}


// Input processing. This schedules future output in response to input events.
// NOTE - This strips tags, since there isn't a 1:1 mapping between input and output events.
void ConditionProcessor::handleInput(int64 inputTime, bool inputLevel, int inputTag)
{
// FIXME - Diagnostics. Spammy!
//L_PRINT("handleInput() got input " << (inputLevel ? 1 : 0) << " at time " << inputTime << ".");

#if LOGICDEBUG_BYPASSCONDITION
    LogicFIFO::handleInput(inputTime, inputLevel, inputTag);
#else

    checkPhantomEventsUntil(inputTime);
    checkForTrigger(inputTime, inputLevel);

#endif
}


// Input processing. This advances the internal time to the specified timestamp.
void ConditionProcessor::advanceToTime(int64 newTime)
{
#if LOGICDEBUG_BYPASSCONDITION
    // Do nothing; we're a FIFO for testing purposes.
#else
    checkPhantomEventsUntil(newTime);
#endif
}


// This checks to see if trigger conditions are met and enqueues an output pulse if so.
// The idea is to call this for both real and phantom events.
// This returns true if "nextStableTime" or "nextReadyTime" changed.
bool ConditionProcessor::checkForTrigger(int64 thisTime, bool thisLevel)
{
    bool hadTimeChange = false;

    // Detect edges.
    bool haveRising = (thisLevel && (!prevInputLevel));
    bool haveFalling = ((!thisLevel) && prevInputLevel);

    // Figure out if the signal is stable and if we're still in dead time.
    bool isStable = ( thisTime >= nextStableTime );
    bool isReady = ( thisTime >= nextReadyTime );

    // Record any edge that we just saw.
    if (haveRising || haveFalling)
    {
        nextStableTime = thisTime + config.deglitchSamps;
        hadTimeChange = true;
    }

// FIXME - Diagnostics. Very spammy!
//L_PRINT( "Input " << (thisLevel ? "high" : "low") << " at " << thisTime << " rise: " << (haveRising ? "Y" : "n") << "  fall: " << (haveFalling ? "Y" : "n") << "  Stable: " << (isStable ? "Y" : "n") << "  Rdy: " << (isReady ? "Y" : "n") );

    // If we meet the assert conditions, assert.
    if (isStable && isReady)
    {
        bool wantAssert = false;
        switch (config.desiredFeature)
        {
        case ConditionConfig::levelHigh:
            wantAssert = thisLevel; break;
        case ConditionConfig::levelLow:
            wantAssert = !thisLevel; break;
        case ConditionConfig::edgeRising:
            wantAssert = haveRising; break;
        case ConditionConfig::edgeFalling:
            wantAssert = haveFalling; break;
        default:
            break;
        }

        if (wantAssert)
        {
            // We're past the dead time from our previous trigger; schedule a new output pulse.

            nextReadyTime = thisTime + config.deadTimeSamps;
            hadTimeChange = true;

            int64 thisDelay = rng.nextInt64();
            // Avoid taking the modulo of a negative number, since some implementations give a negative result for that.
            if (thisDelay < 0)
                thisDelay = -(thisDelay + 1);
            thisDelay %= (1 + config.delayMaxSamps - config.delayMinSamps);
            thisDelay += config.delayMinSamps;
// FIXME - Diagnostics. Still spammy.
L_PRINT("Pulsing " << (config.outputActiveHigh ? "high" : "low") << " from " << (thisTime + thisDelay) << " to " << (thisTime + thisDelay + config.sustainSamps) << " (trigger " << thisTime << ").");

            enqueueOutput(thisTime + thisDelay, config.outputActiveHigh, 0);
            enqueueOutput(thisTime + thisDelay + config.sustainSamps, !(config.outputActiveHigh), 0);
        }
    }

    // Update the "last input seen" record.
    setPrevInput(thisTime, thisLevel);

    return hadTimeChange;
}


// This checks for phantom events (becoming stable, becoming ready) up to the specified time.
void ConditionProcessor::checkPhantomEventsUntil(int64 newTime)
{
    // Outside of the ready period, ignore "becoming stable" events.
    // Inside of the ready period, check for them.
    // Becoming stable can only happen once, but re-triggering can happen repeatedly.
    // "prevInputTime" and "prevInputLevel" hold state for the last point we checked.

    bool hadChange = true;
    // We need to be both ready and stable for anything to happen.
    while ( hadChange && (nextReadyTime <= newTime) && (nextStableTime <= newTime) )
    {
        // There are 6 permutations of the ordering of "became stable", "became ready", and "previous time checked".
        // xxP -> already checked; nothing to do.
        // xxR -> only became ready now; check ready.
        // RPS -> check stable.
        // PRS -> check ready, then stable (on the next iteration).
        // ...So, aside from "P last; done", the only non-ready case is "RPS".

        if (nextReadyTime <= prevInputTime)
        {
            if (nextStableTime <= prevInputTime)
                // Already checked both; nothing to do.
                hadChange = false;
            else
                // Already checked "ready"; check "stable".
                hadChange = checkForTrigger(nextStableTime, prevInputLevel);
        }
        else
            // Check "ready". Becoming stable before becoming ready still only triggers when ready.
            hadChange = checkForTrigger(nextReadyTime, prevInputLevel);
    }
}


// This is the end of the file.
