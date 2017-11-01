// threadtracer.c
// (c) 2017 by Game Studio Abraham Stolk Inc.

#include <sys/resource.h>
#include <sys/types.h>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>

#include "threadtracer.h"


#define MAXTHREADS	12		//!< How many threads can we support?
#define MAXSAMPLES	64*1024		//!< How many samples can we record for a thread?

//! How many threads are we currently tracing?
static int numthreads=0;

//! When (in wallclock time) did we start tracing?
static int64_t walloffset=0;

//! When (in cpu time) did we start tracing?
static int64_t cpuoffset=0;

//! Optionally, we can delay the recording until this timestamp using THREADTRACERSKIP env var.
static int64_t wallcutoff=0;

//! Are we currently recording events?
static int isrecording=0;

//! The information we record for a trace event.
typedef struct
{
	const char* cat;			//!< category
	const char* tag;			//!< tag
	const char* phase;			//!< "B" or "E"
	int64_t wall_time;			//!< timestamp on wall clock
	int64_t cpu_time;			//!< timestamp on thread's cpu clock
	int64_t num_involuntary_switches;	//!< number of context switches (premptive)
	int64_t num_voluntary_switches;		//!< number of context switches (cooperative)
} sample_t;

//! The samples recorded, per thread.
static sample_t samples[ MAXTHREADS ][ MAXSAMPLES ];

//! The number of samples recorded, per thread.
static int samplecounts[ MAXTHREADS ];

//! The names for the threads.
static const char* threadnames[ MAXTHREADS ];

//! The thread-ids.
static pthread_t threadids[ MAXTHREADS ];


//! Before tracing, a thread should make itself known to ThreadTracer.
int tt_signin( pthread_t tid, const char* threadname )
{
	if ( numthreads == 0 )
	{
		struct timespec wt, ct;
		clock_gettime( CLOCK_MONOTONIC,         &wt );
		clock_gettime( CLOCK_THREAD_CPUTIME_ID, &ct );
		walloffset = wt.tv_sec * 1000000000L + wt.tv_nsec;
		cpuoffset  = ct.tv_sec * 1000000000L + ct.tv_nsec;
		struct timespec res;
		clock_getres( CLOCK_THREAD_CPUTIME_ID, &res );
		fprintf( stderr, "ThreadTracer: clock resolution: %ld nsec.\n", res.tv_nsec );
		wallcutoff = walloffset;
		const char* d = getenv( "THREADTRACERSKIP" );
		if ( d )
		{
			int delayinseconds = atoi( d );
			wallcutoff += delayinseconds * 1000000000L;
			fprintf( stderr, "ThreadTracer: skipping the first %d seconds before recording.\n", delayinseconds );
		}
		isrecording = 1;
	}
	if ( numthreads == MAXTHREADS )
		return -1;
	if ( tid == (pthread_t) -1 )
		tid = pthread_self();
	int slot = numthreads++;
	threadnames[ slot ] = threadname;
	threadids[ slot ] = tid;
	samplecounts[ slot ] = 0;
	return slot;
}


//! Record a timestamp.
int tt_stamp( const char* cat, const char* tag, const char* phase )
{
	if ( !isrecording )
	{
		if ( !numthreads )
			fprintf( stderr, "ThreadTracer: ERROR. Threads did not sign in yet. Cannot record.\n" );
		return -1;
	}

	const pthread_t tid = pthread_self();

	struct timespec wt, ct;
	clock_gettime( CLOCK_MONOTONIC,         &wt );
	clock_gettime( CLOCK_THREAD_CPUTIME_ID, &ct );
	struct rusage ru;
	int rv;
	rv = getrusage( RUSAGE_THREAD, &ru );
	if ( rv < 0 )
	{
		isrecording = 0;
		fprintf( stderr, "ThreadTracer: rusage() failed. Stopped Recording.\n" );
		return -1;
	}

	const int64_t wall_nsec = wt.tv_sec * 1000000000 + wt.tv_nsec;
	const int64_t cpu_nsec  = ct.tv_sec * 1000000000 + ct.tv_nsec;

	if ( wall_nsec < wallcutoff )
		return -1;

	for ( int i=0; i<numthreads; ++i )
		if ( threadids[ i ] == tid )
		{
			const int cnt = samplecounts[ i ];
			if ( cnt >= MAXSAMPLES )
			{
				isrecording = 0;
				fprintf( stderr, "ThreadTracer: Stopped recording samples. Limit(%d) reached.\n", MAXSAMPLES );
				return -1;
			}
			sample_t* sample = samples[ i ] + samplecounts[ i ];
			sample->wall_time = wall_nsec - walloffset;
			sample->cpu_time  = cpu_nsec - cpuoffset;
			sample->num_involuntary_switches = ru.ru_nivcsw;
			sample->num_voluntary_switches = ru.ru_nvcsw;
			sample->tag = tag;
			sample->cat = cat;
			sample->phase = phase;
			return samplecounts[ i ]++;
		}

	fprintf( stderr, "ThreadTracer: Thread(%ld) was not signed in before recording the first time stamp.\n", tid );
	fprintf( stderr, "ThreadTracer: Recording has stopped due to sign-in error.\n" );
	isrecording = 0;
	return -1;
}


int tt_report( const char* oname )
{
	if ( numthreads == 0 )
	{
		fprintf( stderr, "ThreadTracer: Nothing to report, 0 threads signed in.\n" );
		return -1;
	}
	FILE* f = fopen( oname, "w" );
	if ( !f ) return -1;

	int total = 0;
	int discarded = 0;
	fprintf( f, "{\"traceEvents\":[\n" );

	for ( int t=0; t<numthreads; ++t )
	{
		for ( int s=0; s<samplecounts[t]; ++s )
		{
			const sample_t* sample = samples[t] + s;

			char argstr[128];
			if ( sample->phase[0] == 'E' )
			{
				if ( s==0 ) { discarded++; goto notfound; }
				int i=s-1;
				while ( strcmp( samples[t][i].tag, sample->tag ) || samples[t][i].phase[0] != 'B' )
				{
					i--;
					if ( i<0 ) { discarded++; goto notfound; }
				};
				const sample_t* beginsample = samples[t]+i;
				int64_t preempted = sample->num_involuntary_switches - beginsample->num_involuntary_switches;
				int64_t voluntary = sample->num_voluntary_switches - beginsample->num_voluntary_switches;
				int64_t walldur   = sample->wall_time - beginsample->wall_time;
				int64_t cpudur    = sample->cpu_time  - beginsample->cpu_time;
				int64_t dutycycle = 100 * cpudur / walldur;
				snprintf( argstr, sizeof(argstr), "{\"preempted\":%ld,\"voluntary\":%ld,\"dutycycle(%%)\":%ld}", preempted, voluntary, dutycycle );
			}
			else
				snprintf( argstr, sizeof(argstr), "{}" );

			if ( total )
				fprintf( f, ",\n" );
			fprintf
			(
				f,
				"{\"cat\":\"%s\","
				"\"pid\":%ld,"
				"\"tid\":%ld,"
				"\"ts\":%ld,"
				"\"tts\":%ld,"
				"\"ph\":\"%s\","
				"\"name\":\"%s\","
				"\"args\":%s}",
				sample->cat, 0L, threadids[t], sample->wall_time / 1000, sample->cpu_time / 1000, sample->phase, sample->tag, argstr
			);
			total++;
			// Note: unfortunately, the chrome tracing JSON format no longer supports 'I' (instant) events.
notfound:
			(void)0;
		}
	}
	for ( int t=0; t<numthreads; ++t )
	{
		fprintf( f, ",\n{" );
		fprintf( f, 
			"\"name\": \"thread_name\", "
			"\"ph\": \"M\", "
			"\"pid\":%ld, "
			"\"tid\":%ld, "
			"\"args\": { \"name\" : \"%s\" } }",
			0L,
			threadids[t],
			threadnames[t]
		);
	}

	fprintf( f, "\n]}\n" );
	fclose(f);
	fprintf( stderr, "ThreadTracer: Wrote %d events (%d discarded) to %s\n", total, discarded, oname );
	return total;
}

