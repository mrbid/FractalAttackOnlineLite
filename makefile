ifeq ($(generic), true)
	CFLAGS = -I inc -Ofast
else
	CFLAGS = -I inc -Ofast -march=native
endif

LDFLAGS = -lglfw -lcurl -lm -lpthread

.PHONY: all clean release
all: fractalattackonline

main.o: main.c inc/gl.h inc/glfw3.h inc/esAux2.h inc/res.h assets/exo.h assets/rocks.h
	$(CC) $(CFLAGS) -c $< -o $@

glad_gl.o: glad_gl.c inc/gl.h
	$(CC) $(CFLAGS) -c $< -o $@

assets/exo.o: assets/exo.c assets/exo.h
	$(CC) $(CFLAGS) -c $< -o $@

fractalattackonline: main.o glad_gl.o assets/exo.o
	$(CC) $^ $(LDFLAGS) -o $@

run: fractalattackonline
	./fractalattackonline

clean:
	$(RM) fractalattackonline *.o

release: fractalattackonline
	upx fractalattackonline
