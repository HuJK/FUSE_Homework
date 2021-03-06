COMPILER = gcc
FILESYSTEM_FILES = lsysfs.cpp

build: $(FILESYSTEM_FILES)
	$(COMPILER) $(FILESYSTEM_FILES) -o lsysfs `pkg-config fuse --cflags --libs` -lstdc++
	echo 'To Mount: ./lsysfs -f [mount point]'

clean:
	rm ssfs
