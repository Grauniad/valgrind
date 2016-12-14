### A friendly warning
This is proof of concept toy, with the sole intent of updating the bug report on WLS (and to get something working on my system in the mean time). This seems to work for me, but I'm not a developer of either project and while you are free to use it you do so at your own risk.

There are plenty of regression tests failing on the WLS even after this change indicating there are further discrepancies between WLS and true Linux to be fixed. This does at least make valgrind begin to be useful...
Build instructions
-------------------------------------------
(I assume you have gcc and valgrind's runtime dependencies installed already since you've got here having had the package valgrind explode on you already...)

1. sudo apt-get install autoconf
2. git clone --recursive https://github.com/Grauniad/valgrind.git
3. cd valgrind
4. ./autogen.sh
5. ./configure
6. make 

If you're feeling rigorous:
7. make regtest

If you want to make it your default valgrind:
8. sudo apt-get remove valgrind
9. sudo make install

Running valgrind on Windows Linux Subsystem
-------------------------------------------
The latest release of valgrind (3.12.0) crashes out when run on "bash for windows". When running almost any non-trivial binary valgrind will crash out with the following error:
	lhumphreys@DESKTOP-V1UODOU:~/Call-Graph-Viewer$ valgrind ./publisherSpeed
	==2183== Memcheck, a memory error detector
	==2183== Copyright (C) 2002-2015, and GNU GPL'd, by Julian Seward et al.
	==2183== Using Valgrind-3.12.0 and LibVEX; rerun with -h for copyright info
	==2183== Command: ./publisherSpeed
	==2183==
	==2183==
	==2183== Process terminating with default action of signal 11 (SIGSEGV): dumping core
	==2183==  General Protection Fault
	==2183==    at 0x400967: main (publisherSpeed.cpp:8)
	==2183==
	==2183== HEAP SUMMARY:
	==2183==     in use at exit: 0 bytes in 0 blocks
	==2183==   total heap usage: 1 allocs, 1 frees, 72,704 bytes allocated
	==2183==
	==2183== All heap blocks were freed -- no leaks are possible
	==2183==
	==2183== For counts of detected and suppressed errors, rerun with: -v
	==2183== ERROR SUMMARY: 0 errors from 0 contexts (suppressed: 0 from 0)
	Segmentation fault (core dumped)
	lhumphreys@DESKTOP-V1UODOU:~/Call-Graph-Viewer$


Investigation
-------------
(We're going to skip the boring detours and dead-ends in pursuit of the minimum repro binary, because as we will see later it really doesn't matter too much). Suffice to say that after some experimenting we can repro the issue without ever code of any significance.  The snippet below would crash out right at the start of main, when it was trying to grow its stack).

	int main (int argc, char** argv) {
	   if (argc == -1 ) {
	      // Some boring irrelevant code
	   }
	   return 0;
	}

Running the binary under strace:
// ... lots of boring stuff ...
	mmap(0x5a2a000, 4194304, PROT_READ|PROT_WRITE|PROT_EXEC, MAP_PRIVATE|MAP_FIXED|MAP_ANONYMOUS, 0, 0) = 0x5a2a000
	mmap(0x802bf9000, 16384, PROT_READ|PROT_WRITE|PROT_EXEC, MAP_PRIVATE|MAP_FIXED|MAP_ANONYMOUS, 0, 0) = 0x802bf9000
	--- SIGSEGV {si_signo=SIGSEGV, si_code=SI_KERNEL, si_addr=0xffeffc9ec} ---
	gettid()                                = 4328
	getrlimit(RLIMIT_CORE, {rlim_cur=9999999*1024, rlim_max=9999999*1024}) = 0
	getpid()                                = 4328
	getpid()                                = 4328
	write(2039, "==4328== \n==4328== Process termi"..., 96==4328== 
// ... more boring stuff ...


Sure enough there's the SIGSEV. Now if we run the same binary under valgrind on a true Linux system we get:
// ... lots of boring stuff ...
	mmap(0x5cc1000, 4194304, PROT_READ|PROT_WRITE|PROT_EXEC, MAP_PRIVATE|MAP_FIXED|MAP_ANONYMOUS, 0, 0) = 0x5cc1000
	mmap(0x802ac0000, 16384, PROT_READ|PROT_WRITE|PROT_EXEC, MAP_PRIVATE|MAP_FIXED|MAP_ANONYMOUS, 0, 0) = 0x802ac0000
	mmap(0x802ac4000, 65536, PROT_READ|PROT_WRITE|PROT_EXEC, MAP_PRIVATE|MAP_FIXED|MAP_ANONYMOUS, 0, 0) = 0x802ac4000
	--- SIGSEGV {si_signo=SIGSEGV, si_code=SEGV_MAPERR, si_addr=0xffeff882c} ---
	gettid()                                = 3891
	mmap(0xffeff8000, 24576, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_FIXED|MAP_ANONYMOUS, 0, 0) = 0xffeff8000
	rt_sigreturn()                          = 8715
	rt_sigprocmask(SIG_BLOCK, NULL, ~[ILL TRAP BUS FPE KILL SEGV STOP], 8) = 0
	rt_sigprocmask(SIG_SETMASK, ~[ILL TRAP BUS FPE KILL SEGV STOP], NULL, 8) = 0
	munmap(0, 0)                            = -1 EINVAL (Invalid argument)
	gettid()   
// ... more boring stuff ...

Presumably that change in si_code is the issue, the question is why? Looking up those kernel codes via man:

Digging into the code
---------------------
Having a poke around we quickly end up in m_signals.c and discover that valgrind is intercepting the SEGV_MAPER event to determine that the binary it is instrumenting is attempting to extend its stack. 

If we hack (@f7973769bd09f31190c0ecb3462b0e5a62b94b37) valgrind to accept the KERNEL signal (the consequences of which require someone with far more domain expertise than myself) we find that valgrind this does indeed fix our bug, and valgrind appears to be working.

Running the regression suite we still get some errors, but we are in much better shape than before:
