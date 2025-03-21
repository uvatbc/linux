/* Make sure timers don't return early
 *              by: john stultz (johnstul@us.ibm.com)
 *		    John Stultz (john.stultz@linaro.org)
 *              (C) Copyright IBM 2012
 *              (C) Copyright Linaro 2013 2015
 *              Licensed under the GPLv2
 *
 *  To build:
 *	$ gcc nanosleep.c -o nanosleep -lrt
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <sys/timex.h>
#include <string.h>
#include <signal.h>
#include <include/vdso/time64.h>
#include "../kselftest.h"

/* CLOCK_HWSPECIFIC == CLOCK_SGI_CYCLE (Deprecated) */
#define CLOCK_HWSPECIFIC		10

#define UNSUPPORTED 0xf00f

char *clockstring(int clockid)
{
	switch (clockid) {
	case CLOCK_REALTIME:
		return "CLOCK_REALTIME";
	case CLOCK_MONOTONIC:
		return "CLOCK_MONOTONIC";
	case CLOCK_PROCESS_CPUTIME_ID:
		return "CLOCK_PROCESS_CPUTIME_ID";
	case CLOCK_THREAD_CPUTIME_ID:
		return "CLOCK_THREAD_CPUTIME_ID";
	case CLOCK_MONOTONIC_RAW:
		return "CLOCK_MONOTONIC_RAW";
	case CLOCK_REALTIME_COARSE:
		return "CLOCK_REALTIME_COARSE";
	case CLOCK_MONOTONIC_COARSE:
		return "CLOCK_MONOTONIC_COARSE";
	case CLOCK_BOOTTIME:
		return "CLOCK_BOOTTIME";
	case CLOCK_REALTIME_ALARM:
		return "CLOCK_REALTIME_ALARM";
	case CLOCK_BOOTTIME_ALARM:
		return "CLOCK_BOOTTIME_ALARM";
	case CLOCK_TAI:
		return "CLOCK_TAI";
	};
	return "UNKNOWN_CLOCKID";
}

/* returns 1 if a <= b, 0 otherwise */
static inline int in_order(struct timespec a, struct timespec b)
{
	if (a.tv_sec < b.tv_sec)
		return 1;
	if (a.tv_sec > b.tv_sec)
		return 0;
	if (a.tv_nsec > b.tv_nsec)
		return 0;
	return 1;
}

struct timespec timespec_add(struct timespec ts, unsigned long long ns)
{
	ts.tv_nsec += ns;
	while (ts.tv_nsec >= NSEC_PER_SEC) {
		ts.tv_nsec -= NSEC_PER_SEC;
		ts.tv_sec++;
	}
	return ts;
}

int nanosleep_test(int clockid, long long ns)
{
	struct timespec now, target, rel;

	/* First check abs time */
	if (clock_gettime(clockid, &now))
		return UNSUPPORTED;
	target = timespec_add(now, ns);

	if (clock_nanosleep(clockid, TIMER_ABSTIME, &target, NULL))
		return UNSUPPORTED;
	clock_gettime(clockid, &now);

	if (!in_order(target, now))
		return -1;

	/* Second check reltime */
	clock_gettime(clockid, &now);
	rel.tv_sec = 0;
	rel.tv_nsec = 0;
	rel = timespec_add(rel, ns);
	target = timespec_add(now, ns);
	clock_nanosleep(clockid, 0, &rel, NULL);
	clock_gettime(clockid, &now);

	if (!in_order(target, now))
		return -1;
	return 0;
}

int main(int argc, char **argv)
{
	long long length;
	int clockid, ret;
	int max_clocks = CLOCK_TAI + 1;

	ksft_print_header();
	ksft_set_plan(max_clocks);

	for (clockid = CLOCK_REALTIME; clockid < max_clocks; clockid++) {

		/* Skip cputime clockids since nanosleep won't increment cputime */
		if (clockid == CLOCK_PROCESS_CPUTIME_ID ||
				clockid == CLOCK_THREAD_CPUTIME_ID ||
				clockid == CLOCK_HWSPECIFIC) {
			ksft_test_result_skip("%-31s\n", clockstring(clockid));
			continue;
		}

		fflush(stdout);

		length = 10;
		while (length <= (NSEC_PER_SEC * 10)) {
			ret = nanosleep_test(clockid, length);
			if (ret == UNSUPPORTED) {
				ksft_test_result_skip("%-31s\n", clockstring(clockid));
				goto next;
			}
			if (ret < 0) {
				ksft_test_result_fail("%-31s\n", clockstring(clockid));
				ksft_exit_fail();
			}
			length *= 100;
		}
		ksft_test_result_pass("%-31s\n", clockstring(clockid));
next:
		ret = 0;
	}
	ksft_exit_pass();
}
