j 33
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
equal_0:
10-li r5 1
end_0:
11-beq r5 r0 L0
12-lw r30 r4 2
13-move r28 r4
14-j L1
L0:
15-lw r30 r4 3
16-move r1 r4
17-lw r30 r4 2
18-lw r30 r5 2
19-lw r30 r6 3
20-div r5 r6
21-mfhi r7
22-lw r30 r5 3
23-mult r7 r5
24-mflo r6
25-sub r5 r4 r6
26-move r2 r5
27-jal gcd
28-move r4 r28
29-move r28 r4
L1:
30-lw r30 r31 1
31-subi r30 r30 3
32-jr r31
Func main:
33-addi r30 r30 3
34-sw r31 r30 1
35-input r28
36-move r4 r28
37-sw r4 r30 2
38-input r28
39-move r4 r28
40-sw r4 r30 3
41-lw r30 r4 2
42-move r1 r4
43-lw r30 r4 3
44-move r2 r4
45-jal gcd
46-move r4 r28
47-move r1 r4
48-outputreg r1
49-lw r30 r31 1
50-subi r30 r30 3
51-jr r31
