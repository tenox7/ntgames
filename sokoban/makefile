all: sokoban.exe

sokoban.exe: sokoban.obj sokoban.res
	link.exe /nologo /subsystem:windows sokoban.obj sokoban.res user32.lib gdi32.lib

sokoban.obj: sokoban.c levels.h
	cl.exe /nologo /c /O2 /W3 sokoban.c

genlevels.exe: genlevels.c
	cl.exe /nologo /O1 genlevels.c

levels.h levels.rc: genlevels.exe
	genlevels.exe

sokoban.res: sokoban.rc forklift.bmp crate.bmp truck-empty.bmp truck-full.bmp wall.bmp forklift.ico levels.rc
	rc.exe sokoban.rc

clean:
	-del /f /q sokoban.exe sokoban.obj sokoban.res genlevels.exe genlevels.obj levels.h levels.rc *.pdb *.ilk del *.bak *.tmp err.out

