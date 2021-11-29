# 本程序寻找一组数据项中的最大值
# 变量：寄存器有如下用途
# %edi -- 保存正在检测的数据索引
# %ebx -- 已找到的最大数据项
# %eax -- 当前数据项
#
# 使用以下内存位置
# data_items -- 包含数据项，0表示数据结束

.section .data
data_items :
  .long 100, 1, 300, 400, 75, 12, 3, 6, 0         # .long 表示数据类型

.section .text

.global _start
_start:
  movl $0, %edi                   # 将索引0移入到edx寄存器
  movl data_items(,%edi, 4), %eax # 加载数据项的第一个字节
  movl %eax, %ebx                 # 当前只有一个元素，即为最大值

start_loop:                       # 开始循环
  cmpl $0, %eax                   # 比较是否到达末尾
  je   loop_exit                  # 到达末尾跳出循环
  incl %edi                       # 加载下一个值, %edi前进一步
  movl data_items(,%edi,4), %eax  
  cmpl %ebx, %eax                 # 比较%ebx,与%eax的值
  jle start_loop                  # 如果新数据项不大于当前最大值，跳转到循环开始
  movl %eax, %ebx

  jmp start_loop                  # 无条件跳转

loop_exit:
  movl $1, %eax                   # 退出程序，1号指令为sys_exit
  int $0x80



