#!/bin/bash
user="root"
pass="123456"
db="test"
sql="sql1.sql"
mysql -u${user} -p${pass} $db < $sql
