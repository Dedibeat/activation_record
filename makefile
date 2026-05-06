BIN := bin
CC := cc
CFLAGS := -g -I. -I$(BIN)
OBJS := $(addprefix $(BIN)/, semanttest.o y.tab.o lex.yy.o errormsg.o symbol.o absyn.o env.o types.o semant.o table.o util.o temp.o x86_64frame.o translate.o printtree.o tree.o)

$(BIN)/a.out: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $@

$(BIN):
	mkdir -p $(BIN)

$(BIN)/%.o: %.c | $(BIN)
	$(CC) $(CFLAGS) -c $< -o $@

$(BIN)/semanttest.o: semanttest.c | $(BIN)
	$(CC) $(CFLAGS) -c $< -o $@

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

all-tests := $(addsuffix .test, $(notdir $(basename $(wildcard ../testcases_correct/*.tig))))

test: $(all-tests)

test49.test:
	@echo "Skip test49.tig"

%.test: ../testcases_correct/%.tig
	mkdir -p $(BIN)/testoutput
	$(BIN)/a.out $< > $(BIN)/testoutput/$@ 2>&1
	echo "==============================" >> $(BIN)/testoutput/$@
	cat $< >> $(BIN)/testoutput/$@
