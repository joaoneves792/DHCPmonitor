CFLAGS = -Wall -std=c99 
LIBS = `pkg-config --cflags --libs glib-2.0 libnotify gtk+-2.0`

all: 
	gcc $(CFLAGS) -o DHCPmon DHCPmon.c $(LIBS)

clean: 	
	rm *.o
	rm DHCPmon
