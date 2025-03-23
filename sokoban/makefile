all: sokoban.exe

# Separate target for generating level data
levels: genlevels.exe
	genlevels.exe

# Build genlevels only for host platform
genlevels.exe: genlevels.c
	cl.exe /nologo /O1 genlevels.c

# Generated files as prerequisites but not targets
levels.h levels.rc: 

sokoban.exe: sokoban.obj sokoban.res
	link.exe /nologo /subsystem:windows sokoban.obj sokoban.res user32.lib gdi32.lib

sokoban.obj: sokoban.c levels.h
	cl.exe /nologo /c /O2 /W3 sokoban.c

sokoban.res: sokoban.rc forklift.bmp crate.bmp truck-empty.bmp truck-full.bmp wall.bmp forklift.ico levels.rc
	rc.exe sokoban.rc

clean:
	-del /f /q sokoban.exe sokoban.obj sokoban.res genlevels.obj levels.h levels.rc *.pdb *.ilk del *.bak *.tmp err.out

clean_all: clean
	-del /f /q genlevels.exe