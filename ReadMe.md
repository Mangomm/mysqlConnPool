本连接池是参考其它github项目的，然后自己重新写了一遍，并且优化了部分代码，自己添加了一些注释和需要注意的地方。

1. 平台：
只支持Linux，并且要求支持C++11，不过可以自己改一下代码就能在Windows或者其它平台使用，因为我只使用了Linux特有的posix标准的锁。

2. 使用前提：
Linux系统需要下载mysql。
若下载了mysql，但找不到mysql.h，需要安装依赖：
sudo apt-get install libmysqlclient-dev




V0.0.0版本：
使用：
1）首先在数据库创建test数据库。
2）cd sql/  //去到本项目的sql目录
3）./import.sh
4）make
5）./test

优化相关
MySQLConnPool目前需要优化的地方：
    1. 返回值和一些类型转换相关的需要进行部分优化，例如例如数据库返回的插入id是uint_64，若用32位去接就会数据丢失。

V1.0.0版本：
    1）优化插入的返回值，将uint32改成uint64。
    2）添加myemployes.sql、girls.sql文件，执行后会生成对应的数据库及其tables。
    3）新增test1.cpp，用于测试分组查询、92连接查询以及99版本的连接查询，但保留test.cpp，是否测试test1.cpp的内容只需改变who变量即可，默认true测试test1.cpp。
    4）注意：查询方面，目前已经实现了支持分组查询，92的连接查询(只需要支持内连即可)，以及支持99的内连外连交叉连。

V2.0.0版本：
    1)增加支持子查询与联合查询。都是同一接口execSelectSubQuery，这个接口同样应该支持其他前面的分组、连接查询，可以认为是万能的接口。
    注意：目前调用execSelectSubQuery时，发现一个bug，貌似当某个字段存在null的时候，该字段的值全为0，数据库有值的情况下也是0，但是其他字段不受影响(不存在null)，值是正常的。
    这个情况可以去test1.cpp的SelectFunc5去测试，观察commission_pct字段，情况是必现的，明天或者过两天再看看。

V2.0.1版本：
    1）修复V2.0.0的bug，即修复当数据库字段是浮点型，并且值类型存在小数点的而读取失败的问题，因为使用strtoul、strtoull转化字符串成浮点型的话，这两函数遇到0-9以外的非法值会立马
    返回，例如这里是小数点"."，所以读取的时候出现commission_pct都是0的情况，因为数据库的数据是0.3，那么遇到"."就会返回，所以只转化了0。若是1.3，那么返回1。

    注，下面的合并数据库数据的函数(fullSelectDataByRow)： 
    1）当类型是CHAR+UCHAR或者是数值型(short~double)时，若值为0，可能代表数据库的值是null或者是真的0。
    2）strtoul与strtoull是一样的，都是将传入的字符串参1转成对应的进制，只不过一个是unsigned long返回，一个是unsigned long long返回，
    所以short、unsigned short、int、unsigned int这些使用都没问题，不存在访问越界的问题，因为都是通过传入的参1字符串进行处理，只是强转的问题而已，但是至少需要保证
    返回值大于要保存的值，例如要转的字符串大于long，但是你的返回值类型仍用long，那么就会返回时出现数据丢失，所以此时对一个已经数据丢失的数据(即返回值)进行强转还是数据丢失。
    3）float、double存在小数的不能使用整数的方法转，否则遇到小数点即非法字符后，函数返回，例如1.3，则返回1，0.3则返回0，即修复V2.0.0版本的问题。

V2.1.0版本：
    1）增加了支持Update的通用方法(重载)，只需要直接传入sql语句即可。
    2）修改makefile，调试对应的testxxx.cpp，只需要改变who的数值即可。
    3）添加test2.cpp,里面测试了通用的Update。

V2.1.1版本：
    1）该版本只是测试是否需要在~DBPool进行回收处理。由于使用shared_ptr，程序结束时自动调用fini()去回收每一个连接，故无需处理任何东西。
    2）具体测试看test2.cpp，程序首先执行生成一个连接，执行完毕后，待10s程序会退出，若打印"test auto recycling"，说明自动回收成功。

V2.2.1版本：
    1)修复不同编译器编译变长数组时，有可能报错的问题。例如纯makefile编译时不报错，但是使用cmake编译时会报错。

V2.3.1版本：
    1）本版本修改较多，将多余的接口删掉，并各自提供了select、insert、update、delete的统通用接口。
    2）test.cpp主要是测新增的通用接口。test1.cpp主要是测select的通用接口，测试该通用接口是能够正常执行分组、连接、子查询等语句。test2.cpp在该版本的作用实际在test.cpp测过了。
    3）makefile有时容易报错，大概是tab被空格代替了，适当的去注释一下即可，很简单，这里不做解释。
    4）所有的testxxx.cpp文件都已经测试过了，忽略下面的问题，接口都是正常的，可以放心的用到商业代码中去。

该版本存在一个问题：
    1）就是在测试test.cpp中的delete的多表的连接删除时，虽然执行成功，但是影响行数返回0(mysql_affected_rows)，数据库的数据并未被改变；但是直接在navicat执行又没有问题。
        百度了一下，大概可能是事务的一下配置没配好。 
    审视该问题：个人觉得目前没必要深入该问题，因为多表的连接删除在我这边的需求基本遇不到，所以该问题暂时放下，以后有该需求时再深入探讨。

后续版本要改进的问题：
    1）添加一种处理，连接池被创建时，池里有一定的连接，并且当连接池大于规定的空闲连接时，自动回收，最后提供当程序结束时，有接口能将连接池剩余的连接回收掉。
        这种思想也是线程池的思想，即动态开辟与关闭。
    2）优化V2.3.1版本的多表连接删除的问题。

V2.3.2版本：
    1）添加动态增加、减少连接的功能。
    2）优化了makefile，编译不同测试文件，只需要将makefile中的变量who改成对应的数值即可。

后续版本要改进的问题：
    1）将map中存储连接的key换成时间戳去处理，因为动态处理时，当长时间去增加连接，有可能会到达int的max值，这是一种隐患。所以我们换成系统时间戳去递增是最好的处理。
        当然，其实我们基本很难到达int的max值。
    2）优化V2.3.1版本的多表连接删除的问题(这个应该是mysql配置文件的配置问题)。

V2.3.3版本：
    1）在MyTime.h增加了获取时间戳的相关接口。
    2）将map中存储连接的key换成时间戳去处理。已测试通过，目前并未发现太大问题。

    备注：调试时应尽量在DBPool类的成员函数打印，而不是在DBConn连接类本身打印，否则看起来很难理解。

后续版本要改进的问题：
    1）优化V2.3.1版本的多表连接删除的问题(这个应该是mysql配置文件的配置问题, see test.cpp)。
    2）可以优化类结构，例如函数名、隐藏外界不需要调用的成员函数等等。

    实际上这两个小问题可以忽略掉，本连接池基本已经很完善了，想要拓展其他功能可以自己额外添加，例如添加超时提供给外界配置、统计信息等等。

V2.3.4版本：
    1）添加连接数据库是禁用ssl的选项(SSL_MODE_DISABLED)，否则有的mysql启用ssl(show variables like "%ssl%")，将会连接失败

V2.3.5版本：
    1）将本连接池兼容Windows版本.主要是处理连接池的调整线程，在win下，使用C++11的thread去代替。锁同样使用C++11的锁代替，所以后续可以全部将相关代码换成C++11的内容去做也行.
    
    兼容windows遇到的问题：
    1）注释问题。我直接将对应的代码拷贝到VS编译，会出现很多问题，明明语法是没错的，例如报错可能看到"在注释中遇到意外的文件结束"。
        原因是：utf8格式出错，有一个注释是/* 中文*/，这里由于编码问题，中文和英文联合起来，吞掉了注释的*/，导致bug。只需要改为/* 中文 */。
        所以为了不出错，中文注释可能应该前后加英文字符，如前面加空格，后面加"."号。使用/**/注释的最好做法是/* 中文xxx. */。

    2）上面解决完编译成功后，发现在VS使用printf打印是乱码来的。原因是：
        我本项目的文件是在vscode开发，服务器是linux，所以git下来拷贝到VS项目时，是utf8的编码，而VS的编码默认是gbk，所以我们需要将utf8编码转成gbk后，
        再拷贝到VS项目中。
        具体做法：先git clone代码下来-->然后使用记事本打开每个文件，在记事本左上角选择文件-另存为，将其保存为ANSI(对应gbk编码)，然后放到VS项目即可。
        这里我也提供一份转换后的代码，放在gbk目录下面。注，本程序的源代码默认都是使用utf8，在win开发的需要注意这一点。
        实际上上面两点都是因为编码造成的，因为在linux(utf8)是正常的，而去到VStudio的环境(gbk)就不行。

    3）在win开发时，由于cmd窗口也是gbk编码，而我们从mysql select出来的内容一般都是utf8，所以当我们打印中文时，一般会看到乱码。
        解决：不难，我们在调用连接池对应的方法时，传编码方式为"set names gbk"即可。
    注意，上面三个问题一般只有出现中文才会出现这种错误。

    所以总结上面三个问题，两句话：
    1）Linux开发源代码编码格式设为utf8好点，Win则设为gbk好点。
    2）Linux下执行mysql的编码默认是utf8即可，Win则需要设为gbk。

    搭建Win下的项目很简单：
    将源代码编码转为gbk后，将DBDefine.h、DBPool.cpp、DBPool.h、MyLock.h、MyTime.h、以及测试程序windowsTest.cpp放到新建的VS项目即可。
    然后将我提供的mysql库链进入，win链的时候只需要链libmysql.lib这个库，然后将libmysql.dll拷贝到exe路径下即可。