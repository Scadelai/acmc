0-j 27
R
Func gcd:
1-sw r31 r30 1
2-addi r30 r30 1
3-# Parameter u in r1
4-# Parameter v in r2
5-sub r59 r2 r0
6-bne r59 r0 L0
7-move r28 r1
8-lw r30 r31 1
9-jr r31
10-j L1
11-# Label L0:
12-div r1 r2
13-mflo r9
14-mult r9 r2
15-mflo r10
16-sub r11 r1 r10
17-move r28 r2
18-move r28 r11
19-jal gcd
20-move r12 r28
21-move r28 r12
22-lw r30 r31 1
23-jr r31
24-# Label L1:
25-lw r30 r31 1
26-jr r31
CEHOLDER
Func main:
27-sw r31 r30 1
28-addi r30 r30 1
29-input r28
30-move r15 r28
31-move r4 r15
32-input r28
33-move r17 r28
34-move r5 r17
35-move r28 r4
36-move r28 r5
37-jal gcd
38-move r19 r28
39-move r28 r19
40-outputreg r28
41-lw r30 r31 1
42-jr r31
