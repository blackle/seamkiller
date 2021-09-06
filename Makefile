seamkiller.so : seamkiller.c
	gcc -shared -o $@ $^ -fPIC `pkg-config --cflags --libs gegl-0.4` -I.

.PHONY: install
install :
	cp seamkiller.so ~/.local/share/gegl-0.4/plug-ins/