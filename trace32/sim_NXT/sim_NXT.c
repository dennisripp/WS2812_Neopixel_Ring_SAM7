#include "simul.h"
#include "AT91SAM7S64.h"
#include "sim_NXT.h"
#include <string.h>

/**************************************************************************

	Local port structure
	
**************************************************************************/

/**************************************************************************

	Entry point of the Model

**************************************************************************/

int SIMULAPI SIMUL_Init(simulProcessor processor, simulCallbackStruct * cbs)
{
	void *nxt;
	
    strcpy(cbs->x.init.modelname, "AT91SAM7 - NXT Port from '"__DATE__"'");

	SIMUL_Printf(processor,"%s loading...\n",cbs->x.init.modelname);
	
//    if (cbs->x.init.argc != 3)
//	{
//		SIMUL_Warning(processor, "parameters: <address> <portnumber>");
//		return SIMUL_INIT_FAIL;
//    }

	nxt=NXT_Init(processor);
	AIC_PortInit(processor);
    PIT_PortInit(processor);
    SPI_PortInit(processor,nxt);
 	TWI_PortInit(processor,nxt);
	PIO_PortInit(processor,nxt);

	SIMUL_Printf(processor,"%s loaded\n",cbs->x.init.modelname);

    return SIMUL_INIT_OK;
}
