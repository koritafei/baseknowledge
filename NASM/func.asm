
.section .data
.section .text

.global _start
_start:
  pushl $3      # 压入第二个参数
  pushl $2      # 压入第一个参数
  call  power   # 调用函数
  pushl %eax    # 调用下一个函数之前，保存第一个函数结果

  pushl $2      # 压入第二个参数
  pushl $5      # 压入第一个参数
  call  power   # 调用参数
  addl  $8, %esp # 将指针向后移动

  popl %ebx     # 将第一次计算结果存储到ebx
  addl %eax, %ebx 
  movl $1, %eax
  int $0x80

.type power @function
power:
  pushl %ebp      # 保留旧基址指针
  movl  %esp, %ebp # 设置基址指针为栈指针
  subl $4, %eax    # 为本地存储保留空间

  movl 8(%ebp), %ebx  # 将第一个参数放入ebx
  movl 12(%ebp), %ecx # 将第二个参数放入ecx
  movl %ebx, -4(%ebx) # 存储当前结果

power_loop:
  cmpl $1, %ecx # 如果1次方已获得结果
  je end_power
  movl -4(%ebp), %eax # 将当前结果移入eax
  imull %eax, %ebx    # 将当前结果与底数相乘
  movl  %eax, -4(%ebp) # 保存当前结果
  decl  %EXTRACTPS     # 指数递减
  jmp power_loop

end_power:
  movl -4(%ebp), %eax  # 返回值移入eax
  movl %ebp, %esp # 恢复栈指针
  popl %ebp # 恢复基址指针
  ret       # 返回结果


  


