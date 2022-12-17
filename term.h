#ifndef term_h
#define term_h

#if defined(MODE_RAMOPENOCD)
//Prototypen aus Datei openOCD/dcc_stdio.c

void dbg_trace_point(      unsigned long   number);
void dbg_write_u32  (const unsigned long  *val, long len);
void dbg_write_u16  (const unsigned short *val, long len);
void dbg_write_u8   (const unsigned char  *val, long len);
void dbg_write_char (               char   msg);
void dbg_write_str  (const          char  *msg);
void _term_hex(unsigned int val, unsigned int places);
void _term_unsigned_worker(unsigned int val, unsigned int places, unsigned int sign);

#define TERM_INIT()           
#define TERM_READ(c)           (*c=0,(int)-1)
#define TERM_CHAR(c)          dbg_write_char(c)
#define TERM_STRING(str)      dbg_write_str(str)
#define TERM_CLEAR(c)         dbg_write_str("\xff")
#define TERM_UPDATE()         dbg_write_str("\n\r")
#define TERM_GOTO_XY(x,y)     
#define TERM_HEX(v,p)         _term_hex(v, p)
#define TERM_UNSIGNED(v,p)    _term_unsigned_worker(v, p, 0)
#define TERM_INT(v,p)         _term_unsigned_worker((v < 0) ? -v : v, p, (v < 0))

#elif defined(MODE_RAM) || defined(MODE_SIM)
//Prototypen aus Datei trace32/udmon3.c
void _term_init(void);
int  _term_read_available(void);
int  _term_read(unsigned char *c);
int  _term_write_possible(void);
void _term_char(int c);
void _term_string(const char *str);
void _term_hex(unsigned int val, unsigned int places);
void _term_unsigned_worker(unsigned int val, unsigned int places, unsigned int sign);

#define TERM_INIT()           _term_init()
#define TERM_READ_AVAILABLE() _term_read_available()
#define TERM_READ(c)          _term_read(c)

#define TERM_WRITE_POSSIBLE() _term_write_possible()
#define TERM_CHAR(c)          _term_char(c)
#define TERM_STRING(str)      _term_string(str)
#define TERM_CLEAR(c)         _term_string("\xff")
#define TERM_UPDATE()         _term_string("\n\r")
#define TERM_GOTO_XY(x,y)     
#define TERM_HEX(v,p)         _term_hex(v, p)
#define TERM_UNSIGNED(v,p)    _term_unsigned_worker(               v, p, 0);
#define TERM_INT(v,p)         _term_unsigned_worker((v < 0) ? -v : v, p, (v < 0));

#define TERM_SIZE 64

#else

#define TERM_INIT() 
#define TERM_READ_AVAILABLE() 
#define TERM_READ(c)           (*c=0,(int)-1)

#define TERM_WRITE_POSSIBLE() 
#define TERM_CHAR(c)           ((void)c)
#define TERM_STRING(str)       ((void)str)
#define TERM_CLEAR(c)          ((void)c))
#define TERM_UPDATE()         
#define TERM_GOTO_XY(x,y)      ((void)x,(void)y)
#define TERM_HEX(v,p)          ((void)v,(void)p)
#define TERM_UNSIGNED(v,p)     ((void)v,(void)p)
#define TERM_INT(v,p)          ((void)v,(void)p)

#endif

#endif