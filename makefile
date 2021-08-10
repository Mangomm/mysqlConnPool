# false(其它值)执行test.cpp，true执行test1.cpp，并且注意注释不能写在who=true后面，否则不生效。
who=true

ifeq ($(who),false)
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

else
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

.PHONY:clean ALL    # 防止执行make clean当前目录有clean文件名而不能成功删除

endif