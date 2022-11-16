#ifndef TTLTOOLS_CONDITION_H_DEFINED
#define TTLTOOLS_CONDITION_H_DEFINED

// This is intended to be included via "TTLTools.h", rather than included manually.

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


	// Condition processing for one TTL signal.
	// NOTE - This strips tags, since there isn't a 1:1 mapping between input and output events.
	// NOTE - You'll have to call advanceToTime(), since this buffers input events for deglitching.
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
		void resetInput(int64 resetTime, bool newInput, int newTag = 0) override;
		void handleInput(int64 inputTime, bool inputLevel, int inputTag = 0) override;
		void advanceToTime(int64 newTime) override;

	protected:
		Random rng;

		ConditionConfig config;

		LogicFIFO inputBuffer;

		int64 nextStableTime;
		int64 nextReadyTime;

		// This returns true if "nextStableTime" or "nextReadyTime" changed.
		bool checkForTrigger(int64 thisTime, bool thisLevel);
	};
}

#endif


// This is the end of the file.
