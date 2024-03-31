/*
 *
 */

#ifndef _lake_job_h
#define _lake_job_h

struct Target;

/* ================================================================================================ Job
 */
struct Job
{
	Target* target;

	int closure_ref;
};

#endif
