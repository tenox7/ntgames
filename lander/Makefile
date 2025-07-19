all: lander.exe

lander.obj: lander.c
	cl -c -W3 -O2 -nologo lander.c

lander.exe: lander.obj
	link -out:lander.exe lander.obj kernel32.lib user32.lib gdi32.lib

clean:
	del *.obj
	del lander.exe
