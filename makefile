#
# Makefile reference from etherdfs, requires Open Watcom v2
# Copyright (C) 2017 Mateusz Viste
# Copyright (C) 2021 Davide Bresolin
#
# http://etherdfs.sourceforge.net
#
# OwnFMDRV Makefile
#
# Copyright (C) 2024 Jeong Jinwook
#

## WATCOM Environment

WATCOM = C:\WATCOM
PATH    = $(WATCOM)\binnt64;$(WATCOM)\binnt;$(WATCOM)\binw;$(PATH)
INCLUDE=$(WATCOM)\H;$(WATCOM)\H\NT;$(WATCOM)\H\NT\DIRECTX;$(WATCOM)\H\NT\DDK;%INCLUDE%
EDPATH=$(WATCOM)\EDDAT
WHTMLHELP=$(WATCOM)\BINNT\HELP
WIPFC=$(WATCOM)\WIPFC
LIB=$(WATCOM)\lib286\dos;$(WATCOM)\lib286;$(WATCOM)\lib386\dos;$(WATCOM)\lib386

## Individual Files

### OWNFMDRV
OWNFMDRV_OBJ   =  CHINT.OBJ CD_RESID.OBJ DOSUTIL.OBJ INTERUP.OBJ OWNFMDRV.OBJ CD_TRANS.OBJ INIPARSE.OBJ
OWNFMDRV_OBJ_COMMA   = CHINT.OBJ,CD_RESID.OBJ,DOSUTIL.OBJ,INTERUP.OBJ,OWNFMDRV.OBJ,CD_TRANS.OBJ,INIPARSE.OBJ
OWNFMDRV_EXE   = OWNFMDRV.EXE
OWNFMDRV_MAP   = OWNFMDRV.MAP

# -0      generate code for 8086
# -s      disable stack overflow checks
# -d0     don't include debugging information
# -ms     small memory model
# -wx     set warning level to max
# -os     optimize for size
# -ot     optimize for execution time

# Compiler settings to optimize for execution time
MAX_CDAUDIOTRACK=98 	# MSCDEX audiotrack max
#MAX_CDAUDIOTRACK=42		# Max Koei Soundware tracks
CFLAGS=-0 -s -d0 -ms -wx -ot -DINTSIG_OFF=23 -DOWNFMDRV_VER="1.3.4" -DMAX_CDAUDIOTRACK=$(MAX_CDAUDIOTRACK)

# Compiler settings to optimize for memory footprint
#CFLAGS=-0 -s -d0 -ms -wx -os -DINTSIG_OFF=19


all: setenv $(OWNFMDRV_EXE)

chint.obj: chint086.asm
	wasm -0 chint086.asm -fo=chint.obj -ms

.C.OBJ:
	wcc $(CFLAGS) $<

$(OWNFMDRV_EXE): $(OWNFMDRV_OBJ)
	wlink system dos file $(OWNFMDRV_OBJ_COMMA) LIBP $(LIB) option map=ownfmdrv name ownfmdrv order clname rdata clname code segment begtext segment _text clname far_data clname begdata

setenv:
	SET WATCOM=$(WATCOM)

# system dos    compile to a DOS real-mode application
# option map=   generate a map file
# name          set output file name
# order         set order of program segments

clean:
	del /Q /F $(OWNFMDRV_EXE)
	del /Q /F $(OWNFMDRV_MAP)
	del /Q /F *.OBJ
	del /Q /F *.err
	del /Q /F *.lst