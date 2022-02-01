
# Das erste Target im Makefile ist das Haupttarget
# Wir wollen mehrere Executables erzeugen.
alles: listen_to_eth

# Linken der Objects zum Executable
listen_to_eth: listen_to_eth.o
	gcc -Wall -lrt listen_to_eth.o -o listen_to_eth -lm

# Compilieren c zu o
listen_to_eth.o: listen_to_eth.c plc_homeplug.h
	gcc -Wall -c listen_to_eth.c

    
# Ergebnisse löschen
clean:
	rm *.o
	rm listen_to_eth
