#include "TTLToolsCircBuf.h"
#include "TTLToolsLogic.h"

using namespace TTLTools;

// Diagnostic tattle macros.

// Tattle enable/disable switch.
#define LOGICWANTDEBUG 1

// Condition bypass switch. Set this to just act like a FIFO.
#define LOGICDEBUG_BYPASSCONDITION 1

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
    pendingOutputTags.clear();

    // FIXME - Bogus timestamp!
    prevAcknowledgedTime = -1;
    prevAcknowledgedLevel = false;
    prevAcknowledgedTag = 0;
}


// This overwrites our record of the previous input levels without causing an event update.
// This is used for initialization.
void LogicFIFO::resetInput(int64 resetTime, bool newInput, int newTag)
{
    prevInputTime = resetTime;
    prevInputLevel = newInput;
    prevInputTag = newTag;
}


// Input processing. For the FIFO, input events are just copied to the output.
void LogicFIFO::handleInput(int64 inputTime, bool inputLevel, int inputTag)
{
// FIXME - Diagnostics. Spammy!
//L_PRINT("FIFO got input " << (inputLevel ? 1 : 0) << " with tag " << inputTag << " at time " << inputTime << ".");

    // Update the "last input seen" record.
    resetInput(inputTime, inputLevel, inputTag);

    // Copy this event to the output buffer.
    enqueueOutput(inputTime, inputLevel, inputTag);
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


int LogicFIFO::getNextOutputTag()
{
    // NOTE - This will return a safe value (0) if we don't have output.
    return pendingOutputTags.snoop();
}


// This removes the next queued output event, after we've read it.
void LogicFIFO::acknowledgeOutput()
{
    // Save whatever the last output was.
    // These will return safe values (0 or false) if we don't have pending output.
    prevAcknowledgedTime = pendingOutputTimes.snoop();
    prevAcknowledgedLevel = pendingOutputLevels.snoop();
    prevAcknowledgedTag = pendingOutputTags.snoop();

    // Discard return values.
    pendingOutputTimes.dequeue();
    pendingOutputLevels.dequeue();
    pendingOutputTags.dequeue();
}


int64 LogicFIFO::getLastInputTime()
{
    return prevInputTime;
}


bool LogicFIFO::getLastInputLevel()
{
    return prevInputLevel;
}


int LogicFIFO::getLastInputTag()
{
    return prevInputTag;
}


int64 LogicFIFO::getLastAcknowledgedTime()
{
    return prevAcknowledgedTime;
}


bool LogicFIFO::getLastAcknowledgedLevel()
{
    return prevAcknowledgedLevel;
}


int LogicFIFO::getLastAcknowledgedTag()
{
    return prevAcknowledgedTag;
}


// Copy-by-value accessor. This is used for splitting output.
// The idea is that we don't need to know the type of a derived class to get a basic FIFO with a copy of that object's output.

LogicFIFO* LogicFIFO::getCopyByValue()
{
    LogicFIFO* result = new LogicFIFO;

    // All of our internal data fields, including buffers, can be copied by value.

    result->pendingOutputTimes = pendingOutputTimes;
    result->pendingOutputLevels = pendingOutputLevels;
    result->pendingOutputTags = pendingOutputTags;

    result->prevInputTime = prevInputTime;
    result->prevInputLevel = prevInputLevel;
    result->prevInputTag = prevInputTag;

    result->prevAcknowledgedTime = prevAcknowledgedTime;
    result->prevAcknowledgedLevel = prevAcknowledgedLevel;
    result->prevAcknowledgedTag = prevAcknowledgedTag;

    return result;
}


// Protected accessors.

void LogicFIFO::enqueueOutput(int64 newTime, bool newLevel, int newTag)
{
    pendingOutputTimes.enqueue(newTime);
    pendingOutputLevels.enqueue(newLevel);
    pendingOutputTags.enqueue(newTag);
// FIXME - Spammy diagnostics.
//L_PRINT(".. fifo output enqueued for tag " << newTag << " level " << (newLevel ? 1 : 0) << " at time " << newTime << ".");
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
    prevAcknowledgedLevel = !(config.outputActiveHigh);
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



//
// Merging of multiple condition processor outputs - Base class.

// This works by pulling, to avoid needing input buffers.
// The base class implements features shared by the multiplexer and the logical merger.


// Constructor.
MergerBase::MergerBase()
{
    clearInputList();

    // Negative timestamps might exist, but we have to initialize to something.
    earliestTime = -1;

    // We have no inputs yet, so there's no need to call resetState() here.
    // The parent constructor already reset output state, and virtual tables aren't yet initialized.
}


// Accessors.

void MergerBase::clearInputList()
{
    inputList.clear();
    inputTags.clear();
}


void MergerBase::addInput(LogicFIFO* newInput, int idTag)
{
    inputList.add(newInput);
    inputTags.add(idTag);
}


void MergerBase::resetState()
{
    LogicFIFO::resetState();

    for (int inIdx = 0; inIdx < inputList.size(); inIdx++)
        if (NULL != inputList[inIdx])
            (inputList[inIdx])->resetState();
}


// This finds the earliest timestamp in the still-pending input, and acknowledges all input up to that point. It returns false if there's no input.
bool MergerBase::advanceToNextTime()
{
    bool hadInput;

    // Identify the oldest pending timestamp and record it.
    hadInput = false;
    for (int inIdx = 0; inIdx < inputList.size(); inIdx++)
        if (NULL != inputList[inIdx])
            if (inputList[inIdx]->hasPendingOutput())
            {
                int64 thisTime = inputList[inIdx]->getNextOutputTime();
                if ((!hadInput) || (thisTime < earliestTime))
                    earliestTime = thisTime;
                hadInput = true;
            }

    // Acknowledge events that match the earliest timestamp.
    // NOTE - Tolerate the case where a single source has several pending inputs with that timestamp.
    if (hadInput)
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

    // Done.
    return hadInput;
}


int64 MergerBase::getCurrentInputTime()
{
    return earliestTime;
}



//
// Merging of multiple condition processor outputs - Multiplexer.

// This works by pulling, to avoid needing input buffers.
// This is a multiplexer, combining several input streams into an in-order output stream with input identification tags.
// Output events are tagged with the input stream's ID tag (input event tags are discarded).


// Constructor.
MuxMerger::MuxMerger()
{
    // Nothing more to do. The base class constructor handled everything.
}


// Accessors.

void MuxMerger::processPendingInput()
{
    bool hadInput;
    int64 currentTime, thisTime;

    // Scan over all inputs, pick the oldest, and process it.
    do
    {
        // Advance to the oldest still-pending timestamp and acknowledge input to that point.
        hadInput = advanceToNextTime();

        // If we had inputs, emit output events corresponding to the input events that just happened.
        if (hadInput)
        {
            currentTime = getCurrentInputTime();
            for (int inIdx = 0; inIdx < inputList.size(); inIdx++)
                if (NULL != inputList[inIdx])
                {
                    int64 thisTime = inputList[inIdx]->getLastAcknowledgedTime();
                    if (thisTime == currentTime)
                    {
                        bool thisLevel = inputList[inIdx]->getLastAcknowledgedLevel();
                        enqueueOutput(thisTime, thisLevel, inputTags[inIdx]);
                    }
                }
        }
    }
    while (hadInput);
}



//
// Merging of multiple condition processor outputs - Logical merger.

// This works by pulling, to avoid needing input buffers.
// This performs a boolean AND or OR operation on its inputs, returning a single output.
// We're stripping input tags, since there isn't a 1:1 relation between input and output events.


// Constructor.
LogicMerger::LogicMerger()
{
    mergeMode = mergeAnd;

    // The parent constructor already initialized everything else.
}


// Accessors.

void LogicMerger::setMergeMode(LogicMerger::MergerType newMode)
{
    mergeMode = newMode;
}


void LogicMerger::processPendingInput()
{
    bool hadInput;
    bool thisOutput;

    // Scan over all inputs, pick the oldest, and process it.
    do
    {
        // Advance to the oldest still-pending timestamp and acknowledge input to that point.
        hadInput = advanceToNextTime();

        // If we had input events, build a new output event based on the last acknowledged inputs.
        if (hadInput)
        {
            // Get the logical-AND or logical-OR of all acknowledged outputs.
            thisOutput = (mergeMode == mergeAnd);
            for (int inIdx = 0; inIdx < inputList.size(); inIdx++)
                if (NULL != inputList[inIdx])
                {
                    bool thisLevel = inputList[inIdx]->getLastAcknowledgedLevel();
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
            // FIXME - We're not checking to see if output actually _changed_, here.
            enqueueOutput(earliestTime, thisOutput, 0);
        }
    }
    while (hadInput);
}


// This is the end of the file.
