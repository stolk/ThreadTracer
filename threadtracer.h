// threadtracer.h
// (c) 2017 by Game Studio Abraham Stolk Inc.

#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif
extern int tt_signin( pthread_t tid, const char* threadname );

extern int tt_stamp( const char* cat, const char* tag, const char* phase );

extern int tt_report( const char* oname = 0 );

#ifdef __cplusplus

class tt_scope
{
	public:
		tt_scope( const char* tag ) : name(tag)
		{
			tt_stamp( "generic", tag, "B" );
		}
		~tt_scope()
		{
			tt_stamp( "generic", name, "E" );
		}
	protected:
		const char* name;
};

#define TT_SCOPE(S) \
	tt_scope __scope( S )

}
#endif

#define TT_BEGIN(S) tt_stamp( "generic", S , "B" )

#define TT_END(S)   tt_stamp( "generic", S , "E" )

