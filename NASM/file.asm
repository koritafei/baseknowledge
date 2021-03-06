#### 常数 ####
# 系统调用号
.equ SYS_OPEN, 5
.equ SYS_WRIOTE, 4
.equ SYS_READ, 3
.equ SYS_CLOSE, 6
.equ SYS_EXIT, 1

# 文件打开选项
.equ O_RDONLY, 0
.equ O_CREAT_WRONLY_TRUNC, 03101

# 标准文件描述符
.equ STDIN 0
.equ STDOUT 1
.equ STDERR 2

# 系统中断
.equ LINUX_SYSCALL, 0x80
.equ END_OF_FILE, 0
.equ NUMBER_ARGUMENTS, 2

.section .bss
# 缓冲区
.equ BUFFER_SIZE, 500
.lcomm BUFFER_DATA, BUFFER_SIZE

.section .text
# 栈位置
.equ ST_SIZE_RESERVE, 8
.equ ST_FD_IN, -4
.equ ST_FD_OUT, -8
.equ ST_ARGC, 0 # 参数数目
.equ ST_ARGC_0, 4 # 程序名
.equ ST_ARGC_1, 8 # 输入文件名
.equ ST_ARGC_2, 12 # 输出文件名


.global _start

_start:
         # 保存栈指针
         movl %esp, %ebp
         # 在栈上为文件描述符分配空间
         subl $ST_SIZE_RESERVE, %esp
         
         open_files:
         open_fd_in:
               # 打开系统调用
               movl $SYS_OPEN, %eax
               movl ST_ARGC_1(%ebp), %ebx
               movl $O_RDONLY, %ecx
               movl $0666, %edx
               int $LINUX_SYSCALL

         store_fd_in:
               # 保存返回的文件描述符
               movl %eax, ST_FD_IN(%ebp)

         
         open_fd_out:
               movl $SYS_OPEN, %eax
               movl ST_ARGC_2(%ebp), %ebx
               movl $O_CREAT_WRONLY_TRUNC, %ecx
               movl $0666, %edx
               int $LINUX_SYSCALL

         store_fd_out:
               movl %eax, ST_FD_OUT(%ebx)

         read_loop_begin:
               # 从输入文件中读取一个数据
               movl $SYS_READ, %eax
               movl $ST_FD_IN(%ebp), %ebx
               movl $BUFFER_DATA, %ecx
               movl $BUFFER_SIZE, %edx
               int $LINUX_SYSCALL
               
               cmpl $END_OF_FILE, %eax
               jle end_loop

         continue_loop:
               pushl $BUFFER_DATA
               pushl %eax #缓冲区大小
               call convert_to_upper
               popl %eax
               addl $4, %esp
               
               # 字符写到输出文件
               movl %eax, %edx
               movl $SYS_WRITE, %eax
               movl ST_FD_OUT(%ebp), %edx
               movl $BUFFER_DATA, %ecx
               int $LINUX_SYSCALL

               jmp read_loop_begin
        
         end_loop:
               movl $SYS_CLOSE, %eax
               movl ST_FD_OUT(%ebp), %ebx
               int $LINUX_SYSCALL

               movl $SYS_CLOSE, %eax
               movl ST_FD_IN(%ebp), %ecx
               int $LINUX_SYSCALL

               # 退出
               movl $SYS_EXIT, %eax
               movl $0, %ebx
               int $LINUX_SYSCALL

.equ LOWERCASE_A, 'a'
.equ LOWERCASE_Z, 'z'
.equ UPPER_CONVERSION, 'A' - 'a'

# 栈相关信息
.equ ST_BUFFER_LEN, 8
.equ STBUFFER, 12

convert_to_upper:
         pushl %ebp
         movl %esp, %ebp

         movl ST_BUFFER(%ebp), %eax
         movl ST_BUFFER_LEN(%ebp), %ebx

         movl $0, %edi
         cmpl $0, %ebx
         je end_convert_loop

convert_loop:
         movb (%eax, %edi, 1), %cl
         cmpb $LOWER_CASE_A, %cl
         jl next_byte
         cmpb $LOWER_CASE_Z, %cl
         jg next_byte

         addb $UPPER_COBVERT, %cl
         movl %cl, (%eax, %edi, 1)

next_byte:
         incl %edi
         cmpl %edi, %ebx
         jne convert_loop

end_convert_loop:
         movl %ebp, %esp
         popl %ebp
         ret
         




