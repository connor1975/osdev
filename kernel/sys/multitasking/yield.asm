global yield
yield:
mov rax, 17 ; sched_yield
int 0x80
ret