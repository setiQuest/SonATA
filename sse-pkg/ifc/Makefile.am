################################################################################
#
# File:    Makefile.am
# Project: OpenSonATA
# Authors: The OpenSonATA code is the result of many programmers
#          over many years
#
# Copyright 2011 The SETI Institute
#
# OpenSonATA is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
# 
# OpenSonATA is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with OpenSonATA.  If not, see<http://www.gnu.org/licenses/>.
# 
# Implementers of this code are requested to include the caption
# "Licensed through SETI" with a link to setiQuest.org.
# 
# For alternate licensing arrangements, please contact
# The SETI Institute at www.seti.org or setiquest.org. 
#
################################################################################

## Process this file with automake to produce Makefile.in

LDFLAGS = @GPIB_LDFLAGS@
LIBADD = @GPIB_LIBADD@
AUTOMAKE_OPTIONS = foreign

bin_SCRIPTS = \
        ifc

#check_PROGRAMS = 

#TESTS = unitTest.exp

EXTRA_DIST = $(bin_SCRIPTS) Ifc.i unitTest.exp unitTest.cfg

LIB_DEPENDS = $(SSE_INTERFACE_LIB) $(GPIB_INTERFACE_LIB) \
	$(SSEUTIL_LIB)

INCLUDES= -I$(SSE_INC_DIR) -I$(SSE_INTERFACE_INC_DIR) \
        -I$(SSEUTIL_INC_DIR) \
	-I$(GPIB_INTERFACE_INC_DIR) @GPIB_INCLUDES@  \
	-I/usr/include/tcl -I/usr/include/tcl8.3 -I/usr/include/tcl8.2

# build a version of tclsh that has all the libraries
# needed to run the test signal generator equipment
# (GPIB libraries, etc).

bin_PROGRAMS = ifctclsh

ifctclsh_SOURCES = \
	Stx.h \
	Stx.cpp \
	IfcAttnScu.h \
	IfcAttnScu.cpp \
	Ifc.h \
	Ifc.cpp \
	Ifc_wrap.cpp	

ifctclsh_LIBS =  $(LIB_DEPENDS) \
	 $(GPIB_LDFLAGS) $(GPIB_LIBS) -ldl -lnsl

ifctclsh_DEPENDENCIES = $(LIB_DEPENDS)

ifctclsh_LDADD = $(ifctclsh_LIBS)

Ifc_wrap.cpp: Ifc.i
	swig -c++ -tcl -o Ifc_wrap.cpp -ltclsh.i Ifc.i

CLEANFILES = Ifc_wrap.cpp

# Dummy target to please automake.
# The real program main is created by swig with the -ltclsh.i
# option, above.

ifctclsh.cpp: