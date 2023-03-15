## tutorial 03

字符串解析: 
- 结构: 字符串由双引号`"..."`包围, 解析时一次读取将引号内内容(含转义字符处理), 并压入栈中, 读取完所有字符串再全部取出来;
- 执行: 解析到左引号, 则说明一个字符串开始, 依次解析. 当解析到右引号, 则字符串解析完成; 当解析到'\0', 则说明字符串结束, 还没有解析到右引号, 则存在语法错误

解析栈设计: 包含栈地址和顶部长度指针, 通过地址+长度获得栈顶元素, 栈内存储解析到的字符内容

Linux/OSX 下的内存泄漏检测方法:

在 Linux、OS X 下，我们可以使用 [valgrind](https://valgrind.org/) 工具（用 `apt-get install valgrind`、 `brew install valgrind`）。我们完全不用修改代码，只要在命令行执行：

~~~
$ valgrind --leak-check=full  ./leptjson_test
==22078== Memcheck, a memory error detector
==22078== Copyright (C) 2002-2015, and GNU GPL'd, by Julian Seward et al.
==22078== Using Valgrind-3.11.0 and LibVEX; rerun with -h for copyright info
==22078== Command: ./leptjson_test
==22078== 
--22078-- run: /usr/bin/dsymutil "./leptjson_test"
160/160 (100.00%) passed
==22078== 
==22078== HEAP SUMMARY:
==22078==     in use at exit: 27,728 bytes in 209 blocks
==22078==   total heap usage: 301 allocs, 92 frees, 34,966 bytes allocated
==22078== 
==22078== 2 bytes in 1 blocks are definitely lost in loss record 1 of 79
==22078==    at 0x100012EBB: malloc (in /usr/local/Cellar/valgrind/3.11.0/lib/valgrind/vgpreload_memcheck-amd64-darwin.so)
==22078==    by 0x100008F36: lept_set_string (leptjson.c:208)
==22078==    by 0x100008415: test_access_boolean (test.c:187)
==22078==    by 0x100001849: test_parse (test.c:229)
==22078==    by 0x1000017A3: main (test.c:235)
==22078== 
...
~~~

它发现了在 `test_access_boolean()` 中，由 `lept_set_string()` 分配的 2 个字节（`"a"`）泄漏了。

Valgrind 还有很多功能，例如可以发现未初始化变量。我们若在应用程序或测试程序中，忘了调用 `lept_init(&v)`，那么 `v.type` 的值没被初始化，其值是不确定的（indeterministic），一些函数如果读取那个值就会出现问题：

~~~c
static void test_access_boolean() {
    lept_value v;
    /* lept_init(&v); */
    lept_set_string(&v, "a", 1);
    ...
}
~~~

这种错误有时候测试时能正确运行（刚好 `v.type` 被设为 `0`），使我们误以为程序正确，而在发布后一些机器上却可能崩溃。这种误以为正确的假像是很危险的，我们可利用 valgrind 能自动测出来：

~~~
$ valgrind --leak-check=full  ./leptjson_test
...
==22174== Conditional jump or move depends on uninitialised value(s)
==22174==    at 0x100008B5D: lept_free (leptjson.c:164)
==22174==    by 0x100008F26: lept_set_string (leptjson.c:207)
==22174==    by 0x1000083FE: test_access_boolean (test.c:187)
==22174==    by 0x100001839: test_parse (test.c:229)
==22174==    by 0x100001793: main (test.c:235)
==22174== 
~~~

它发现 `lept_free()` 中依靠了一个未初始化的值来跳转，就是 `v.type`，而错误是沿自 `test_access_boolean()`。

编写单元测试时，应考虑哪些执行次序会有机会出错，例如内存相关的错误。然后我们可以利用 TDD 的步骤，先令测试失败（以内存工具检测），修正代码，再确认测试是否成功。

## tutorial 04

