/*****************************************************************************
* Product: QDK/C_ARM-GNU_AT91SAM7S-EK, QK-port
* Last Updated for Version: 4.1.02
* Date of the Last Update:  Jan 13, 2010
*
*                    Q u a n t u m     L e a P s
*                    ---------------------------
*                    innovating embedded systems
*
* Copyright (C) 2002-2010 Quantum Leaps, LLC. All rights reserved.
*
* This software may be distributed and modified under the terms of the GNU
* General Public License version 2 (GPL) as published by the Free Software
* Foundation and appearing in the file GPL.TXT included in the packaging of
* this file. Please note that GPL Section 2[b] requires that all works based
* on this software must also be made publicly available under the terms of
* the GPL ("Copyleft").
*
* Alternatively, this software may be distributed and modified under the
* terms of Quantum Leaps commercial licenses, which expressly supersede
* the GPL and are specifically designed for licensees interested in
* retaining the proprietary status of their code.
*
* Contact information:
* Quantum Leaps Web site:  http://www.quantum-leaps.com
* e-mail:                  info@quantum-leaps.com
*****************************************************************************/
OUTPUT_FORMAT("elf32-littlearm", "elf32-bigarm", "elf32-littlearm")
OUTPUT_ARCH(arm)
ENTRY(_vectors)

/*****************************************************************************
ROM->PROGRAMM ist im ROM enthalten
       ROM-Bereich: wurde durch Debugger aus ELF-File programmiert
       RAM-Bereich: nicht initialisiert
RAM-> Programm ist nur im RAM enthalten
       ROM-Bereich: Inhalt ist ohne Bedeutung
       RAM-Bereich: Inhalt wurde durch Debugger aus ELF-File korrekt geladen
                    (incl. DATA und FASTCODE)
SIM-> Hier wird nicht zwischen RAM und ROM unterschieden
      sonder es steht der Anwendung den gesamten Speicherbereich zur Verfügung
      Memory-Swap wird nicht unterstützt
	  ROM-Bereich: wird vom Debugger aus ELF-File geladen
	  RAM-Bereich: wird vom Debugger aus ELF-File geladen
	               (incl. DATA und FASTCODE)
SAMBA-> Der Prozessorinterne Bootloader unterstützt Anweisungen zum 
        Programmieren des internen RAM's.
		Die Anweisungen zum download werden über 'bin2samba' aus dem durch 
		'objcopy' aus dem ELF-File erzeugten BIN-File generiert 
		Der Bootloader belegt den Speicherbereich 0x200000..0x202000, so dass
		dieser Bereich nicht überladen werden sollte, sondern erst zur Laufzeit
		der Anwendung für HEAP/STACK/BSS genutzt werden kann.
		Ein Memory-Remap wurde noch nicht durchgeführt
		Der Prozessor wurde bereits initialisiert (für USB notwendig), jedoch 
		läuft der PIT in dieser Einstellung nicht mit der erwarteten Geschwindigkeit
		Nach Aktivierung von SAM-BA merkt sich der Prozessor diesen Modus, so 
		das beim nächsten Prozessor-Start der SAM-BA erneut aktiviert wird
		Zu ladendes Programm wird o
*******************************************************************************
  Speicher    ROM-Mode     RAM-Mode      SIM-MODE      SAMBA
  +-------+
  |       |
  |  ...  |
  +-------+               
  |       |                                             DATA
  |  RAM  |                                           7)main
  |  RAM  |               7)main                      5)_cstartup
  |  RAM  |               5)_cstartup                 3)ll_init
  |       |               3)ll_init                     FASTCODE
  |       |               2)_reset                    2)_reset
  |       |               1)_vectors                  1)_vectors
  |       |  STACK          STACK
  |       |  HEAP           HEAP         STACK
  |       |  BSS            BSS          HEAP          STACK
  |       |  6)DATA         DATA         BSS           HEAP
  |       |  6)Fastcode     FASTCODE     DATA          BSS
  +-------+  4)RAMVEC     4)RAMVEC       FASTCODE     4)RAMVEC
  |  ROM  |  6)DATA
  |       |  7)main
  |       |  6)Fastcode
  |       |  5)_cstartup
  +-------+  3)ll_init     
  |RAM/ROM|
  | REMAP |                              7)main
  |       |                              5)_cstartup
  |       |                              3)ll_init
  |       |  2)_reset                    2)_reset
  +-------+  1)_vectors   4)RAMVEC       1)_vectors   4)RAMVEC
             4)RAMVEC                      RAMVEC
1) startadresse
    - Im ROM-Mode durch im Prozessor hard codierte Reset-Vector-Tabelle vorgegeben
	- In RAM-/SIM-Mode durch ENTRY Anweisung hier im LD-File vorgegeben
	- IM SAMBA-Mode durch Benutzer manuelle einzugegeben
2) _reset (startup.s)
    = Maschinensprachebefehle zum Aufruf von low_level_init()
    - wird über relativen Sprung in der Reset-Vector-Tabelle angesprungen
	- Im ROM-Mode dadurch möglich, das ROM bei 0x0000 eingeblendet ist
3) low_level_init (low_level_init.c)
	= Prozessor Initialisierung (außer im SIM-Mode) (48MHz, FLASH Zugriff) 
	= Vector-Tabelle vorbereiten, die anschließend auf Adresse 0x0000 zum vorschein kommt
	= Memory-Swap (außer im SIM-MODE), d.h. auf Speicheradresse 
	    0x000000 wird inhalt von 0x200000 eingeblendet
	- Aufruf über Sprung zur absoluten Adresse
	- Als Rücksprungadresse wird absolute Adresse von _cstartup eingetragen
	  (und nicht die aktuelle Adresse zum Zeitpunkt der Ausführung)
	  Notwendig, da low_level_init ein Memory-Swap durchführt und an der
	  eigentlichen Rücksprungadresse nach dem Swap kein Code durchführt
4) Vector-Tabelle
	- Damit diese zur Laufzeit geändert werden kann, muss auf Adresse 0x000000 RAM
	  vorhanden sein.  Zur Bootzeitpunkt ist in diesem Bereich jedoch das ROM
	  eingeblendet, so dass eine Memory-Remap/Swap Befehl notwendig wird
	- Außer im SIM-Mode ist somit ein Memory-Swap Befehl notwendig, so dass 
	  im Bereich von 0x200000..0x20003F Speicher hierfür freigehalten werden muss
	  welcher nach dem Remap auch in 0x00000..0x000003F sichtbar wird
	- In der Vector-Tabelle sind Absolute Sprünge vorhanden, so dass hierüber 
	  auf jede Speicheradresse gesprungen werden kann
5) _cstartup (startup.s)
	= BSS/STACK/CLIB Initialisieren
	= DATA/FASTCODE  aus ROM in den RAM kopieren (nur ROM-Mode)
	= Main Aufruf
6)  DATA/FASTCODE Initialisierungswerte
	- Initialisierungswerte aus ROM in den RAM kopieren
	- hierfür wurden hier im linker-File über 'RAM AT>ROM' Platz im RAM und ROM vorgehalten
	- Nur im ROM-Mode nötig
	- Im RAM/SIM-Mode wird dieser Bereich direkt durch den Debugger geladen
	- Im SAMBA-Mode dadurch ereicht, dass diese beiden Bereich dem ROM zugeordnet wurden
7)  main
	- Ohne Worte
*****************************************************************************/

MEMORY {                                       /* memory map of AT91SAM7S64 */
ROM_ONLY         ROM (rx)  : ORIGIN = 0x00100000, LENGTH = 256k
ROM_ONLY         RAM (rwx) : ORIGIN = 0x00200000, LENGTH =  64k
RAM_ONLY         RAM (rwx) : ORIGIN = 0x00200000, LENGTH =  64k-0x100
RAM_ONLY         WA  (r)   : ORIGIN = 0x0020FF00, LENGTH =  0x0100  /* Parameter Speicher */
SIM_ONLY         ROM (rx)  : ORIGIN = 0x00000000, LENGTH = 256k 
SIM_ONLY         RAM (rwx) : ORIGIN = 0x00200000, LENGTH =  64k-0x100
SIM_ONLY         WA  (r)   : ORIGIN = 0x0020FF00, LENGTH =  0x100   /* Parameter Speicher */
SAMBA_ONLY       ROM (rwx) : ORIGIN = 0x00202000, LENGTH =  56k 
SAMBA_ONLY       RAM (rwx) : ORIGIN = 0x00200000, LENGTH =   8k
SAMBA_ONLY       WA  (r)   : ORIGIN = 0x00200000, LENGTH =   8k  /* Samba Working Area */
RAMOPENOCD_ONLY  RAM (rwx) : ORIGIN = 0x00200000, LENGTH =  64k-4k
RAMOPENOCD_ONLY  WA  (r)   : ORIGIN = 0x0020F000, LENGTH =   4k  /* Debugger Working Area */
}

/* The size of the stack used by the application. NOTE: you need to adjust  */
STACK_SIZE = 3000;

/* The size of the heap used by the application. NOTE: you need to adjust   */
/* 1048 ist die Größe des ersten sbrk() Aufrufes, aufgrund der iprint()     */
//HEAP_SIZE = 1024;
HEAP_SIZE = 4096;

SECTIONS {
 
    .ramvect : {                        /* used for vectors remapped to RAM */
        __ram_start = .;                /* muss immer auf 0x200000 liegen   */
ROM_ONLY        . = 0x40;
RAM_ONLY        . = 0x40;
SAMBA_ONLY      . = 0x40;
/*In der Sim-Version wird innerhalb von startup.s der Speicherplatz reserviert */
RAMOPENOCD_ONLY . = 0x40;
    } >RAM

    .reset : {
        __rom_start = .;	    
        *startup.o (.text)  /* startup code (ARM vectors and reset handler) */
        . = ALIGN(0x4);
ROM_ONLY        } >ROM
RAM_ONLY        } >RAM
SIM_ONLY        } >ROM
SAMBA_ONLY      } >ROM
RAMOPENOCD_ONLY } >RAM

    .fastcode : {
        __fastcode_load = LOADADDR (.fastcode);
        __fastcode_start = .;

        *(.glue_7)                                /* glue arm to thumb code */
        *(.glue_7t)                               /* glue thumb to arm code */
        *(.text.fastcode)   /* all functions explicitly placed in .fastcode */
        *(.text.QF_tick)
        *(.text.QF_run)
        *(.text.QK_schedule_)
        /* add other modules here ... */

        . = ALIGN (4);
        __fastcode_end = .;
ROM_ONLY        } >RAM AT>ROM
RAM_ONLY        } >RAM
SIM_ONLY        } >RAM
SAMBA_ONLY      } >ROM        /* ObjCopy erzeugt nur Abbild vom ROM-Bereich */
SAMBA_ONLY                    /*  daher ROM auf RWX gestellt                */
RAMOPENOCD_ONLY } >RAM

    .text : {
        . = ALIGN(4);
        *(.text)                                   /* .text sections (code) */
        *(.text*)                                 /* .text* sections (code) */
        *(.rodata)           /* .rodata sections (constants, strings, etc.) */
        *(.rodata*)         /* .rodata* sections (constants, strings, etc.) */
        *(.glue_7) /* glue arm to thumb (NOTE: placed already in .fastcode) */
        *(.glue_7t)/* glue thumb to arm (NOTE: placed already in .fastcode) */

        KEEP (*(.init))
        KEEP (*(.fini))

        . = ALIGN(4);
        _etext = .;                         /* global symbol at end of code */
ROM_ONLY        } >ROM
RAM_ONLY        } >RAM
SIM_ONLY        } >ROM
SAMBA_ONLY      } >ROM
RAMOPENOCD_ONLY } >RAM


    .preinit_array : {
        PROVIDE_HIDDEN (__preinit_array_start = .);
        KEEP (*(SORT(.preinit_array.*)))
        KEEP (*(.preinit_array*))
        PROVIDE_HIDDEN (__preinit_array_end = .);
ROM_ONLY        } >ROM
RAM_ONLY        } >RAM
SIM_ONLY        } >ROM
SAMBA_ONLY      } >ROM
RAMOPENOCD_ONLY } >RAM

    .init_array : {
        PROVIDE_HIDDEN (__init_array_start = .);
        KEEP (*(SORT(.init_array.*)))
        KEEP (*(.init_array*))
        PROVIDE_HIDDEN (__init_array_end = .);
ROM_ONLY        } >ROM
RAM_ONLY        } >RAM
SIM_ONLY        } >ROM
SAMBA_ONLY      } >ROM
RAMOPENOCD_ONLY } >RAM

    .fini_array : {
        PROVIDE_HIDDEN (__fini_array_start = .);
        KEEP (*(.fini_array*))
        KEEP (*(SORT(.fini_array.*)))
        PROVIDE_HIDDEN (__fini_array_end = .);
ROM_ONLY        } >ROM
RAM_ONLY        } >RAM
SIM_ONLY        } >ROM
SAMBA_ONLY      } >ROM
RAMOPENOCD_ONLY } >RAM

    .data : {
        __data_load = LOADADDR (.data);
        __data_start = .;
        *(.data)                                          /* .data sections */
        *(.data*)                                        /* .data* sections */
        . = ALIGN(4);
        _edata = .;
ROM_ONLY        } >RAM AT>ROM
RAM_ONLY        } >RAM
SIM_ONLY        } >RAM
SAMBA_ONLY      } >ROM        /* ObjCopy erzeugt nur Abbild vom ROM-Bereich */
SAMBA_ONLY                    /* daher ROM auf RWX gestellt                 */
RAMOPENOCD_ONLY } >RAM

    .bss : {
        __bss_start__ = . ;
        *(.bss)
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;                     /* define a global symbol at bss end */
        __bss_end__ = .;
    } >RAM

    PROVIDE ( end = _ebss );
    PROVIDE ( _end = _ebss );
    PROVIDE ( __end__ = _ebss );

    .heap : {
        __heap_start__ = . ;
        . = . + HEAP_SIZE;
        . = ALIGN(4);
        __heap_end__ = . ;
    } >RAM

    .stack : {
        __stack_start__ = . ;
        . += STACK_SIZE;
        . = ALIGN (4);
        __c_stack_top__ = . ;
        __stack_end__ = .;
    } >RAM

	.schrott_ram_end__ : {
			__ram_end = .;
	} >RAM

	.schrott_rom_end__ : {
			__rom_end = .;
ROM_ONLY        } >ROM
RAM_ONLY        } >RAM
SIM_ONLY        } >ROM
SAMBA_ONLY      } >ROM
RAMOPENOCD_ONLY } >RAM

    /* Remove information from the standard libraries */
    /DISCARD/ : {
        libc.a ( * )
        libm.a ( * )
        libgcc.a ( * )
    }
}
