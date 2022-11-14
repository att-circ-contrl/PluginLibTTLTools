#ifndef TTLTOOLS_LOGIC_H_DEFINED
#define TTLTOOLS_LOGIC_H_DEFINED

#include <CommonLibHeader.h>


// Magic constant: maximum number of pending TTL events in a single bit-line.
// Making this a power of 2 _should_ be faster but isn't vital.
#define TTLTOOLSLOGIC_EVENT_BUF_SIZE 16384


// Class declarations.
namespace TTLTools
{
	// Configuration for processing conditions on one signal.
	// Nothing in here is dynamically allocated, so copy-by-value is fine.
	class COMMON_LIB ConditionConfig
	{
	public:
		enum FeatureType
		{
			levelHigh = 0,
			levelLow = 1,
			edgeRising = 2,
			edgeFalling = 3
		};

		// Configuration parameters. External editing is fine.
		FeatureType desiredFeature;
		int64 delayMinSamps, delayMaxSamps;
		int64 sustainSamps;
		int64 deadTimeSamps;
		int64 deglitchSamps;
		bool outputActiveHigh;

		// Constructor.
		ConditionConfig();
		// Default destructor is fine.

		// This sets a known-sane configuration state.
		void clear();
		// This forces configuration parameters to be valid and self-consistent.
		void forceSanity();
	};


	// Parent class for buffered TTL handling.
	class COMMON_LIB LogicFIFO
	{
	public:
		// Constructor.
		LogicFIFO();
		// Default destructor is fine.


		// Accessors.

		virtual void resetState();

		virtual void resetInput(int64 resetTime, bool newInput);
		virtual void handleInput(int64 inputTime, bool inputLevel);
		virtual void advanceToTime(int64 newTime);

		bool hasPendingOutput();
		int64 getNextOutputTime();
		bool getNextOutputLevel();
		void acknowledgeOutput();

		bool getLastInput();
		bool getLastAcknowledgedOutput();

		// Copy-by-value accessor. This is used for splitting output.
		LogicFIFO* getCopyByValue();

	protected:
		CircBuf<int64,TTLTOOLSLOGIC_EVENT_BUF_SIZE> pendingOutputTimes;
		CircBuf<bool,TTLTOOLSLOGIC_EVENT_BUF_SIZE> pendingOutputLevels;

		int64 prevInputTime;
		bool prevInputLevel;
		bool prevAcknowledgedOutput;

		void enqueueOutput(int64 newTime, bool newLevel);
	};


	// Condition processing for one TTL signal.
	class COMMON_LIB ConditionProcessor : public LogicFIFO
	{
	public:
		// Constructor.
		ConditionProcessor();
		// Default destructor is fine.

		// Accessors.

		void setConfig(ConditionConfig &newConfig);
		ConditionConfig getConfig();

		void resetState() override;
		void handleInput(int64 inputTime, bool inputLevel) override;
		void advanceToTime(int64 newTime) override;

	protected:
		ConditionConfig config;
	};


	// Merging of multiple condition outputs.
	// This works by pulling, to avoid needing input buffers.
	class COMMON_LIB LogicMerger : public LogicFIFO
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

		void clearInputList();
		void addInput(LogicFIFO* newInput);

		void setMergeMode(MergerType newMode);

		void resetState() override;
		void processPendingInput();

	protected:
		Array<LogicFIFO*> inputList;
		MergerType mergeMode;
	};
}

#endif


// This is the end of the file.
