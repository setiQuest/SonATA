
 /* ==================================================================
    FILE: "/home/joze/src/tclreadline/tclreadline.h.in"
    LAST MODIFICATION: "Mit, 20 Sep 2000 17:18:12 +0200 (joze)"
    (C) 1998 - 2000 by Johannes Zellner, <johannes@zellner.org>
    $Id: tclreadline.h.in,v 1.1.1.1 2002/02/28 21:23:50 kilsdonk Exp $
    vim:set ft=c:
    ---
    tclreadline -- gnu readline for tcl
    http://www.zellner.org/tclreadline/
    Copyright (c) 1998 - 2000, Johannes Zellner <johannes@zellner.org>
    This software is copyright under the BSD license.
    ================================================================== */  

#ifndef TCLREADLINE_H_
#define TCLREADLINE_H_

#include <tcl.h>

#define TCLRL_LIBRARY        "/home/abhagat/sonata_install/lib/tclreadline9.9.0"

/* VERSION STRINGS */
#define TCLRL_VERSION_STR    "9.9.0"
#define TCLRL_PATCHLEVEL_STR "9.9.0"

/* VERSION NUMBERS */
#define TCLRL_MAJOR      9
#define TCLRL_MINOR      9
#define TCLRL_PATCHLEVEL 0

#ifdef __cplusplus
extern "C" {
#endif
int Tclreadline_Init(Tcl_Interp *interp);
int Tclreadline_SafeInit(Tcl_Interp *interp);
#ifdef __cplusplus
}
#endif

#endif /* TCLREADLINE_H_ */
