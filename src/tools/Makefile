ifeq (,$(TOOLS))
all:
	make -C .. TOOLS=1
endif

APPLICATIONS += src/tools/vc1parser
ALL_SOURCES  += $(wildcard src/tools/*.cpp)

vc1parser_DEPENDENCIES = $(DEP_COMMON)
vc1parser_SOURCES = src/tools/vc1parser.cpp
vc1parser_OBJECTS = $(patsubst %.cpp,%.o,$(vc1parser_SOURCES))
vc1parser_LDADD = -lmtxcommon $(MAGIC_LIBS) -lmatroska -lebml \
		-lz $(COMPRESSION_LIBRARIES) -lexpat $(ICONV_LIBS) \
		$(LIBINTL_LIBS) $(LIBRPCRT)

vc1parser: src/tools/vc1parser$(EXEEXT)

src/tools/vc1parser$(EXEEXT): $(vc1parser_OBJECTS) $(vc1parser_DEPENDENCIES)
	@echo '    LINK ' $@
	$(Q)$(LINK) -o $@ $(vc1parser_OBJECTS) $(vc1parser_LDADD)
