//-----------------------------------------------------------------------------------
//newlib ist eine für embedded Systeme entwickelte Standard C und Math-Library
//newlib stellt eine breite Palette an Funktionen bereit welche in Abhängigkeit 
//des Bedarfs durch Nutzerfunktionen angepasst werden müssen 
//Grob können die Funktionalitäten wie folgt klassifiziert werden
//  - Prozessverwaltung
//     - kill() fork() system() exec() wait() getpid()
//  - Speicherverwaltung
//     - sbrk() zur Bereitstellung von Speicher für die dynamische Speicherverwaltung
//  - Dateiverwaltung (incl. read and Write Buffering)
//     - fopen() fclose() fread() fwrite()
//     - stat(), fstat(), link(), unlink(), lseek()-Anweisungen
//  - Zeitverwaltung
//     - time() gettimeofday()
//  - Mathematische Funktionen
//     - sin() sqrt() ...    für doppelte Genauigkeit
//     - sinf() sqrtf() ...  für einfache Genauigkeit
//     - matherr() für Überlauf Unterlauf
//
//Konfiugration
//newlib ist so programmiert, dass es bei der Erstellung unterschiedlich 
//konfiguriert werden kann
//  - reentrant Fähigkeit: Für Systeme mit OS, wo mehrere Task und damit Systemaufrufe 
//    parallel laufen können 
//    - Alle Funktionen enden mit _r  (bspw. _read_r() anstatt _read() und können 
//      'gleichzeitig' laufen.
//    - Die reentrant Funktionen beinhalten einen 'struct _reent' Pointer als Parameter,
//      in dem Kontextspezifische Informationen abgelegt werden (z.B. errno)
//  - noreentrant Fähgikeit: Für Systeme ohne OS, dementsprechend können Systemaufrufe
//    nicht unterbrochen werden
//  - Floating / Point
//    - Integer Only
//    - Floating Point und Integer -> ist ausgewählt
//  - Malloc
//    - Malloc auf Basis von nano-Malloc(), welches klein aber nicht schnell ist
//    - Malloc von Doug Lea, welches ein Zwischending zwischen schnell und optimal ist
//      Dieses ist implementiert
//      Benötigt mindestens eine Heap-Größe von 3000 Bytes
//  - SCANF
//    - SCANF auf Basis eines nano-scanf()
//    - SCANF auf Basis von ??, 
//      Dieses ist implementiert
//
//         Ohne   IPRINTF   PRINTF     SCANF      PRINTF/SCANF
// RAM     3324    5038      5038       5160         5160
// ROM     3e80    b698     10d60      185b0        1c9b8
// STACK        6b0h=1712  700h=1792  408h=1032
//
//Sonstige Informationen
//-  printf() ist sowohl für integer als auch für float-Zahlen ausgelegt und damit sehr groß
//- iprintf() ist nur für integer-Zahlen ausgelegt und damit geringer im Speicherbedarf
//
//Sonstiges
//- Lizenz berücksichtigen
//- Folgende Funktionen benötigen sbrk() und damit malloc()
//   - ..printf() unter Nutzung von FloatingPoing Zahlen benötigt malloc()
//   - ..scanf() 
//   - fopen() fclose() fgetc() fgets() fputc() fputs() fread()  fwrite() f....()
//   - getc() getchar() gets() putc() putchar() ungetc()
//   - perror()  setbuf()   psignal() ...
//- SourceCode der Library (als auch sin() printf()) kann über github heruntergeladen werden
//  https://sourceware.org/newlib/libc.html
//
//Weitere C-Library
//- clibc aus Linux
//- sglibc
//- eglibc dietlibc 
//
//See Also
//http://www.billgatliff.com/newlib.html
//devkitARM\arm-none-eabi\include\reent.h
//https://www.embedded.com/design/prototyping-and-development/4024867/Embedding-with-GNU-Newlib
//https://sourceware.org/newlib/libc.html#Misc 
//-----------------------------------------------------------------------------------

#include "term.h"

//printf() Adresse des Puffers ohne INIT
//Größe 

//-----------------------------------------------------------------------------------
// errno und Kontextspezifische-Informationen
//errno als Beispiel ist im jeden Kontext eigenständig vorhanden, so dass
//unterschiedliche Fehler in unterschiedlichen Kontexten nicht zur Verwirrung führen
//Auch hat jeder Kontext seinen eigenen Dateihandlerbereich (stdin, stdout, stderr) so
//dass jeder Kontext einen anderen Ein-/Ausgabekanal nutzen kann.
//Zur Speicherung dieser Daten gibt es den _impure_ptr, der zum Laufzeitbeginn eines
//jeden Kontextes gesetzt werden muss. Der Speicher zum Speichern der Kontextspezifischen
//Daten muss ebenfalls von Nuter bereitgestellt werden
//Hinweis: errno ist in newlib nicht als Variable sondern als Rückgabewert des 
//   Funktionsaufrufes __errno(){ return _impure_ptr->errno} definiert
//-----------------------------------------------------------------------------------
#define MODE_IMPURE_NOP 0
#define MODE_IMPURE_ST  1
#define MODE_IMPURE_MT  2
//Wird spätestens dann benötigt, wenn errno und weitere FileOperationen benötigt werden
//Für reines printf/scanf nicht notwendig
#define MODE_IMPURE     MODE_IMPURE_NOP

#if   MODE_IMPURE == MODE_IMPURE_NOP
#elif MODE_IMPURE == MODE_IMPURE_ST
	#include <reent.h>
	#include <string.h>
	struct _reent reent;
	static void newlib_syscalls_init(void)
	{
		_REENT_INIT_PTR(&reent);
		_impure_ptr = &reent;
	}  
	__attribute__((section(".preinit_array"))) static void(*fcn)(void) __attribute__((unused)) = {newlib_syscalls_init};
#elif MODE_IMPURE == MODE_IMPURE_MT
#include <reent.h>
	strcut _reent rtbl[OS_MAX_TASKS];
	static void newlib_syscalls_init(void)
	{
		int i;
		for(i=0;i<OS_MAX_TASKS;i++)
			_REENT_INIT_PTR(&rtbl[i]);
		_impure_ptr = &rtbl[0];
	}  
	void OSTaskSwHook(void)
	{
		_impure_ptr = &rtbl[xyz];
	}
	__attribute__((section(".preinit_array"))) static void(*fcn)(void) __attribute__((unused)) = {newlib_syscalls_init};
	//void func(void)
	//{
	//	...
	//	switch(errno)    //switch(*_errno() -> switch(*&_impure_ptr->_errno);
	//}
#else
	#error "Inccorret MODE_IMPURE"
#endif

//-----------------------------------------------------------------------------------
//fork() execve() kill() wait() getpid()-Anweisung
//fork aus Posix zum Erzeugen eine neuen Task als Kopie der aktuellen Task
//execve -> Transfer control to a new process
//kill -> Send a signal.
//wait -> Wait for a child process
//getpid -> Process-ID;
//Starten/Beenden von Task ist nicht Bestandteil von newlib und muss bei Bedarf
//über das vorhandene OS händisch nachgebildet oder eben durch eine Fehleranweisung 
//deren Nichtexistenz dargestellt werden
//-----------------------------------------------------------------------------------
#define MODE_TASK_NOP 0
#define MODE_TASK_ERR 1
#define MODE_TASK     MODE_TASK_ERR

#if    MODE_TASK == MODE_TASK_NOP
#elif MODE_TASK == MODE_TASK_ERR
	#include <reent.h>
	#include <errno.h>
	
	int _fork_r (struct _reent *ptr )
	{
		ptr->_errno = ENOTSUP;
		return -1;
	}

	int _execve_r (struct _reent *ptr, const char *name, char *const *argv, char *const *env)
	{
		(void)name;
		(void)argv;
		(void)env;
		ptr->_errno = ENOMEM;
		return -1;
	}

	int _kill_r (struct _reent *ptr, int pid, int sig)
	{
		(void)pid;
		(void)sig;
		ptr->_errno = EINVAL;
		return -1;
	}

	int _wait_r (struct _reent *ptr, int *status)
	{
		(void)status;
		ptr->_errno = ECHILD;
		return -1;
	}

//wird von printf() benötigt
	int _getpid_r(struct _reent *ptr)
	{
		(void)ptr;
		//These functions are always successful.
		return 1;
	}
#else	
	#error "Incorrect MODE_TASK"
#endif

//-----------------------------------------------------------------------------------
//malloc()- sbrk()-Anweisung
//Anweisung für die Dynamische Speicherverwaltung. Wird immer dann aufgerufen,
//wenn zuvor noch kein HEAP definiert wurde, bzw. der mit malloc() reservierte
//Bereich aufgebraucht wurde. sbrk() gibt keine neuen Bereich zurück, sondern 
//vergrößtert nur den bereits vorhandenen!
//Für mehr Details siehe Manpage von sbrk()
//Die Malloc() Funktionalität benötigt viel Speicher, daher kann Sie durch
//einfaches überladen 'ausgeschaltet' werden.
//Zur Sicherstellung der Reentrantfähigkeit müssen noch die Funktionen
// __malloc_lock()  und __malloc_unlock() bereitgestellt werden, so dass
// bei gleichzeitigen Zugriff keine korrupte Daten entstehen.
//  - To permit multiple processing contexts in newlib's malloc() implementation, 
//    you must also provide the functions __malloc_lock() and __malloc_unlock() 
//    to protect your memory pool from corruption during simultaneous allocations. 
//    If you are using an RTOS's reentrant memory pool implementation for dynamic 
//    memory allocation, however, this heap protection is unnecessary—the RTOS 
//    protects the heap itself. I will return to these functions in a moment.
//-----------------------------------------------------------------------------------
#define MODE_HEAP_NO     1
#define MODE_HEAP_NEWLIB 2
#define MODE_HEAP_USER   3
#define MODE_HEAP      MODE_HEAP_NEWLIB

#if MODE_HEAP == MODE_HEAP_NO
	#include <stdio.h>   //für size_t
	
	void *_sbrk_r (struct _reent *ptr, ptrdiff_t incr)
	{
		return (void *)NULL;
	}

	void *malloc(size_t size) 
	{
		return (void *)0;
	}

	void free(void *ptr) 
	{
	}

#elif MODE_HEAP == MODE_HEAP_NEWLIB
	#include <reent.h>
	#include <stdint.h>
	#include <stdlib.h>   //fuer abort()
	#include <unistd.h>   //fuer write()
	#include <errno.h>
	
	extern uint32_t __heap_start__;		//Definiert in link.ld
	extern uint32_t __heap_end__;		//Definiert in link.ld

	//NewLib fordert, dass _sbrk_r vorhanden ist
	void *_sbrk_r (struct _reent *ptr, ptrdiff_t incr)
	{
		static unsigned char *     heap_end;
			   unsigned char *prev_heap_end;
		(void) ptr;
		
		if( heap_end == 0 ) 
			heap_end = (unsigned char *)&__heap_start__;

		prev_heap_end = heap_end;

		if( heap_end + incr > (unsigned char *)&__heap_end__ ) {
			#if 1
			//iprintf() faengt diesen Fehler nicht ab!
			//daher hier der radikale Lösungsweg für dieses Problem!
			/* heap overflow—announce on stderr */
			write( 2, "Heap overflow!\n", 15 );
			abort();
			#else
			(ptr)->_errno=ENOMEM;
			return (void *)-1;
			#endif
		}

		heap_end += incr;
#if 1
		static unsigned char sbrk_fill=0x47;
		for(unsigned char *ptr=prev_heap_end;ptr<heap_end;ptr++)
			*ptr=sbrk_fill;
		sbrk_fill++;
#endif
		return (void *) prev_heap_end;
	}
	//Für Multitasking-Betrieb 
	void __malloc_lock(struct _reent *r)
	{
		(void) r;
	}
	void __malloc_unlock(struct _reent *r)
	{
		(void) r;
	}
	
#elif MODE_HEAP == MODE_HEAP_USER
	extern uint32_t __heap_start__;		//Definiert in link.ld
	extern uint32_t __heap_end__;		//Definiert in link.ld

	typedef struct HEAP_ST {
		struct HEAP_ST *prev;	//Zeiger auf vorherigen Verwaltungsblock
		struct HEAP_ST *next;	//Zeiger auf nächsten Verwaltungsblock
		short    size;			//positive Wert  -> Speicherblock belegt
								//negativer Wert -> Speicherbereich frei
		short    crc;   		//Zum Überwachen des Speicherbereiches
								//= Quersumme über next, prev und size
	} HEAP_STRUCT;

	void *_sbrk_r (struct _reent *ptr, ptrdiff_t incr)
	{
		return (void *)-1;
	}
	
	void *malloc(size_t size) 
	{
		return (void *)0;
	}

	void free(void *ptr) 
	{
	}

#else
	#error "Incorrect MODE_HEAP"
#endif

//-----------------------------------------------------------------------------------
//read()/write()-Anweisung
//Die read()/write() Funktionen werden sowohl für stdin/stdout/stderr als auch für alle 
//Dateioperationen aufgerufen. In der newlib Implementierung ist gleichermaßen kein 
//Dateisystem als auch kein Terminalsystem implementiert, so dass diese Funktionalitäten
//durch den Anwender bei Bedarf bereitgestellt werden müssen
//  - Die ersten 3 Dateihandler sind defaultmäßig geöffnet und wie folgt vorbelegt
//      0 -> stdin
//      1 -> stdout
//      2 -> stderr
//  - mit jeden open() werden dann Dateihandler > 2 zurückgegeben
//  - die Aufrufhierarchie von printf() / scanf() sieht wie folgt aus:
//      printf() -> puts() -> write(stdout,...) -> _write_r(...,1,...)
//      scanf()  -> getc() -> read(stdin,...)   -> _read_r(...,0,...)
//Alle IO-Operationen werden somit über _read_r() und _write_r() abgebildet,
//welche somit entsprechend der Funktionalität bereitgestellt werden müssen
//-----------------------------------------------------------------------------------
#define MODE_RW_NOP       0
#define MODE_RW_UARTONLY  1
#define MODE_RW_FULL      2
#define MODE_RW           MODE_RW_UARTONLY

#if   MODE_RW == MODE_RW_NOP
	_ssize_t _read_r (struct _reent *r, int fileDesc, void *ptr, size_t len)
	{ return -1; }
	_ssize_t _write_r(struct _reent *r, int fileDesc,const void *ptr,size_t len)
	{ return -1; }
	
#elif MODE_RW == MODE_RW_UARTONLY
	#include <stdio.h>   //für EOF
	#include <sys/types.h>
	#include <sys/stat.h>
	#include <unistd.h>
	#include <errno.h>
	
	//NewLib doesnt call open() for stdin/stdout/stderr
	int _open_r ( struct _reent *r, const char *file, int flags, int mode )
	{
		(void) r;
		(void) file;
		(void) flags;
		(void) mode;
		return -1; 
	}

	int _close_r(struct _reent *r, int fd )
	{ 
		(void) r;
		(void) fd;
		return -1; 
	}

	_ssize_t _read_r (struct _reent *r, int fileDesc, void *ptr, size_t len)
	{
		/* GetChar : Your implementation to receive the character from the serial port.*/
		size_t i;
		char *p=ptr;
		(void) r;        //wird hier nicht benötigt
		(void) fileDesc; //sollte aufgrund von scanf immer den Wert 0 haben
		for(i=0;i<len;i++) {
			unsigned char c;
			while(TERM_READ(&c)==-1);
			if((c=='\n') || (c=='\r')) {
				*p++=0;//EOF;
				return i+1;
			}
			*p++=(char)c;
			//if(error)
			//  r->errno=EIO;
		}
		return (len);
	}

#if 1
	_ssize_t _write_r(struct _reent *r, int fileDesc,const void *ptr,size_t len)
	{
		/* PutChar : Your implementation to send the character to the serial port.*/
		size_t  i;
		char *p=(char *)ptr;
		(void) r;        //wird hier nicht benötigt
		(void) fileDesc; //sollte aufgrund von printf immer den Wert 1 haben
		for(i=0;i<len;i++) {      
			TERM_CHAR(*p++);
			//if(error)
			//  r->errno=EIO;
		} 
		//Wenn hier nicht die angeforderte Länge zurückgegeben wird,
		//ruft newlib die _write_r() erneut auf
		return len;
	}
#else 
	ssize_t _write(int fd, const void *buf, size_t count)
	{
		(void)fd;
		(void) buf;
		(void)count;
		return 0;
	}
#endif
/*	int _stat_r (  struct _reent *r, const char *file,struct stat *pstat ) */
	int _fstat_r ( struct _reent *r, int fd          ,struct stat *pstat )
	{
		(void) r;
		(void) fd;
		(void) pstat;
		//Angabe, dass es sich bei diesem File um eine Character-Device handelt
		pstat->st_mode = S_IFCHR;
		return 0;
	}
	
	_off_t _lseek_r( struct _reent *r, int fd, _off_t pos, int whence )
	{
		(void) r;
		(void) fd;
		(void) pos;
		(void) whence;
		return 0;
	}

	int _isatty_r( struct _reent *r, int fd)
	{
		(void) r;
		(void) fd;
		return 1;
	}
/*
	int _link_r ( struct _reent *_r, const char *oldname, const char *newname )
	{
		r->errno = EMLINK;
		return -1;
	}

	int _unlink_r ( struct _reent *_r, const char *name )
	{
		r->errno = EMLINK;
		return -1;
	}
*/
#elif MODE_RW == MODE_RW_FULL
	typedef struct {
	   const char *name;
	   int    (*open_r  )(struct _reent *r, const char *path,int flags, int mode );
	   int    (*close_r )(struct _reent *r, int fd );
	   long   (*write_r )(struct _reent *r, int fd, const char *ptr, int len );
	   long   (*read_r  )(struct _reent *r, int fd,       char *ptr, int len );
	   int    (*fstat_r )(struct _reent *r, int fd,struct stat *pstat );
	   _off_t (*lseek_r )(struct _reent *r, int fd, _off_t pos, int whence );
	} devoptab_t;

	/* devoptab for an example stream device called "com1" */
	const devoptab_t devoptab_com1 = { "com1",
									   com1_open_r,
									   com1_close_r,
									   com1_write_r,
									   com1_read_r };
									   
	const devoptab_t *devoptab_list[] = {	&dotab_com1,  /* standard input */
											&dotab_com1,  /* standard output */
											&dotab_com1,  /* standard error */
											&dotab_com2,  /* another device */
											... ,         /* and so on... */
											0             /* terminates the list */
	};

	_ssize_t _write_r ( struct _reent *ptr, int fd, const void *buf, size_t cnt )
	{
	   return devoptab_list[fd].write_r( ptr, fd, buf, cnt );
	}

	_ssize_t _read_r( struct _reent *ptr, int fd, void *buf, size_t cnt )
	{
	   return devoptab_list[fd].read_r( ptr, fd, buf, cnt );
	}

	int _open_r ( struct _reent *ptr, const char *file, int flags, int mode )
	{
		int which_devoptab = 0;
		int fd = -1;

		/* search for "file" in dotab_list[].name */
		do {
			if(strcmp(devoptab_list[which_devoptab].name, file ) == 0 ) {
				fd = which_devoptab;
				break;
			}
		} while( devoptab_list[which_devoptab++] );

		/* if we found the requested file/device, then invoke the device's open_r() method */
		if( fd != -1 ) 
			devoptab_list[fd].open_r( ptr, file, flags, mode );
		else 
			ptr->errno = ENODEV;

		return fd;
	}

	int _close_r(struct _reent *ptr, int fd )
	{
		return devoptab_list[fd].close_r( ptr, fd );
	}

	int _fstat_r ( struct _reent *ptr, int fd          ,struct stat *pstat )
	{
		return devoptab_list[fd].fstat_r(ptr,fd,pstat);
	}

	_off_t _lseek_r (struct _reent *ptr, int fd, _off_t pos, int whence )
	{
		return devoptab_list[fd].lseek_r(ptr,fd,pos,whence);
	}
#else
	#error "Incorrect MODE_RW"
#endif

//-----------------------------------------------------------------------------------
// time() Funktion
// Für Zeitabfragen muss die Funktion _time_r() bereitgestellt werden
//-----------------------------------------------------------------------------------
#if 0
#include <time.h>
/*  Get Process Times, P1003.1b-1993, p. 92 */
struct tms {
};
_CLOCK_T_ _times_r        (struct _reent *ptr, struct tms *tms)
{
	tms->tms_utime  = (clock_t)xyz;		/* user time */
	tms->tms_stime  = (clock_t)xyz;		/* system time */
	tms->tms_cutime = (clock_t)xyz;		/* user time, children */
	tms->tms_cstime = (clock_t)xyz;		/* system time, children */
	
	return((clock_t)xyz);
}

/* This one is not guaranteed to be available on all targets.  */
int       _gettimeofday_r (struct _reent *, struct timeval *__tp, void *__tzp));

#endif

//-----------------------------------------------------------------------------------
//setenv() / getenv()
//Zum setzen und lesen der Environment Variablen (z.B. PATH)
//Zur Sicherstellung der Reentrantfähigkeit müssen noch die Funktionen
//__env_lock() und __env_unlock() bereitgestellt werden
//-----------------------------------------------------------------------------------

//-----------------------------------------------------------------------------------
//weitere Funktionen, die bei Bedarf bereitgestellt werden müssen
//-----------------------------------------------------------------------------------
#if 0
int       _fcntl_r        (struct _reent *, int, int, int)
int       _isatty_r       (struct _reent *, int)
int       _mkdir_r        (struct _reent *, const char *, int)
int       _rename_r       (struct _reent *, const char *, const char *)
#endif





