j 54
Func gcd:
1-addi r30 r30 6
2-sw r31 r30 1
3-sw r1 r30 2
4-sw r2 r30 3
5-lw r4 r30 2
6-move r1 r4
7-outputreg r1
8-lw r4 r30 3
9-move r1 r4
10-outputreg r1
11-lw r4 r30 2
12-lw r5 r30 3
13-div r4 r5
14-mfhi r6
15-sw r6 r30 5
16-lw r4 r30 5
17-move r1 r4
18-outputreg r1
19-lw r4 r30 5
20-lw r5 r30 3
21-mult r4 r5
22-mflo r6
23-sw r6 r30 6
24-lw r4 r30 6
25-move r1 r4
26-outputreg r1
27-lw r4 r30 2
28-lw r5 r30 6
29-sub r6 r4 r5
30-sw r6 r30 7
31-lw r4 r30 7
32-move r1 r4
33-outputreg r1
34-lw r4 r30 3
35-sub r59 r4 r0
36-beq r59 r0 equal_0
37-li r5 0
38-j end_0
equal_0:
39-li r5 1
end_0:
40-beq r5 r0 L0
41-lw r4 r30 2
42-move r28 r4
43-j L1
L0:
44-lw r4 r30 3
45-move r1 r4
46-lw r4 r30 7
47-move r2 r4
48-jal gcd
49-move r4 r28
50-move r28 r4
L1:
51-lw r30 r31 1
52-subi r30 r30 6
53-jr r31
Func main:
54-addi r30 r30 3
55-sw r31 r30 1
56-input r28
57-move r4 r28
58-sw r4 r30 8
59-input r28
60-move r4 r28
61-sw r4 r30 9
62-lw r4 r30 8
63-move r1 r4
64-lw r4 r30 9
65-move r2 r4
66-jal gcd
67-move r4 r28
68-move r1 r4
69-outputreg r1
70-lw r30 r31 1
71-subi r30 r30 3
72-jr r31
