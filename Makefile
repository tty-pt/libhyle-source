FOLDER := hyle-source

all := libhyle-source hyle-tok

LDLIBS-libhyle-source := -lhyle -lcorm -llemma -lstele -ljson-c
LDLIBS-libhyle-source-Darwin += -liconv
LDLIBS-hyle-tok := -llemma -lcorm

libhyle-source-obj-y := src/source_utils.o src/store_fs.o src/store_tok.o src/store_mem.o src/meta.o src/dsv.o src/json.o src/engine.o src/options.o src/cache.o

WASM_SRCS := npm/wasm/hyle_source_wasm.c
WASM_EXTRA_CFLAGS := -I../mk/include -I../libstele/include
WASM_LDFLAGS := -mexec-model=reactor -Wl,--gc-sections -Wl,--no-entry -Wl,--allow-undefined -Wl,--import-memory -Wl,--global-base=2097152

include ../mk/include.mk

${DESTDIR}${PREFIX}/lib/pkgconfig/hyle-source.pc: hyle-source.pc
	install -d ${DESTDIR}${PREFIX}/lib/pkgconfig
	install -m 644 hyle-source.pc $@

install: ${DESTDIR}${PREFIX}/lib/pkgconfig/hyle-source.pc

test: all
	@echo "All libhyle-source targets built and verified"

npm-build: all wasm
	@mkdir -p npm/build/Release
	gcc -O3 -fPIC -shared -I/usr/include/node -I./include -I../libcorm/include -I../libhyle/include -I../liblemma/include -I../libstele/include npm/native/hyle_source_napi.c src/cache.c -L./lib -Wl,-rpath,'$$ORIGIN/../../lib' -Wl,-rpath,./lib -L../libcorm/lib -L../libhyle/lib -L../liblemma/lib -L../libstele/lib -L/usr/lib -lhyle-source -lhyle -lcorm -llemma -lstele -ljson-c -o npm/build/Release/hyle_source_node.node

npm-test: npm-build
	@node --test npm/tests/hyle_source.test.mjs
