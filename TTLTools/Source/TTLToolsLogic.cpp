#include "TTLTools.h"
#define LOGICDEBUGPREFIX "[TTLToolsLogic]  "
#include "TTLToolsDebug.h"

using namespace TTLTools;

// Private constants.

// This timestamp could happen, but we need _something_ as the default.
#define LOGIC_TIMESTAMP_BOGUS (-1)


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

    prevAcknowledgedTime = LOGIC_TIMESTAMP_BOGUS;
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

    // Copy this event to the output buffer.
    enqueueOutput(inputTime, inputLevel, inputTag);

    // Update the "last input seen" record.
    // Doing this after enqueue so that enqueue can check the previous state.
    resetInput(inputTime, inputLevel, inputTag);
}


// Input processing. This advances the internal time to the specified timestamp.
void LogicFIFO::advanceToTime(int64 newTime)
{
    // Nothing to do for the base class.
}


// Input processing. This pulls from another FIFO the same way merger classes do, calling handleInput() to process pulled events.
// Events with the same timestamp are merged (only the last event is forwarded).
void LogicFIFO::pullFromFIFOUntil(LogicFIFO *source, int64 newTime)
{
    bool hadInput = true;

    if (NULL != source)
        while (hadInput)
        {
            hadInput = false;
            if (source->hasPendingOutput())
            {
                int64 thisTime = source->getNextOutputTime();
                if (thisTime <= newTime)
                {
                    hadInput = true;

                    // Acknowledge everything with this timestamp, so that we're only dealing with the last relevant event.
                    while ( (source->hasPendingOutput()) && ( (source->getNextOutputTime()) == thisTime ) )
                        source->acknowledgeOutput();

                    handleInput(thisTime, source->getLastAcknowledgedLevel(), source->getLastAcknowledgedTag());
                }
            }
        }
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
    if (hasPendingOutput())
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
}


// This acknowledges and discards output up to and including the specified timestamp.
void LogicFIFO::drainOutputUntil(int64 newTime)
{
    while ( hasPendingOutput() && (pendingOutputTimes.snoop() <= newTime) )
        acknowledgeOutput();
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

    // Sanity check for debugging.
    // NOTE - This may give false alarms if input wasn't initialized (reset) before enqueueOutput was called!
    if (prevInputTime >= newTime)
    {
// FIXME - This can get spammy if there's a bug that trips it!
        L_PRINT(".. WARNING - FIFO event enqueued out of order (prev time " << prevInputTime << ", new " << newTime << ").");
    }
}



//
// Merging of multiple FIFO outputs - Base class.

// This works by pulling, to avoid needing input buffers.
// The base class implements features shared by the multiplexer and the logical merger.


// Constructor.
MergerBase::MergerBase()
{
    clearInputList();

    // This timestamp might occur, but we have to initialize to something.
    earliestTime = LOGIC_TIMESTAMP_BOGUS;
    isValid = false;

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

    if (hadInput)
        isValid = true;

    // Done.
    return hadInput;
}


int64 MergerBase::getCurrentInputTime()
{
    return earliestTime;
}


bool MergerBase::inputTimeValid()
{
    return isValid;
}



//
// Merging of multiple FIFO outputs - Multiplexer.

// This works by pulling, to avoid needing input buffers.
// This is a multiplexer, combining several input streams into an in-order output stream with input identification tags.
// Output events are tagged with the input stream's ID tag (input event tags are discarded).


// Constructor.
MuxMerger::MuxMerger()
{
    // Nothing more to do. The base class constructor handled everything.
}


// Accessors.

void MuxMerger::processPendingInputUntil(int64 newTime)
{
    bool hadInput;
    int64 currentTime, thisTime;

    // Scan over all inputs, pick the oldest, and process it.
    // Only do this up to the specified time.
    hadInput = inputTimeValid();
    currentTime = getCurrentInputTime();
    while ( hadInput && (currentTime <= newTime) )
    {
        // We have pending inputs. Emit output events corresponding to the input events that just happened.
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

        // Advance to the oldest still-pending timestamp and acknowledge input to that point.
        hadInput = advanceToNextTime();
        currentTime = getCurrentInputTime();
    }
}



//
// Merging of multiple FIFO outputs - Logical merger.

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


void LogicMerger::processPendingInputUntil(int64 newTime)
{
    bool hadInput;
    bool thisOutput;

    // Scan over all inputs, pick the oldest, and process it.
    // Only do this up to the specified time.
    hadInput = inputTimeValid();
    while ( hadInput && (getCurrentInputTime() <= newTime) )
    {
        // We have pending inputs. Build a new output event based on the last acknowledged inputs.
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

        // Advance to the oldest still-pending timestamp and acknowledge input to that point.
        hadInput = advanceToNextTime();
    }
}


// This is the end of the file.
