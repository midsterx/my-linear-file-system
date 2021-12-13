To create the executable (.o) file:	
	gcc myfs.c -o myfs `pkg-config fuse --cflags --libs`
	
To run the code:
	./myfs -o atomic_o_trunc -f mp
	, where mp is the mount point (directory) 
