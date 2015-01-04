/*
 * Constable: constable.c
 * (c)2002 by Marek Zelem <marek@terminus.sk>
 * $Id: constable.c,v 1.2 2002/10/23 10:25:43 marek Exp $
 */

#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include "constable.h"

char *retprintf( const char *fmt, ... )
{ va_list ap;
    static char buf[4096];

    va_start(ap,fmt);
    vsnprintf(buf, 4000, fmt, ap);
    va_end(ap);
    return(buf);
}

int init_error( const char *fmt, ... )
{ va_list ap;
    char buf[4096];

    va_start(ap,fmt);
    vsnprintf(buf, 4000, fmt, ap);
    va_end(ap);
    sprintf(buf+strlen(buf),"\n");
    write(1,buf,strlen(buf));
    return(-1);
}

#ifdef DEBUG_TRACE
char runtime_file[64];
char runtime_pos[12];
#endif
int runtime( const char *fmt, ... )
{ va_list ap;
    char buf[4096];

#ifndef DEBUG_TRACE
    sprintf(buf,"Runtime error : ");
#else
    sprintf(buf,"Runtime error [\"%s\" %s]: ",runtime_file,runtime_pos);
#endif

    va_start(ap,fmt);
    vsnprintf(buf+strlen(buf), 4000, fmt, ap);
    va_end(ap);
    //	medusa_printlog("%s",buf);
    sprintf(buf+strlen(buf),"\n");
    write(1,buf,strlen(buf));
    return(0);
}

int fatal( const char *fmt, ... )
{ va_list ap;
    char buf[4096];

    sprintf(buf,"Fatal error : ");

    va_start(ap,fmt);
    vsnprintf(buf+strlen(buf), 4000, fmt, ap);
    va_end(ap);
    //	medusa_printlog("%s",buf);
    sprintf(buf+strlen(buf),"\n");
    write(1,buf,strlen(buf));
    exit(-1);
    return(0);
}


