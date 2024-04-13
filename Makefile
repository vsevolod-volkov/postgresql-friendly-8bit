conv_friendly_8bit.so: conv_friendly_8bit.o utf8_util.o trace.o
	@echo Link...; test `uname` = "Darwin" && \
      cc \
         -bundle \
         -flat_namespace \
         -undefined suppress \
         -o $@ \
         $^ \
		|| \
		cc \
			-shared \
			-o $@ \
			$^

%.o: src/%.c
	@echo Compile $^...; test `uname` = "Darwin" && \
      cc \
         -I `pg_config --includedir-server` -I include \
         -c \
         -o $@ \
         $^ \
		|| \
      cc \
         -fPIC \
         -std=gnu99 \
         -I `pg_config --includedir-server` -I include \
         -c \
         -o $@ \
         $^

src/*.c: include/*.h
	@echo Touch $@ due to $? change...; \
   touch $@

.PHONY: clean rebuild
clean:
	@echo Clean...; rm *.o *.so 2>/dev/null || true

rebuild: clean conv_friendly_8bit.so
