all:
	rc /r ntrigue.rc
	cl.exe /O2 /nologo ntrigue.c ntrigue.res gdi32.lib user32.lib winmm.lib

clean:
	-del ntrigue.exe
	-del ntrigue.obj
	-del ntrigue.res