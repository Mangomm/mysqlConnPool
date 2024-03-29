# 0(其它值)执行test.cpp，1执行test1.cpp，并且注意注释不能写在who=1后面，否则不生效。
who=3

ifeq ($(who),0)
$(info ============= test =============)
CPPFLAGS = -std=c++11 -Wall -g -gstabs
test : test.o DBPool.o
	g++ $(CPPFLAGS) -o test test.o DBPool.o -lmysqlclient -lpthread

test.o : test.cpp DBDefine.h
	g++ $(CPPFLAGS) -c test.cpp

DBPool.o : DBPool.cpp DBPool.h MyTime.h MyLock.h
	g++ $(CPPFLAGS) -c DBPool.cpp 

clean:
	rm -rf test
	rm -rf test.o
	rm -rf DBPool.o

.PHONY : test clean

else ifeq ($(who), 1)
$(info ============= test1 =============)
#all代表要生成的目标，可以有多个,从右往左开始执行
ALL:test1

CPPFLAGS = -std=c++11 -Wall -g -gstabs
test1 : test1.o DBPool.o
	g++ $(CPPFLAGS) -o test1 test1.o DBPool.o -lmysqlclient -lpthread

test1.o : test1.cpp DBDefine.h
	g++ $(CPPFLAGS) -c test1.cpp

DBPool.o : DBPool.cpp DBPool.h MyTime.h MyLock.h
	g++ $(CPPFLAGS) -c DBPool.cpp 

clean:
	rm -rf test1
	rm -rf test1.o
	rm -rf DBPool.o

# 防止执行make clean当前目录有clean文件名而不能成功删除
.PHONY:clean ALL   

else ifeq ($(who), 2)
$(info ============= test2 =============)
#all代表要生成的目标，可以有多个,从右往左开始执行
ALL:test2

CPPFLAGS = -std=c++11 -Wall -g -gstabs
test2 : test2.o DBPool.o
	g++ $(CPPFLAGS) -o test2 test2.o DBPool.o -lmysqlclient -lpthread

test2.o : test2.cpp DBDefine.h
	g++ $(CPPFLAGS) -c test2.cpp

DBPool.o : DBPool.cpp DBPool.h MyTime.h MyLock.h
	g++ $(CPPFLAGS) -c DBPool.cpp 

clean:
	rm -rf test2
	rm -rf test2.o
	rm -rf DBPool.o

# 防止执行make clean当前目录有clean文件名而不能成功删除
.PHONY:clean ALL
else
$(info ============= test3 =============)
#all代表要生成的目标，可以有多个,从右往左开始执行
ALL:test3

CPPFLAGS = -std=c++11 -Wall -g -gstabs
test3 : test3.o DBPool.o
	g++ $(CPPFLAGS) -o test3 test3.o DBPool.o -lmysqlclient -lpthread

test3.o : test3.cpp DBDefine.h
	g++ $(CPPFLAGS) -c test3.cpp

DBPool.o : DBPool.cpp DBPool.h MyTime.h MyLock.h
	g++ $(CPPFLAGS) -c DBPool.cpp 

clean:
	rm -rf test3
	rm -rf test3.o
	rm -rf DBPool.o

.PHONY:clean ALL    

endif