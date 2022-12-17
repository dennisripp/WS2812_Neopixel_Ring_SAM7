#ifndef main_h
#define main_h
#include <stdint.h>

//ARM-Register
//R0-R12 = User Registers 
//R13 = StackPointer (SP)
//R14 = Link Register (LR) When a call is made to a function the return address is automatically
//                         stored in the link register an dis immediately availabe on return from
//                         the function. this allows quick entry and return into a "leaf" function
//                         (a function that is not going to call further functions). If the function
//                         is part of a brach (i.e. it is going to call other functions) the the
//                         link register must be preserved on the stack
//R15 = Program counter (PC)

//ASCII-Zeichen
//\a The “alert” character, Ctrl-g, ASCII code 7 (BEL). (This usually makes some sort of audible noise.)
//\b Backspace, Ctrl-h, ASCII code 8 (BS).
//\f Formfeed, Ctrl-l, ASCII code 12 (FF).
//\n Newline, Ctrl-j, ASCII code 10 (LF).
//\r Carriage return, Ctrl-m, ASCII code 13 (CR).
//\t Horizontal TAB, Ctrl-i, ASCII code 9 (HT).
//\v Vertical tab, Ctrl-k, ASCII code 11 (VT).
//\nnn The octal value nnn, where nnn stands for 1 to 3 digits between ‘0’ and ‘7’. For example, the code for the ASCII ESC (escape) character is ‘\033’.
//\xhh...The hexadecimal value hh, where hh stands for a sequence of hexadecimal digits (‘0’–‘9’, and either ‘A’–‘F’ or ‘a’–‘f’). 
//       Like the same construct in ISO C, the escape sequence continues until 
//       the first nonhexadecimal digit is seen. (c.e.) However, using more 
//       than two hexadecimal digits produces undefined results. 
//       (The ‘\x’ escape sequence is not allowed in POSIX awk.)
//\/ A literal slash (necessary for regexp constants only). This sequence 
//       is used when you want to write a regexp constant that contains a 
//       slash. Because the regexp is delimited by slashes, you need to 
//       escape the slash that is part of the pattern, in order to tell 
//       awk to keep processing the rest of the regexp.
//\"  A literal double quote (necessary for string constants only). 
//       This sequence is used when you want to write a string constant 
//       that contains a double quote. Because the string is delimited by 
//       double quotes, you need to escape the quote that is part of 
//       the string, in order to tell awk to keep processing the rest
//       of the string.#define ANSI_BLACK   30

                                                         /* Main Clock [Hz] */
#define MAINCK            18432000
                                     /* Maseter Clock (PLLRC div by 2) [Hz] */
#define MCK               47923200
                                             /* System clock tick rate [Hz] */
#define BSP_TICKS_PER_SEC 1000

extern uint32_t __stack_start__[];   //Definiert in link.ld
extern uint32_t __stack_end__;       //Definiert in link.ld
#define STACK_FILL 0x11111111

static __inline__ void stack_fill(void) __attribute__((always_inline));
static __inline__ void stack_fill(void)
{
	         uint32_t *ptr;
	register uint32_t *sp asm("r13");
	for(ptr=&__stack_start__[0];ptr<sp;ptr++)
		*ptr=STACK_FILL;
}

static __inline__ int32_t stack_check(void) __attribute__((always_inline));
static __inline__ int32_t stack_check(void)
{
	         uint32_t *ptr;
//	register uint32_t *sp asm("r13");
	for(ptr=&__stack_start__[0];*ptr==STACK_FILL;++ptr);
	return (int32_t)(ptr-&__stack_start__[0]);
}

#endif
