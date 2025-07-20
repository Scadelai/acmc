j 18
Func simple:
1-addi r30 r30 3
2-sw r31 r30 1
3-sw r4 r30 2
4-lw r30 r5 2
5-li r58 5
6-sub r59 r5 r58
7-beq r59 r0 equal_0
8-li r6 0
9-j end_0
equal_0:
10-li r6 1
end_0:
11-beq r6 r0 L0
12-move r28 r7
13-j L1
L0:
14-move r28 r8
L1:
15-lw r30 r31 1
16-subi r30 r30 3
17-jr r31
Func main:
18-addi r30 r30 3
19-sw r31 r30 1
20-jal simple
21-move r4 r28
22-move r1 r4
23-outputreg r1
24-lw r30 r31 1
25-subi r30 r30 3
26-jr r31
