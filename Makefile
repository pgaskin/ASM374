CC      = clang
CC_HOST = $(CC)
CC_WASM = $(CC) --target=wasm32
CC_WIN  = x86_64-w64-mingw32-clang # llvm-mingw-20220906-msvcrt-ubuntu-18.04-x86_64

CFLAGS       = -std=c11 -pedantic -Wall -Wextra -Wno-unused-function -fvisibility=hidden
CFLAGS_WASM  = -mcpu=mvp -Oz -nostdlib -fshort-enums -flto -s # note: short-enums reduces code size, and is safe for us since we store temporary values as a different type before casting to an enum
CFLAGS_HOST  =
CFLAGS_WIN   =
LDFLAGS      =
LDFLAGS_WASM = -Wl,--no-entry -Wl,--export-dynamic -Wl,--strip-all
LDFLAGS_HOST =
LDFLAGS_WIN  = -static

all: asm374 asm374_test asm374.exe asm374.dist.html

asm374: asm374.c
	$(CC_HOST) $(LDFLAGS) $(LDFLAGS_HOST) $(CFLAGS) $(CFLAGS_HOST) -o $@ $<

asm374_test: asm374.c
	$(CC_HOST) $(LDFLAGS) $(LDFLAGS_HOST) $(CFLAGS) $(CFLAGS_HOST) -DTESTS -o $@ $<

asm374.wasm: asm374.c
	$(CC_WASM) $(LDFLAGS) $(LDFLAGS_WASM) $(CFLAGS) $(CFLAGS_WASM) -o $@ $<

asm374.exe: asm374.c
	$(CC_WIN) $(LDFLAGS) $(LDFLAGS_WIN) $(CFLAGS) $(CFLAGS_WIN) -o $@ $<

asm374.dist.js: asm374.js asm374.wasm
	sed -E 's:(/\*\*/")(.*)("/\*\*/):\1data\:application/wasm;base64,'$$(base64 -w0 asm374.wasm)'\3:g' asm374.js > asm374.dist.js

asm374.dist.html: asm374.html asm374.dist.js
	sed -E 's:(/\*\*/")(.*)("/\*\*/):\1data\:text/javascript;base64,'$$(base64 -w0 asm374.dist.js)'\3:g' asm374.html > asm374.dist.html

test: asm374_test
	./asm374_test

icon: # moka application-x-executable + hsl(-72, 270%, 80%)
	curl -L https://github.com/snwh/moka-icon-theme/raw/master/src/bitmaps/A/application-x-executable.svg | sed -e 's:#aa89aa:hsl(228,43%,48%):g' -e 's:#c9b2d6:hsl(206,84%,61%):g' -e 's:#a38ca6:hsl(221,35%,48%):g' -e 's:#775e7a:hsl(221,35%,33%):g' | inkscape -p -i rect16x16 -o favicon.png
	optipng -o7 -strip all favicon.png
	echo "data:image/png;base64,$$(base64 -w0 favicon.png)"
	rm favicon.png

clean:
	rm -f asm374 asm374_test *.exe *.wasm *.dist.js *.dist.html

.PHONY: all clean test icon
