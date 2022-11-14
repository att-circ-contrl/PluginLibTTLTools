 //This prevents include loops. We recommend changing the macro to a name suitable for your library
#ifndef COMMONLIB_H_INCLUDED
#define COMMONLIB_H_INCLUDED

#include <CommonLibHeader.h>

//namespace must be an unique name for your library
namespace CommonLibrary
{
	//All classes that will be exported for its use in plugins need to include the COMMON_LIB clause
	class COMMON_LIB LibraryClass
	{
		public: 
			LibraryClass();
			~LibraryClass();
	};
}

#endif
