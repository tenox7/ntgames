all: sokoban.exe

sokoban.exe: sokoban.obj sokoban.res
	link.exe /nologo /subsystem:windows sokoban.obj sokoban.res user32.lib gdi32.lib

sokoban.obj: sokoban.c
	cl.exe /nologo /c /O2 /W3 sokoban.c

sokoban.res: sokoban.rc forklift.bmp crate.bmp truck-empty.bmp truck-full.bmp wall.bmp forklift.ico
	rc.exe sokoban.rc

clean:
	-del /f /q sokoban.exe sokoban.obj sokoban.res  *.pdb *.ilk del *.bak *.tmp err.out

