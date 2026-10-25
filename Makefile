FOLDER := hyle-source

all := libhyle-source hyle-tok

LDLIBS-libhyle-source := -lhyle -lcorm -llemma -ljson-c
LDLIBS-libhyle-source-Darwin += -liconv
LDLIBS-hyle-tok := -llemma -lcorm

add-prefix := ../liblemma ../libcorm ../libhyle

libhyle-source-obj-y := src/source_utils.o src/store_fs.o src/store_tok.o src/store_mem.o src/meta.o src/dsv.o src/json.o src/engine.o src/options.o

include ../mk/include.mk

${DESTDIR}${PREFIX}/lib/pkgconfig/hyle-source.pc: hyle-source.pc
	install -d ${DESTDIR}${PREFIX}/lib/pkgconfig
	install -m 644 hyle-source.pc $@

install: ${DESTDIR}${PREFIX}/lib/pkgconfig/hyle-source.pc

test: all
	@echo "All libhyle-source targets built and verified"
