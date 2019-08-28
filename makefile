all:
	make -f template.mk TARGET=wm EXCLUDE=wmevent
	make -f template.mk TARGET=wmevent EXCLUDE=wm

debug:
	make debug -f template.mk TARGET=wm EXCLUDE=wmevent
	make debug -f template.mk TARGET=wmevent EXCLUDE=wm

release:
	make release -f template.mk TARGET=wm EXCLUDE=wmevent
	make release -f template.mk TARGET=wmevent EXCLUDE=wm

clean:
	make clean -f template.mk TARGET=wm EXCLUDE=wmevent
	make clean -f template.mk TARGET=wmevent EXCLUDE=wm

setup:
	make setup -f template.mk TARGET=wm EXCLUDE=wmevent
	make setup -f template.mk TARGET=wmevent EXCLUDE=wm

install:
	make install -f template.mk TARGET=wm EXCLUDE=wmevent
	make install -f template.mk TARGET=wmevent EXCLUDE=wm
