
serialterm: serialterm.c
	gcc -O2 -std=c11 -Werror -Wall -Wpedantic -o $@ $<

clean:
	rm -f serialterm.exe

