generate-proto:
	protoc-c --proto_path=../OSM-binary/src --c_out=. ../OSM-binary/src/*.proto

all:
	gcc -D_GNU_SOURCE -Wall -pedantic -std=c99 -O0 -ggdb -o test main.c fileformat.pb-c.c osmformat.pb-c.c -lprotobuf-c -lz -lbz2
