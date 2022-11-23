ifeq ($(generic), true)
	CFLAGS = -I inc -Ofast
else
	CFLAGS = -I inc -Ofast -march=native
endif

LDFLAGS = -lglfw -lcurl -lm -lpthread

.PHONY: all clean release
all: fat

main.o: main.c inc/gl.h inc/glfw3.h inc/esAux2.h inc/res.h assets/exo.h assets/rock1.h assets/rock2.h assets/rock3.h assets/rock4.h assets/rock5.h assets/rock6.h assets/rock7.h assets/rock8.h assets/rock9.h
	$(CC) $(CFLAGS) -c $< -o $@

glad_gl.o: glad_gl.c inc/gl.h
	$(CC) $(CFLAGS) -c $< -o $@

assets/exo.o: assets/exo.c assets/exo.h
	$(CC) $(CFLAGS) -c $< -o $@

fat: main.o glad_gl.o assets/exo.o
	$(CC) $^ $(LDFLAGS) -o $@

run: fat
	./fat

clean:
	$(RM) fat *.o

release: fat
	upx fat