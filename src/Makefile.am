bin_PROGRAMS = pbf2osm

pbf2osm_SOURCES = \
  fileformat.pb-c.c fileformat.pb-c.h \
  osmformat.pb-c.c osmformat.pb-c.h \
  main.c

BUILT_SOURCES = \
  fileformat.pb-c.c fileformat.pb-c.h \
  osmformat.pb-c.c osmformat.pb-c.h

AM_CFLAGS = -Wall -Wextra -pedantic
AM_LDFLAGS = -lprotobuf-c -lz -lbz2

# protobuf C default rules

%.pb-c.c: ../OSM-binary/src/%.proto
	 $(AM_V_GEN) $(PROTOC_C) --proto_path=../OSM-binary/src --c_out=. $<

%.pb-c.h: %.pb-c.c
	@if test ! -f $@; then \
	  rm -f $<; \
	  $(MAKE) $(AM_MAKEFLAGS) $<; \
	else :; fi

