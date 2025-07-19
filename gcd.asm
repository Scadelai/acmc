0-jump 44
unc gcd,:
1-sw r29 r31 1
2-addi r30 r30 1
3-addi r30 r30 1
4-addi r30 r30 1
5-# Parameter u, mapped to r1
6-# Parameter v, mapped to r2
7-sub r3 r1 r2
8-beq r3 r0 3
9-blt r3 r0 3
10-jump L1,
11-# Label L0:
12-div r3 r4
13-mflo r3
14-mult r5 r6
15-mflo r7
16-sub r10 r8 r9
17-# Argument v,
18-# Argument R3,
19-sw r30 r11 0
20-addi r30 r30 1
21-sw r30 r2 0
22-addi r30 r30 1
23-sw r30 r3 0
24-addi r30 r30 1
25-subi r30 r30 1
26-lw r30 r3 0
27-subi r30 r30 1
28-lw r30 r2 0
29-subi r30 r30 1
30-lw r30 r1 0
31-sw r30 r29 0
32-move r29 r30
33-addi r30 r30 1
34-jal 1
35-move r30 r29
36-lw r29 r29 0
37-# Unknown IR: STORE_RET __, __, R4, __
38-move r28 r12
39-lw r29 r31 1
40-jr r31 r0 r0
41-# Label L1:
42-lw r29 r31 1
43-jr r31 r0 r0
CEHOLDER
Func main,:
44-sw r29 r31 1
45-addi r30 r30 1
46-addi r30 r30 1
47-addi r30 r30 1
48-sw r30 r13 0
49-addi r30 r30 1
50-sw r30 r2 0
51-addi r30 r30 1
52-sw r30 r3 0
53-addi r30 r30 1
54-subi r30 r30 1
55-lw r30 r3 0
56-subi r30 r30 1
57-lw r30 r2 0
58-subi r30 r30 1
59-lw r30 r1 0
60-sw r30 r29 0
61-move r29 r30
62-addi r30 r30 1
63-jal 1
64-move r30 r29
65-lw r29 r29 0
66-# Unknown IR: STORE_RET __, __, R7, __
67-sw r29 r14 2
68-sw r30 r15 0
69-addi r30 r30 1
70-sw r30 r2 0
71-addi r30 r30 1
72-sw r30 r3 0
73-addi r30 r30 1
74-subi r30 r30 1
75-lw r30 r3 0
76-subi r30 r30 1
77-lw r30 r2 0
78-subi r30 r30 1
79-lw r30 r1 0
80-sw r30 r29 0
81-move r29 r30
82-addi r30 r30 1
83-jal 1
84-move r30 r29
85-lw r29 r29 0
86-# Unknown IR: STORE_RET __, __, R9, __
87-sw r29 r16 3
88-# Argument R8,
89-# Argument R10,
90-sw r30 r17 0
91-addi r30 r30 1
92-sw r30 r2 0
93-addi r30 r30 1
94-sw r30 r3 0
95-addi r30 r30 1
96-subi r30 r30 1
97-lw r30 r3 0
98-subi r30 r30 1
99-lw r30 r2 0
100-subi r30 r30 1
101-lw r30 r1 0
102-sw r30 r29 0
103-move r29 r30
104-addi r30 r30 1
105-jal 1
106-move r30 r29
107-lw r29 r29 0
108-# Unknown IR: STORE_RET __, __, R11, __
109-# Argument R11,
110-sw r30 r18 0
111-addi r30 r30 1
112-sw r30 r2 0
113-addi r30 r30 1
114-sw r30 r3 0
115-addi r30 r30 1
116-subi r30 r30 1
117-lw r30 r3 0
118-subi r30 r30 1
119-lw r30 r2 0
120-subi r30 r30 1
121-lw r30 r1 0
122-sw r30 r29 0
123-move r29 r30
124-addi r30 r30 1
125-jal 1
126-move r30 r29
127-lw r29 r29 0
128-lw r29 r31 1
129-jr r31 r0 r0
