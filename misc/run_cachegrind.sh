date=`date "+%d-%m-%Y-%H-%M-%S"`
mkdir "cgo/$date"
valgrind --tool=callgrind --trace-children=yes --trace-children-skip=*clang*,*luajit --cache-sim=yes --branch-sim=yes --callgrind-out-file=/home/sushi/src/enosi/cgo/$date/callgrind.out.%p bin/lake
