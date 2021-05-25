/*
 * operations on IDE disk.
 */

#include "fs.h"
#include "lib.h"
#include <mmu.h>

// Overview:
// 	read data from IDE disk. First issue a read request through
// 	disk register and then copy data from disk buffer
// 	(512 bytes, a sector) to destination array.
//
// Parameters:
//	diskno: disk number.
// 	secno: start sector number.
// 	dst: destination for data read from IDE disk.
// 	nsecs: the number of sectors to read.
//
// Post-Condition:
// 	If error occurred during read the IDE disk, panic. 
// 	
// Hint: use syscalls to access device registers and buffers
void ide_read(u_int diskno, u_int secno, void *dst, u_int nsecs)
{
	// 0x200: the size of a sector: 512 bytes.
	int offset_begin = secno * 0x200;
	int offset_end = offset_begin + nsecs * 0x200;
	int offset = 0;
	const u_int base = 0x13000000;

	while (offset_begin + offset < offset_end) {
        // Your code here
        // error occurred, then panic.
		u_int tmp = offset_begin + offset;
		u_int dev_id = 0;
		u_int enable = 0;
		u_int status;

		if (syscall_write_dev(&tmp, base, 4))) user_panic("read IDE disk error");
		if (syscall_write_dev(&dev_id, base + 0x0010, 4)) user_panic("read IDE disk error");
		if (syscall_write_dev(&enable, base + 0x0020, 4)) user_panic("read IDE disk error");
		if (syscall_read_dev(&status, base + 0x0030, 4)) user_panic("read IDE disk error");
		if (status == 0) user_panic("read IDE disk error");
		if (syscall_read_dev(dst + offset, base + 0x4000, 0x200)) user_panic("read IDE disk error");

		offset += 0x200;
	}
}


// Overview:
// 	write data to IDE disk.
//
// Parameters:
//	diskno: disk number.
//	secno: start sector number.
// 	src: the source data to write into IDE disk.
//	nsecs: the number of sectors to write.
//
// Post-Condition:
//	If error occurred during read the IDE disk, panic.
//	
// Hint: use syscalls to access device registers and buffers
void ide_write(u_int diskno, u_int secno, void *src, u_int nsecs)
{
    // Your code here
	int offset_begin = secno * 0x200;
	int offset_end = offset_begin + nsecs * 0x200;
	int offset = 0;
	const u_int base = 0x13000000;

	// DO NOT DELETE WRITEF !!!
	writef("diskno: %d\n", diskno);

	while (offset_begin + offset < offset_end) {
	    // copy data from source array to disk buffer.
		// if error occur, then panic.
		u_int tmp = offset_begin + offset;
		u_int dev_id = 0;
		u_int enable = 1;
		u_int status;

		if (syscall_write_dev(src + offset, base + 0x4000, 0x200)) user_panic("write IDE disk error");
		if (syscall_write_dev(&tmp, base, 4))) user_panic("write IDE disk error");
		if (syscall_write_dev(&dev_id, base + 0x0010, 4)) user_panic("write IDE disk error");
		if (syscall_write_dev(&enable, base + 0x0020, 4)) user_panic("write IDE disk error");
		if (syscall_read_dev(&status, base + 0x0030, 4)) user_panic("write IDE disk error");
		if (status == 0) user_panic("write IDE disk error");

		offset += 0x200;
	}
}
