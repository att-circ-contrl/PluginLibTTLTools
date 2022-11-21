#ifndef TTLTOOLS_LOGIC_H_DEFINED
#define TTLTOOLS_LOGIC_H_DEFINED

// This is intended to be included via "TTLTools.h", rather than included manually.


// Magic constant: maximum number of pending TTL events in a single bit-line.
// Making this a power of 2 _should_ be faster but isn't vital.
#define TTLTOOLSLOGIC_EVENT_BUF_SIZE 16384


// Class declarations.
namespace TTLTools
{
	// Parent class for buffered TTL handling.
	class COMMON_LIB LogicFIFO
	{
	public:
		// Constructor.
		LogicFIFO();
		// Default destructor is fine.


		// Accessors.

		// Setup.
		virtual void clearBuffer();
		virtual void setPrevInput(int64 resetTime, bool newInput, int newTag = 0);

		// Input processing.
		virtual void handleInput(int64 inputTime, bool inputLevel, int inputTag = 0);
		virtual void advanceToTime(int64 newTime);

		// Alternate input method: Have it pull from another FIFO the same way merger objects do.
		// This calls handleInput() to process events that it pulls.
		virtual void pullFromFIFOUntil(LogicFIFO *source, int64 newTime);

		// State accessors.

		bool hasPendingOutput();
		int64 getNextOutputTime();
		bool getNextOutputLevel();
		int getNextOutputTag();
		void acknowledgeOutput();

		// This acknowledges and discards output up to and including the specified timestamp.
		void drainOutputUntil(int64 newTime);

		int64 getLastInputTime();
		bool getLastInputLevel();
		int getLastInputTag();

		int64 getLastAcknowledgedTime();
		bool getLastAcknowledgedLevel();
		int getLastAcknowledgedTag();

		// Copy-by-value accessor. This is used for splitting output.
		LogicFIFO* getCopyByValue();

	protected:
		CircBuf<int64,TTLTOOLSLOGIC_EVENT_BUF_SIZE> pendingOutputTimes;
		CircBuf<bool,TTLTOOLSLOGIC_EVENT_BUF_SIZE> pendingOutputLevels;
		CircBuf<int,TTLTOOLSLOGIC_EVENT_BUF_SIZE> pendingOutputTags;

		int64 prevInputTime;
		bool prevInputLevel;
		int prevInputTag;

		int64 prevAcknowledgedTime;
		bool prevAcknowledgedLevel;
		int prevAcknowledgedTag;

		void enqueueOutput(int64 newTime, bool newLevel, int newTag);
	};


	// Merging of multiple FIFO outputs.
	// This works by pulling, to avoid needing input buffers.
	// The base class implements features shared by the multiplexer and the logical merger.
	class COMMON_LIB MergerBase : public LogicFIFO
	{
	public:
		// Constructor.
		MergerBase();
		// Default destructor is fine.

		// Accessors.
		// NOTE - Do not call the LogicFIFO input accessors. Call advanceToNextTime() instead.

		void clearInputList();
		// Whether id tags are used as event tags is up to the child class.
		void addInput(LogicFIFO* newInput, int idTag = 0);

		void clearBuffer() override;

		// This finds the earliest timestamp in the still-pending input, and acknowledges all input up to that point. It returns false if there's no input.
		bool advanceToNextTime();
		int64 getCurrentInputTime();
		bool inputTimeValid();

	protected:
		Array<LogicFIFO*> inputList;
		Array<int> inputTags;
		int64 earliestTime;
		bool isValid;
	};


	// Merging of multiple FIFO outputs.
	// This works by pulling, to avoid needing input buffers.
	// This is a multiplexer, combining several input streams into an in-order output stream.
	// Output events are tagged with the input stream's ID tag (input event tags are discarded).
	class COMMON_LIB MuxMerger : public MergerBase
	{
	public:
		// Constructor.
		MuxMerger();
		// Default destructor is fine.

		// Accessors.
		// NOTE - Do not call the LogicFIFO input accessors. Call processPendingInput() instead.

		void processPendingInputUntil(int64 newTime);

	protected:
	};


	// Merging of multiple FIFO outputs.
	// This works by pulling, to avoid needing input buffers.
	// This performs a boolean AND or OR operation on its inputs, returning a single output.
	// We're stripping input tags, since there isn't a 1:1 relation between input and output events.
	class COMMON_LIB LogicMerger : public MergerBase
	{
	public:
		enum MergerType
		{
			mergeAnd = 0,
			mergeOr = 1
		};

		// Constructor.
		LogicMerger();
		// Default destructor is fine.

		// Accessors.
		// NOTE - Do not call the LogicFIFO input accessors. Call processPendingInput() instead.

		void setMergeMode(MergerType newMode);
		void processPendingInputUntil(int64 newTime);

	protected:
		MergerType mergeMode;
	};
}

#endif


// This is the end of the file.
