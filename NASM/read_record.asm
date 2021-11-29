.include "record_def.s"
.include "linux.s"

.equ ST_READ_BUFFER, 8
.equ ST_FILEDES, 12

.section .text
.global read_record
.type read_record, @function

# 写入记录
write_record:
  pushl %ebp
  movl %esp, %ebp

  pushl %ebx
  movl ST_FILEDES(%ebp), %ebx
  movl ST_READ_BUFFER(%ebp), %ecx
  movl RECORD_SIZE, %edx
  movl $SYS_READ, %eax
  int $LINUX_SYSCALL

  popl %ebx
  movl %ebp, %esp
  pop %ebp
  ret


# 写入记录
.section .data

record1:
  .ascii "Fredrick\0"
  .rept 31 #填充到40字节
  .byte 0
  .endr

  .ascii "Bartlett\0"
  .rept 31 #填充到40字节
  .byte 0
  .endr

  .ascii "4242 S Prairie\nTusla, ok 55555\0"
  .rept 209 #填充到240字节
  .byte 0
  .endr

  .long 45

  .ascii "test.dat\0"   #要写入的文件名
  .equ ST_FILE_DESCRIPTOR , -4

.global _start

_start: 
  movl %esp, %ebp
  subl $4, %esp
  movl $SYS_OPEN, %eax
  movl $file_name, %ebx
  movl $0101, %ecx
  movl $0666, %edx

  int $LINUX_SYSCALL

  movl %eax, ST_FILE_DESCRIPTOR(%ebp)

  pushl ST_FILE_DESCRIPTOR(%ebp)
  pushl $record1      
  call write_record
  addl %$8, $esp


  movl $SYS_CLOSE, %eax
  movl ST_FILE_DESCRIPTOR(%ebp), %ebx
  int $LINUX_SYSCALL

  movl $SYS_EXIT, %eax
  movl $0, %ebp
  int $LINUX_SYSCALL




