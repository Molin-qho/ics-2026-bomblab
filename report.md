# Lab2：BombLab 实验报告

## 1. 实验目的

本实验要求在没有正式关卡源码的情况下，使用 `objdump` 反汇编和 GDB 动态调试个人 `bomb`，分析函数调用、参数传递、分支、循环、递归、结构体/指针以及函数指针等机制，并在 `defuser.c` 中实现从 `challenge` 计算口令的函数。

本次实验完成了 Phase 1–6 的解法，并实现了 Honor 的 `solve_secret`。解法不是针对某个固定 seed 写死口令，而是根据输入的 `challenge`（以及 Honor 使用的 `context`）计算结果。

## 2. 实验环境与工具

- 系统：Windows 11 + WSL Ubuntu，Linux x86_64
- 编译器：GCC，使用模板 Makefile 编译（C11、`-Wall -Wextra -Wpedantic -Werror`）
- 构建工具：GNU Make
- 反汇编工具：`objdump`（binutils）
- 调试工具：GDB 17.1
- 主要命令：`objdump -d`、`nm -n`、GDB 的 `break`、`run`、`continue`、`x`、`p/u` 等

主要分析流程为：导出目标函数汇编，识别参数寄存器与关键比较/跳转指令；对被调用的辅助函数继续反汇编；必要时用 GDB 检查跳转表、全局数据和运行时寄存器/内存；将计算关系转换为 C 代码，并用不同 seed 验证。

## 3. 各关卡分析

以下算术表达式均按汇编的 32 位寄存器语义计算；写入 `eax` 或其他 32 位寄存器的结果保留低 32 位，即按模 \(2^{32}\) 回绕。`C` 表示当前关卡的 64 位 `challenge`。

### 3.1 Phase 1：函数调用、参数传递与返回值

`phase_01` 通过 `valid_u32_fields` 和 `sscanf` 确认输入恰好包含一个无符号 32 位整数。随后将 `challenge` 作为参数传给 `p1_compute`，并将其返回值与用户输入比较。

`p1_compute` 将 challenge 拆成三个参与计算的 32 位值：

- \(H=(C>>32)\) 的低 32 位；
- \(M=(C>>16)\) 的低 32 位；
- \(L=C\) 的低 32 位。

三个辅助函数的汇编含义：

- `p1_step1`：`sub %esi,%edi` 后通过 `lea` 计算 `5*(H-M)+0x6c41`；
- `p1_step0`：通过 `lea`、`add`、`xor` 计算 `(5*L+H)^0x3c26`；
- `p1_step2`：先将第一个中间结果与 `0xa906` 异或，再加上第二个中间结果的两倍。

因此定义：

\[
A=5(H-M)+0x6c41
\]
\[
B=(5L+H)\oplus0x3c26
\]
\[
\boxed{v_0=(A\oplus0xa906)+2B}
\]

解法输出一个整数 `v0`。

### 3.2 Phase 2：数组、循环与递推

`phase_02` 要求输入六个整数。第一个整数由 challenge 右移 17 位、乘 2、取低 8 位后再加 `0x1d` 得到：

\[
v_0=\left(2(C>>17)\ \&\ 0xff\right)+0x1d
\]

之后循环变量 \(i\) 从 1 到 5。每轮从 challenge 中提取连续的 5 位：

\[
t_i=(C>>(10+5(i-1)))\ \&\ 0x1f
\]

再结合循环下标和前一个数组元素计算：

\[
\boxed{v_i=5\left((t_i+i)\oplus v_{i-1}\right)},\quad i=1,\ldots,5
\]

汇编中的 `shr %cl` 表示按 `cl` 指定的位数右移，`and $0x1f` 提取低 5 位，`xor %esi,%eax` 将当前计算值与前一个输入值异或，`lea (%rax,%rax,4),%eax` 实现乘 5。该关体现了用循环实现递推关系。

### 3.3 Phase 3：switch 分支与跳转表

`phase_03` 要求输入两个整数。第一个整数为 challenge 的低 3 位：

\[
x=C\ \&\ 7
\]

传给 `p3_dispatch` 的另一个 32 位参数为：

\[
y=((C>>32)\ \&\ 0xffffffff)\oplus((C>>3)\ \&\ 0xffffffff)
\]

通过 GDB 读取地址 `0x405820` 的 8 项跳转表，并结合 `p3_dispatch` 中的跳转位置，得到分支与函数的对应关系。注意跳转表索引并不等于辅助函数名称中的编号：

| `x` / case | 实际调用函数 | 返回值 |
|---:|---|---|
| 0 | `p3_case_2` | `(y ^ 0x7259) + (y >> 1)` |
| 1 | `p3_case_5` | `5*y + 0x2d866` |
| 2 | `p3_case_0` | `(2*y) ^ 0x1da2` |
| 3 | `p3_case_3` | `3*y + 0xb1d8` |
| 4 | `p3_case_1` | `5*y + 0x35476` |
| 5 | `p3_case_6` | `(y ^ 0x50ee) + (y >> 1)` |
| 6 | `p3_case_7` | `5*y + 0x6062` |
| 7 | `p3_case_4` | `(4*y) ^ 0x1956` |

最终口令为两个整数：`x` 和对应分支的返回值。该关通过读取跳转表，将间接跳转还原成 C 语言 `switch` 结构。

### 3.4 Phase 4：递归调用与返回过程

`phase_04` 要求输入一个整数，并调用：

\[
\texttt{p4\_walk}(target,0,127,context_4)
\]

其中：

\[
target=C\ \&\ 0x7f
\]
\[
context_4=((C>>7)\ \&\ 0xffffffff)\oplus((C>>32)\ \&\ 0xffffffff)
\]

`p4_walk` 根据 `low`、`high` 计算中点：

\[
mid=low+((high-low)>>1)
\]

若 `target == mid`，递归终止并返回：

\[
(mid+30)\oplus context_4
\]

若 `target < mid`，递归搜索左区间 `[low, mid-1]`；若 `target > mid`，递归搜索右区间 `[mid+1, high]`。递归返回时，每一层还会继续加工子调用结果：

- 左分支：`(child * 5) ^ (mid + context_4) ^ 0x15`；
- 右分支：`(child * 5) ^ (mid + context_4) ^ 1`。

由此在 C 中实现了与汇编相同的二分递归以及逐层返回计算。

### 3.5 Phase 5：结构体布局、指针与链表遍历

`phase_05` 要求输入两个整数，并调用 `p5_traverse(challenge, &last_b)`。函数返回值是第一个口令整数，通过指针写出的最后访问节点 `b` 值是第二个整数。

根据访问偏移 `0(%rdx)`、`4(%rdx)`、`8(%rdx)`，结合 x86-64 指针宽度，节点布局为：

```c
struct Node {
    uint32_t a;       // offset 0
    uint32_t b;       // offset 4
    struct Node *next; // offset 8
};
```

每个节点占 16 字节。使用 GDB 读取 `p5_heads`（地址 `0x405b60`）和节点区域（地址 `0x405ba0`），得到的数据如下。下表节点编号仅用于在解法中用数组模拟指针链表：

| 编号 | 地址 | `a` | `b` | `next` 编号/地址 |
|---:|---|---:|---:|---|
| 0 | `0x405ba0` | 293 | 561 | 5 |
| 1 | `0x405bb0` | 1318 | 2416 | 9 |
| 2 | `0x405bc0` | 292 | 866 | `0x405c50`（本次读取范围之外） |
| 3 | `0x405bd0` | 7217 | 2144 | 2 |
| 4 | `0x405be0` | 59736 | 2896 | 1 |
| 5 | `0x405bf0` | 21634 | 2649 | 6 |
| 6 | `0x405c00` | 8646 | 96 | 7 |
| 7 | `0x405c10` | 26855 | 295 | 8 |
| 8 | `0x405c20` | 10547 | 1866 | 3 |
| 9 | `0x405c30` | 12739 | 1315 | 0 |
| 10 | `0x405c40` | 6029 | 1085 | 4 |

8 个起始节点索引为：

\[
heads=[10,4,1,9,0,5,6,7]
\]

链表起点索引和遍历次数分别为：

\[
index=C\ \&\ 7,\qquad steps=(C>>3)\ \&\ 3
\]

初始混合值：

\[
T_5=((C>>6)\ \&\ 0xffffffff)\oplus((C>>32)\ \&\ 0xffffffff)\oplus0x19f
\]

处理起始节点后：

\[
result=(3T_5+a[node])\oplus b[node]
\]

之后沿 `next` 指针额外前进 `steps` 次，每次执行：

\[
result=(3\cdot result+a[node])\oplus b[node]
\]

每次访问节点时更新 `last_b=b[node]`。最终输出 `result` 和 `last_b` 两个整数。由于最多额外遍历 3 次，解法只需保存实际可能访问到的节点数据，并用数组下标模拟链表。

### 3.6 Phase 6：函数指针表与间接调用

`phase_06` 要求输入一个整数，调用 `p6_dispatch(challenge)`。通过 GDB 读取 `0x405e00` 的函数指针表，并通过 `nm -n` 确认函数地址，得到索引对应关系：

| 索引 | 函数 |
|---:|---|
| 0、1、2 | `p6_op_2` |
| 3 | `p6_op_3` |
| 4 | `p6_op_1` |
| 5 | `p6_op_0` |
| 6 | `p6_op_4` |
| 7 | `p6_op_5` |

定义 32 位函数 `f_i(x)`，各项为：

- `f_0`、`f_1`、`f_2`：`(x + 0x8d5a) ^ (x >> 7)`；
- `f_3`：32 位循环右移 5 位，即 `ROR32(x,5)`；
- `f_4`：32 位循环左移 3 位后异或 `0x11c1`，即 `ROL32(x,3) ^ 0x11c1`；
- `f_5`：`((x << 5) + 0x5f40) ^ (3*x)`；
- `f_6`：`((x >> 1) ^ x) + 0x6b3c`；
- `f_7`：`(x ^ 0x3be5) + (x << 7)`。

调度函数先计算：

\[
y=((C>>6)\ \&\ 0xffffffff)\oplus((C>>32)\ \&\ 0xffffffff)
\]

\[
i=C\ \&\ 7,\qquad j=(C>>3)\ \&\ 7
\]

先调用 `A=f_i(y)`，再将 `A ^ 0xfe1d` 作为第二次调用的参数，最后加上 `0x30a6`：

\[
\boxed{answer=f_j(A\oplus0xfe1d)+0x30a6}
\]

在解法中用 `switch` 模拟函数指针表，用 32 位旋转函数实现 `rol` 和 `ror`。

## 4. Honor 分析

Honor 由 `secret_gate`、`secret_copy` 和 `secret_phase` 共同完成。`secret_copy` 将输入字符串复制到局部缓冲区。`secret_gate` 检查字符串长度，并检查缓冲区偏移 32 处的 4 字节标记。常量 `0x4e45504f` 在小端序下对应 ASCII `OPEN`。随后 `secret_phase` 从缓冲区偏移 36 处开始解析三个无符号整数，格式字符串经 GDB 确认为 `"%u %u %u"`。

因此 Honor 文本布局为：32 个填充字符、`OPEN`、空格和三个整数。例如：

```text
AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAOPEN <v0> <v1> <v2>
```

设 `C` 为传入 `solve_secret` 的 challenge，`K` 为 context。结合 `secret_phase` 的寄存器传递和实际调试中第一个整数的比较结果，定义：

\[
S=((C>>32)\ \&\ 0xffffffff)\oplus(C\ \&\ 0xffffffff)
\]
\[
T=((K>>32)\ \&\ 0xffffffff)\oplus(K\ \&\ 0xffffffff)
\]
\[
X=S\oplus T,\qquad v_0=K\ \&\ 3
\]

`secret_mix` 的汇编为：先将输入复制到 `eax`，再分别执行右移、异或常量、异或 `0x2aac` 和加法。因此：

\[
secret\_mix(X)=((X>>1)\oplus0x6c9d)+(X\oplus0x2aac)
\]

第二、第三个整数为：

\[
v_1=secret\_mix(X)\oplus\left(((K>>16)\ \&\ 0xffffffff)\oplus(v_0\cdot0x2aac)\right)
\]
\[
v_2=v_1\oplus\left((S>>1)+T+v_0+0x6c9d\right)
\]

最终由 `snprintf` 输出 32 个填充字符、`OPEN` 和三个十进制整数。所有中间运算遵循 32 位无符号回绕。

## 5. 测试与验证记录

已在本地对 Phase 1–6 分别进行单关验证，均得到对应的 `Phase N defused.`。此外，使用 seed 42 生成七行输入文件并通过文件重定向运行完整程序；修正 Honor 的参数对应关系后，完成了本地 Honor 调试。此前排查过程中还发现，直接从 VSCode 集成终端重定向命令输出时，文件可能混入终端 Shell Integration 控制序列；改用 Python `subprocess` 捕获标准输出并写入文件后，输入文件不再包含 ESC 控制字符。

| 测试项目 | 当前记录 |
|---|---|
| Phase 1–6 单关测试 | 全部通过 |
| Phase 1–6 完整 seed 42 测试 | 全部通过 |
| Honor seed 42 | 全部通过 |
| 各关额外 seed（非公开 seed） | 全部通过 |
| `make test` / 公开测试统计 | 全部通过 |


## 6. 实验中遇到的问题与处理

1. **汇编阅读**：通过 `objdump --disassemble=<函数名>` 逐个导出入口函数和辅助函数，结合 x86-64 System V 调用约定追踪 `rdi`、`rsi`、`rdx`、`rcx` 等参数寄存器及 `eax` 返回值。
2. **跳转表**：Phase 3 和 Phase 6 通过 GDB 读取内存中的表项，再结合 `nm -n` 和辅助函数反汇编确认索引与目标函数的对应关系。
3. **递归**：Phase 4 分别识别中点计算、递归区间更新、递归终止条件和返回时的逐层运算。
4. **链表数据**：Phase 5 根据结构体成员偏移读取节点数据，并用数组模拟节点与 `next` 关系。
5. **Honor 调试**：根据栈偏移重新确认 `OPEN` 位于前 32 字节之后；使用断点检查入口标记和三个整数的比较过程，确认 `challenge` 与 `context` 的寄存器对应关系。
6. **输入文件污染**：发现集成终端重定向可能混入 ESC 控制序列，改用 Python `subprocess` 的 `stdout=PIPE` 取得程序纯输出并写入输入文件。
7. **C 函数组织**：辅助函数必须定义在文件作用域，不能嵌套定义在 `solve_phase_4` 内；编译器启用 `-Werror` 时，未使用的静态函数也会导致编译失败。

## 7. 辅助工具与参考资料

- 课程提供的 BombLab 实验文档及仓库中的 tutor 示例。
- GNU `objdump` / binutils：用于导出和阅读反汇编。
- GDB：用于设置断点、查看寄存器、内存和跳转表。
- **OpenAI ChatGPT（AI 辅助工具）**：用于解释汇编指令和 x86-64 调用约定、协助梳理寄存器数据流、将已观察到的汇编转换为数学表达式、讨论 C 语言实现和排查编译/调试问题。所有关键关系均结合个人 bomb 的反汇编、GDB 运行结果和本地测试进行验证；AI 输出没有直接替代本地验证。


## 8. 运行结果截图

### Phase 1–6

分别运行各正式关卡，六关均通过。
![Phase1运行结果](1d476f9434bdcf3d24c46c174146c629.png)
![Phase2运行结果](af579f4a96195d13815cd0c59a5709ba.png)
![Phase3运行结果](image.png)
![Phase4运行结果](image-1.png)
![Phase5运行结果](image-2.png)
![Phase6运行结果](image-3.png)

### Honor

Honor 验证通过。
![Honour运行结果](image-4.png)