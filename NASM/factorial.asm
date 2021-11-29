.section .data

.section .text

.global start 
.global factorial         # 函数需要共享时，如此定义

_start:
  pushl $4 # 阶乘只有一个参数
  call factorial # call 阶乘函数
  addl $4, %esp # 弹出入栈的参数
  movl %eax, %ebx # 阶乘将结果放回到eax

  movl $1, %eax
  int $0x80


.type factorial, @function
factorial:
  pushl %ebp
  movl %esp, %ebp
  movl 8(%esp), %eax
  cmpl $1, %eax

  je end_factional
  decl %eax
  pushl %eax
  call factorial
  movl 8(%ebp), %ebx
  imull %ebx, %eax

end_factional:
  movl %ebp, %esp
  popl %ebp

  ret



