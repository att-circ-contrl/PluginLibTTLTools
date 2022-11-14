#ifndef TTLTOOLS_CIRCBUF_H_DEFINED
#define TTLTOOLS_CIRCBUF_H_DEFINED

#include <CommonLibHeader.h>


//
// Circular buffer helper class - Delcaration.

// NOTE - If you store pointers, do your own de-allocation! This doesn't do it for you.
// NOTE - This is not MT-safe! Use one of JUCE's buffer types if you need locking.

namespace TTLTools
{
	template <class datatype_t,size_t bufsize> class COMMON_LIB CircBuf
	{
	public:
		CircBuf();
		void clear();
		void enqueue(datatype_t newVal);
		datatype_t dequeue();
		datatype_t snoop();
		size_t count();

	protected:
		datatype_t dataBuffer[bufsize];
		size_t readPtr, writePtr, dataCount;
	};
}



//
// Circular buffer helper class - Implementation.


// C++ compiles templated classes on-demand. The source code has to be included by every file that instantiates templates.
// Only one copy of each variant will actually end up in the binary; extra copies get pruned at link-time.
// There are other ways to handle this, but they're just as ugly as this way.


template <class datatype_t,size_t bufsize>
TTLTools::CircBuf<datatype_t,bufsize>::CircBuf()
{
	clear();
}


template <class datatype_t,size_t bufsize>
void TTLTools::CircBuf<datatype_t,bufsize>::clear()
{
	readPtr = 0;
	writePtr = 0;
	dataCount = 0;
}


template <class datatype_t,size_t bufsize>
void TTLTools::CircBuf<datatype_t,bufsize>::enqueue(datatype_t newVal)
{
	// Silently discard this if we're out of space.
	if (dataCount < bufsize)
	{
		dataBuffer[writePtr] = newVal;
		// The compiler will optimize this if bufsize is a power of two.
		writePtr = (writePtr + 1) % bufsize;
		dataCount++;
	}
}


template <class datatype_t,size_t bufsize>
datatype_t TTLTools::CircBuf<datatype_t,bufsize>::dequeue()
{
	// Do a non-destructive read.
	datatype_t returnVal = snoop();

	// If we had data to fetch, update the read pointer.
	if (dataCount > 0)
	{
		// The compiler will optimize this if bufsize is a power of two.
		readPtr = (readPtr + 1) % bufsize;
		dataCount--;
	}

	return returnVal;
}


template <class datatype_t,size_t bufsize>
datatype_t TTLTools::CircBuf<datatype_t,bufsize>::snoop()
{
	// Pick a safe default value.
	datatype_t returnVal = (datatype_t) 0;

	// Only fetch data if there's data to fetch.
	// Don't change the read pointer - this is a non-destructive read.
	if (dataCount > 0)
		returnVal = dataBuffer[readPtr];

	return returnVal;
}


template <class datatype_t,size_t bufsize>
size_t TTLTools::CircBuf<datatype_t,bufsize>::count()
{
	return dataCount;
}


#endif

//
// This is the end of the file.
