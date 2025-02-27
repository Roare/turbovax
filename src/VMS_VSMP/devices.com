$!
$!  DEVICES.COM - set proper CPU affinity for devices supported by VAX MP
$!                as required for virtual multiprocessing
$!
$ PROC_FILE = F$ENVIRONMENT("PROCEDURE")
$ PROC_DIR = F$PARSE(PROC_FILE,,,"DEVICE","SYNTAX_ONLY") + -
             F$PARSE(PROC_FILE,,,"DIRECTORY","SYNTAX_ONLY")
$ VSMP :== $'PROC_DIR'VSMP
$ SAY := WRITE SYS$OUTPUT
$!
$ DC_DISK = 1
$ DC_TAPE = 2
$ DC_CARD = 65
$ DC_TERM = 66
$ DC_LP = 67
$!
$ DT_CR11 = 1
$ DT_LP11 = 1
$ DT_RL01 = 9
$ DT_RL02 = 10
$ DT_RX02 = 11
$ DT_RX04 = 12
$ DT_TS11 = 4
$ DT_MW_TSV05 = 14
$!
$ DEV = "_CRA0"
$ IF F$GETDVI(DEV, "EXISTS")
$ THEN
$ IF (F$GETDVI(DEV, "REMOTE_DEVICE") .EQS. "FALSE") .AND. -
     (F$GETDVI(DEV, "DEVCLASS") .EQ. DC_CARD) .AND. -
     (F$GETDVI(DEV, "DEVTYPE") .EQ. DT_CR11)
$ THEN
$     CALL SETAF DEV
$ ENDIF
$ ENDIF
$!
$ DEV = "_LPA0"
$ IF F$GETDVI(DEV, "EXISTS")
$ THEN
$ IF (F$GETDVI(DEV, "REMOTE_DEVICE") .EQS. "FALSE") .AND. -
     (F$GETDVI(DEV, "DEVCLASS") .EQ. DC_LP) .AND. -
     (F$GETDVI(DEV, "DEVTYPE") .EQ. DT_LP11)
$ THEN
$     CALL SETAF DEV
$ ENDIF
$ ENDIF
$!
$ DEV = "_DLA0"
$ IF F$GETDVI(DEV, "EXISTS")
$ THEN 
$ IF (F$GETDVI(DEV, "REMOTE_DEVICE") .EQS. "FALSE") .AND. -
     (F$GETDVI(DEV, "DEVCLASS") .EQ. DC_DISK) .AND. -
     ((F$GETDVI(DEV, "DEVTYPE") .EQ. DT_RL01) .OR. (F$GETDVI(DEV, "DEVTYPE") .EQ. DT_RL02))
$ THEN
$     CALL SETAF DEV
$ ENDIF
$ ENDIF
$!
$ DEV = "_DYA0"
$ IF F$GETDVI(DEV, "EXISTS")
$ THEN 
$ IF (F$GETDVI(DEV, "REMOTE_DEVICE") .EQS. "FALSE") .AND. -
     (F$GETDVI(DEV, "DEVCLASS") .EQ. DC_DISK) .AND. -
     ((F$GETDVI(DEV, "DEVTYPE") .EQ. DT_RX02) .OR. (F$GETDVI(DEV, "DEVTYPE") .EQ. DT_RX04))
$ THEN
$     CALL SETAF DEV
$ ENDIF
$ ENDIF
$!
$ DEV = "_MSA0"
$ IF F$GETDVI(DEV, "EXISTS")
$ THEN 
$ IF (F$GETDVI(DEV, "REMOTE_DEVICE") .EQS. "FALSE") .AND. -
     (F$GETDVI(DEV, "DEVCLASS") .EQ. DC_TAPE) .AND. -
     ((F$GETDVI(DEV, "DEVTYPE") .EQ. DT_TS11) .OR. (F$GETDVI(DEV, "DEVTYPE") .EQ. DT_MW_TSV05))
$ THEN
$     CALL SETAF DEV
$ ENDIF
$ ENDIF
$!
$ CALL TMUX "TXA"
$ CALL TMUX "TXB"
$ CALL TMUX "TXC"
$ CALL TMUX "TXD"
$ CALL TMUX "TXE"
$ CALL TMUX "TXF"
$ CALL TMUX "TXG"
$ CALL TMUX "TXH"
$!
$ CALL TMUX "TTA"
$ CALL TMUX "TTB"
$ CALL TMUX "TTC"
$ CALL TMUX "TTD"
$ CALL TMUX "TTE"
$ CALL TMUX "TTF"
$ CALL TMUX "TTG"
$ CALL TMUX "TTH"
$!
$ EXIT
$!
$TMUX: SUBROUTINE
$ DEV = "_" + P1 + "0"
$ IF F$GETDVI(DEV, "EXISTS")
$ THEN 
$ IF (F$GETDVI(DEV, "REMOTE_DEVICE") .EQS. "FALSE") .AND. -
     (F$GETDVI(DEV, "DEVCLASS") .EQ. DC_TERM)
$ THEN
$     CALL SETAF DEV
$ ENDIF
$ ENDIF
$ ENDSUBROUTINE
$!
$SETAF: SUBROUTINE
$ ADEV = F$EXTRACT(1, 3, DEV)
$!SAY "Setting affinity for " + ADEV + " ..."
$ VSMP SET AFFINITY 'ADEV'* /CPU=PRIMARY
$ ENDSUBROUTINE
