BIN := bin
CC := gcc
CFLAGS := -std=c11 -g -I. -I$(BIN)
OBJS := $(addprefix $(BIN)/, semanttest.o y.tab.o lex.yy.o errormsg.o symbol.o absyn.o env.o types.o semant.o table.o util.o temp.o x86_64frame.o translate.o printtree.o tree.o escape.o)
CH6_UNIT_OBJS := $(addprefix $(BIN)/, ch6_unit_tests.o symbol.o absyn.o table.o util.o temp.o x86_64frame.o translate.o printtree.o tree.o escape.o)

$(BIN)/a.out: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $@

$(BIN):
	mkdir -p $(BIN)

$(BIN)/%.o: %.c | $(BIN)
	$(CC) $(CFLAGS) -c $< -o $@

$(BIN)/semanttest.o: semanttest.c | $(BIN)
	$(CC) $(CFLAGS) -c $< -o $@

$(BIN)/ch6_unit_tests.o: tests/ch6/ch6_unit_tests.c | $(BIN)
	$(CC) $(CFLAGS) -c $< -o $@

$(BIN)/ch6_unit_tests: $(CH6_UNIT_OBJS)
	$(CC) $(CFLAGS) $(CH6_UNIT_OBJS) -o $@

$(BIN)/y.tab.o: $(BIN)/y.tab.c $(BIN)/y.tab.h | $(BIN)
	$(CC) $(CFLAGS) -c $(BIN)/y.tab.c -o $@

$(BIN)/lex.yy.o: $(BIN)/lex.yy.c $(BIN)/y.tab.h | $(BIN)
	$(CC) $(CFLAGS) -c $(BIN)/lex.yy.c -o $@

$(BIN)/y.tab.c $(BIN)/y.tab.h: tiger.grm | $(BIN)
	cd $(BIN) && yacc -dv ../tiger.grm

$(BIN)/lex.yy.c: tiger.lex $(BIN)/y.tab.h | $(BIN)
	cd $(BIN) && lex ../tiger.lex

clean:
	rm -rf $(BIN)

external-tests := $(addsuffix .test, $(notdir $(basename $(wildcard ../testcases_correct/*.tig))))
book-tests := $(patsubst tests/book/%.tig,$(BIN)/testoutput/book/%.out,$(wildcard tests/book/*.tig))
compilable-tests := $(patsubst tests/compilable/%.tig,$(BIN)/testoutput/compilable/%.out,$(wildcard tests/compilable/*.tig))
uncompilable-tests := $(patsubst tests/uncompilable/%.tig,$(BIN)/testoutput/uncompilable/%.out,$(wildcard tests/uncompilable/*.tig))

.PHONY: clean test ch6-unit-tests ch6-ir-tests tiger-tests book-tests compilable-tests uncompilable-tests external-tests

test: ch6-unit-tests ch6-ir-tests tiger-tests external-tests

tiger-tests: book-tests compilable-tests uncompilable-tests

book-tests: $(book-tests)

compilable-tests: $(compilable-tests)

uncompilable-tests: $(uncompilable-tests)

external-tests: $(external-tests)

ch6-unit-tests: $(BIN)/ch6_unit_tests
	$(BIN)/ch6_unit_tests

ch6-ir-tests: $(BIN)/a.out
	sh tests/ch6/run_ir_tests.sh $(BIN)/a.out $(BIN)/testoutput/ch6

test49.test:
	@echo "Skip test49.tig"

%.test: ../testcases_correct/%.tig
	mkdir -p $(BIN)/testoutput
	$(BIN)/a.out $< > $(BIN)/testoutput/$@ 2>&1
	echo "==============================" >> $(BIN)/testoutput/$@
	cat $< >> $(BIN)/testoutput/$@

$(BIN)/testoutput/book/%.out: tests/book/%.tig $(BIN)/a.out
	mkdir -p $(BIN)/testoutput/book
	TIGER_PARSE_ONLY=1 $(BIN)/a.out $< > $@ 2>&1 || { code=$$?; if [ $$code -ge 128 ]; then exit $$code; fi; }
	echo "==============================" >> $@
	cat $< >> $@

$(BIN)/testoutput/compilable/%.out: tests/compilable/%.tig $(BIN)/a.out
	mkdir -p $(BIN)/testoutput/compilable
	TIGER_PARSE_ONLY=1 $(BIN)/a.out $< > $@ 2>&1
	echo "==============================" >> $@
	cat $< >> $@

$(BIN)/testoutput/uncompilable/%.out: tests/uncompilable/%.tig $(BIN)/a.out
	mkdir -p $(BIN)/testoutput/uncompilable
	if $(BIN)/a.out $< > $@ 2>&1; then \
		if ! grep -Eiq "error|illegal|invalid|unclosed|unterminated|incompatible|not inside" $@; then \
			echo "FAIL $<: expected compiler diagnostic" >&2; \
			exit 1; \
		fi; \
	else \
		code=$$?; \
		if [ $$code -ge 128 ]; then exit $$code; fi; \
	fi
	echo "==============================" >> $@
	cat $< >> $@
