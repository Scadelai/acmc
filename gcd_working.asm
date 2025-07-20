j 38
Func gcd:
1-addi r30 r30 3
2-sw r31 r30 1
3-sw r1 r30 2
4-sw r2 r30 3
5-lw r30 r4 3
6-sub r59 r4 r0
7-beq r59 r0 equal_0
8-li r5 0
9-j end_0
10-equal_0:
11-li r5 1
12-end_0:
13-beq r5 r0 L0
14-lw r30 r4 2
15-move r28 r4
16-j L1
17-L0:
18-lw r30 r4 3
19-move r1 r4
20-lw r30 r4 2
21-lw r30 r5 2
22-lw r30 r6 3
23-div r5 r6
24-mfhi r7
25-lw r30 r5 3
26-mult r7 r5
27-mflo r6
28-sub r5 r4 r6
29-move r2 r5
30-jal gcd
31-move r4 r28
32-move r28 r4
33-L1:
34-lw r30 r31 1
35-addi r30 r30 -3
36-jr r31
Func main:
37-addi r30 r30 3
38-sw r31 r30 1
39-input r28
40-move r4 r28
41-sw r4 r30 2
42-input r28
43-move r4 r28
44-sw r4 r30 3
45-lw r30 r4 2
46-move r1 r4
47-lw r30 r4 3
48-move r2 r4
49-jal gcd
50-move r4 r28
51-move r1 r4
52-outputreg r1
53-lw r30 r31 1
54-addi r30 r30 -3
55-jr r31
