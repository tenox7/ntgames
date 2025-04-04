all: RISCoban.exe

levels: genlevels.exe
	genlevels.exe

genlevels.exe: genlevels.c
	cl.exe /nologo /O1 genlevels.c

levels.h levels.rc: 

RISCoban.exe: RISCoban.obj RISCoban.res
	link.exe /nologo /subsystem:windows RISCoban.obj RISCoban.res user32.lib gdi32.lib

RISCoban.obj: RISCoban.c levels.h
	cl.exe /nologo /c /O2 /W3 RISCoban.c

RISCoban.res: RISCoban.rc roboarm.bmp mips.bmp axp.bmp ppc.bmp arm.bmp socket.bmp socket-mips.bmp socket-axp.bmp socket-ppc.bmp socket-arm.bmp risc.ico levels.rc
	rc.exe RISCoban.rc

clean:
	-del /f /q RISCoban.exe RISCoban.obj RISCoban.res genlevels.exe genlevels.obj levels.rc *.pdb *.ilk del *.bak *.tmp err.out
