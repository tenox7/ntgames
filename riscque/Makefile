all:
	rc /r riscque.rc
	cl.exe /O2 /nologo riscque.c riscque.res gdi32.lib user32.lib winmm.lib

clean:
	-del riscque.exe
	-del riscque.obj
	-del riscque.res