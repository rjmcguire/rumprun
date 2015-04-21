/*-
 * Copyright (c) 2014, 2015 Antti Kantee.  All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/types.h>

#include <sys/exec_elf.h>
#include <sys/exec.h>

#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <rump/rump.h>
#include <rump/rump_syscalls.h>

#include <bmk-core/core.h>

#include <rumprun-base/netbsd_initfini.h>
#include <rumprun-base/config.h>

static char *the_env[1] = { NULL } ;
extern void *environ;
void _libc_init(void);
extern char *__progname;

/* XXX */
static struct ps_strings thestrings;
static AuxInfo myaux[2];
extern struct ps_strings *__ps_strings;
extern size_t pthread__stacksize;

typedef void (*initfini_fn)(void);
extern const initfini_fn __init_array_start[1];
extern const initfini_fn __init_array_end[1];
extern const initfini_fn __fini_array_start[1];
extern const initfini_fn __fini_array_end[1];

void *__dso_handle;

static void
runinit(void)
{
	const initfini_fn *fn;

	for (fn = __init_array_start; fn < __init_array_end; fn++)
		(*fn)();
}

static void
runfini(void)
{
	const initfini_fn *fn;

	for (fn = __fini_array_start; fn < __fini_array_end; fn++)
		(*fn)();
}

void
_netbsd_init(void)
{
	thestrings.ps_argvstr = (void *)((char *)&myaux - 2);
	__ps_strings = &thestrings;
	pthread__stacksize = 2*bmk_stacksize;

	rump_boot_setsigmodel(RUMP_SIGMODEL_IGNORE);
	rump_init();

	environ = the_env;
	rumprun_lwp_init();
	runinit();
	_libc_init();

	/* XXX: we should probably use csu, but this is quicker for now */
	__progname = "rumprun";

#ifdef RUMP_SYSPROXY
	rump_init_server("tcp://0:12345");
#endif
	_rumprun_config();

	/*
	 * give all threads a chance to run, and ensure that the main
	 * thread has gone through a context switch
	 */
	sched_yield();
}

void __dead
_netbsd_fini(void)
{
	_rumprun_deconfig();
	runfini();
	rump_sys_reboot(0, 0);
	/* XXX: Should print something if we ever get here, but how? */
	bmk_platform_halt(NULL);
}
