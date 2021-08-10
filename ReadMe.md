本连接池是参考其它github项目的，然后自己重新写了一遍，并且优化了部分代码，自己添加了一些注释和需要注意的地方。

1. 平台：
只支持Linux，不过可以自己改一下代码就能在Windows或者其它平台使用，因为我只使用了Linux特有的posix标准的锁。

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
