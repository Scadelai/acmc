0-j 94
bal array vet[10]
2-# Allocate variable i in scope minloc
3-# Allocate variable x in scope minloc
4-# Allocate variable k in scope minloc
5-add r8 r7 r9
6-add r8 r7 r9
7-add r8 r7 r9
CEHOLDER
Func minloc:
8-sw r31 r30 1
9-addi r30 r30 1
10-move r4 r5
11-move r6 r4
12-move r4 r5
13-move r7 r8
14-lw r10 r9 0
15-move r11 r10
16-move r4 r5
17-addi r7 r4 1
18-move r12 r7
19-L0:
20-move r4 r12
21-move r7 r13
22-slt r9 r4 r7
23-beq r9 r0 L1
24-move r4 r12
25-move r7 r8
26-lw r10 r9 0
27-move r4 r11
28-slt r7 r10 r4
29-beq r7 r0 L2
30-move r4 r12
31-move r7 r8
32-lw r10 r9 0
33-move r11 r10
34-move r4 r12
35-move r6 r4
36-L2:
37-move r4 r12
38-addi r7 r4 1
39-move r12 r7
40-j L0
41-L1:
42-move r4 r6
43-move r28 r4
44-lw r30 r31 1
45-jr r31
46-# Allocate variable i in scope sort
47-# Allocate variable k in scope sort
48-add r9 r7 r4
49-add r9 r7 r4
CEHOLDER
Func sort:
50-sw r31 r30 1
51-addi r30 r30 1
52-move r4 r5
53-move r6 r4
54-L0:
55-move r4 r6
56-move r7 r8
57-subi r9 r7 1
58-slt r7 r4 r9
59-beq r7 r0 L1
60-move r4 r10
61-move r1 r4
62-move r4 r6
63-move r2 r4
64-move r4 r8
65-move r3 r4
66-jal minloc
67-move r4 r28
68-move r11 r4
69-move r4 r11
70-move r7 r10
71-lw r12 r9 0
72-move r13 r12
73-move r4 r6
74-move r7 r10
75-lw r12 r9 0
76-move r4 r11
77-move r7 r10
78-add r9 r7 r4
79-sw r12 r9 0
80-move r4 r13
81-move r7 r6
82-move r9 r10
83-add r12 r9 r7
84-sw r4 r12 0
85-move r4 r6
86-addi r7 r4 1
87-move r6 r7
88-j L0
89-L1:
90-lw r30 r31 1
91-jr r31
92-# Allocate variable i in scope main
93-add r12 r9 r7
CEHOLDER
Func main:
94-sw r31 r30 1
95-addi r30 r30 1
96-move r5 r4
97-L0:
98-move r6 r5
99-slt r8 r6 r7
100-beq r8 r0 L1
101-input r28
102-move r6 r28
103-move r8 r5
104-move r9 r10
105-add r11 r9 r8
106-sw r6 r11 0
107-move r6 r5
108-addi r8 r6 1
109-move r5 r8
110-j L0
111-L1:
112-move r6 r10
113-move r1 r6
114-move r2 r4
115-move r3 r7
116-jal sort
117-move r6 r28
118-move r5 r4
119-L2:
120-move r8 r5
121-slt r9 r8 r7
122-beq r9 r0 L3
123-move r8 r5
124-move r9 r10
125-lw r12 r11 0
126-move r1 r12
127-outputreg r1
128-move r8 r5
129-addi r9 r8 1
130-move r5 r9
131-j L2
132-L3:
133-lw r30 r31 1
134-jr r31
