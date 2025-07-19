0-j 65
bal array vet[10]
CEHOLDER
Func minloc:
2-sw r31 r30 1
3-addi r30 r30 1
4-# Parameter a in r1
5-# Parameter low in r2
6-# Parameter high in r3
7-move r4 r2
8-add r57 r0 r2
9-lw r10 r57 0
10-move r5 r10
11-addi r12 r2 1
12-move r6 r12
13-# Label L0:
14-sub r59 r6 r3
15-bgte r59 r0 L1
16-add r57 r0 r6
17-lw r14 r57 0
18-sub r59 r14 r5
19-bgte r59 r0 L2
20-add r57 r0 r6
21-lw r15 r57 0
22-move r5 r15
23-move r4 r6
24-# Label L2:
25-addi r18 r6 1
26-move r6 r18
27-j L0
28-# Label L1:
29-move r28 r4
30-lw r30 r31 1
31-jr r31
32-lw r30 r31 1
33-jr r31
CEHOLDER
Func sort:
34-sw r31 r30 1
35-addi r30 r30 1
36-# Parameter a in r1
37-# Parameter low in r2
38-# Parameter high in r3
39-move r4 r2
40-# Label L0:
41-subi r24 r3 1
42-sub r59 r4 r24
43-bgte r59 r0 L1
44-move r28 r1
45-move r28 r4
46-move r28 r3
47-jal minloc
48-move r25 r28
49-move r5 r25
50-add r57 r0 r5
51-lw r27 r57 0
52-move r6 r27
53-add r57 r0 r4
54-lw r9 r57 0
55-add r57 r0 r5
56-sw r9 r57 0
57-add r57 r0 r4
58-sw r6 r57 0
59-addi r10 r4 1
60-move r4 r10
61-j L0
62-# Label L1:
63-lw r30 r31 1
64-jr r31
CEHOLDER
Func main:
65-sw r31 r30 1
66-addi r30 r30 1
67-li r4 0
68-# Label L0:
69-li r58 10
70-sub r59 r4 r58
71-bgte r59 r0 L1
72-input r28
73-move r16 r28
74-add r57 r0 r4
75-sw r16 r57 0
76-addi r17 r4 1
77-move r4 r17
78-j L0
79-# Label L1:
80-move r28 r5
81-move r28 r6
82-move r28 r7
83-jal sort
84-li r4 0
85-# Label L2:
86-li r58 10
87-sub r59 r4 r58
88-bgte r59 r0 L3
89-add r57 r0 r4
90-lw r21 r57 0
91-move r28 r21
92-outputreg r28
93-addi r22 r4 1
94-move r4 r22
95-j L2
96-# Label L3:
97-lw r30 r31 1
98-jr r31
