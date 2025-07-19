0-jump 73
n IR: GLOBAL_ARRAY vet, 10, __, __
CEHOLDER
Func minloc,:
2-sw r29 r31 1
3-addi r30 r30 1
4-addi r30 r30 1
5-addi r30 r30 1
6-# Parameter a, mapped to r1
7-# Parameter low, mapped to r2
8-# Parameter high, mapped to r3
9-sw r29 r1 5
10-sw r29 r4 6
11-add r7 r5 r6
12-sw r29 r8 7
13-# Label L0:
14-sub r3 r9 r10
15-beq r3 r0 3
16-blt r3 r0 3
17-sub r3 r13 r14
18-beq r3 r0 3
19-blt r3 r0 3
20-# Label L2:
21-add r17 r15 r16
22-sw r29 r18 8
23-jump L0,
24-# Label L1:
25-move r28 r19
26-lw r29 r31 1
27-jr r31 r0 r0
28-lw r29 r31 1
29-jr r31 r0 r0
CEHOLDER
Func sort,:
30-sw r29 r31 1
31-addi r30 r30 1
32-addi r30 r30 1
33-addi r30 r30 1
34-# Parameter a, mapped to r1
35-# Parameter low, mapped to r2
36-# Parameter high, mapped to r3
37-sw r29 r20 5
38-# Label L0:
39-sub r23 r21 r22
40-sub r3 r24 r25
41-beq r3 r0 3
42-blt r3 r0 3
43-# Argument R9,
44-# Argument R12,
45-# Argument R11,
46-sw r30 r26 0
47-addi r30 r30 1
48-sw r30 r2 0
49-addi r30 r30 1
50-sw r30 r3 0
51-addi r30 r30 1
52-subi r30 r30 1
53-lw r30 r3 0
54-subi r30 r30 1
55-lw r30 r2 0
56-subi r30 r30 1
57-lw r30 r1 0
58-sw r30 r29 0
59-move r29 r30
60-addi r30 r30 1
61-jal 1
62-move r30 r29
63-lw r29 r29 0
64-# Unknown IR: STORE_RET __, __, R14, __
65-sw r29 r27 6
66-sw r29 r3 7
67-add r12 r10 r11
68-sw r29 r13 8
69-jump L0,
70-# Label L1:
71-lw r29 r31 1
72-jr r31 r0 r0
CEHOLDER
Func main,:
73-sw r29 r31 1
74-addi r30 r30 1
75-addi r30 r30 1
76-addi r30 r30 1
77-sw r29 r14 2
78-# Label L0:
79-sub r3 r15 r16
80-beq r3 r0 3
81-blt r3 r0 3
82-sw r30 r17 0
83-addi r30 r30 1
84-sw r30 r2 0
85-addi r30 r30 1
86-sw r30 r3 0
87-addi r30 r30 1
88-subi r30 r30 1
89-lw r30 r3 0
90-subi r30 r30 1
91-lw r30 r2 0
92-subi r30 r30 1
93-lw r30 r1 0
94-sw r30 r29 0
95-move r29 r30
96-addi r30 r30 1
97-jal 1
98-move r30 r29
99-lw r29 r29 0
100-# Unknown IR: STORE_RET __, __, R25, __
101-add r22 r20 r21
102-sw r29 r23 3
103-jump L0,
104-# Label L1:
105-# Argument vet,
106-# Argument 0,
107-# Argument 10,
108-sw r30 r24 0
109-addi r30 r30 1
110-sw r30 r2 0
111-addi r30 r30 1
112-sw r30 r3 0
113-addi r30 r30 1
114-subi r30 r30 1
115-lw r30 r3 0
116-subi r30 r30 1
117-lw r30 r2 0
118-subi r30 r30 1
119-lw r30 r1 0
120-sw r30 r29 0
121-move r29 r30
122-addi r30 r30 1
123-jal 1
124-move r30 r29
125-lw r29 r29 0
126-sw r29 r25 4
127-# Label L2:
128-sub r3 r26 r27
129-beq r3 r0 3
130-blt r3 r0 3
131-# Argument R29,
132-sw r30 r3 0
133-addi r30 r30 1
134-sw r30 r2 0
135-addi r30 r30 1
136-sw r30 r3 0
137-addi r30 r30 1
138-subi r30 r30 1
139-lw r30 r3 0
140-subi r30 r30 1
141-lw r30 r2 0
142-subi r30 r30 1
143-lw r30 r1 0
144-sw r30 r29 0
145-move r29 r30
146-addi r30 r30 1
147-jal 1
148-move r30 r29
149-lw r29 r29 0
150-add r6 r4 r5
151-sw r29 r7 5
152-jump L2,
153-# Label L3:
154-lw r29 r31 1
155-jr r31 r0 r0
