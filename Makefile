CC := clang
CFLAGS := -g -Wall -Wno-deprecated-declarations -Werror 

all: tron

clean:
	rm -f tron

tron: tron.c util.c util.h scheduler.c scheduler.h
	$(CC) $(CFLAGS) -o tron tron.c util.c scheduler.c -lncurses

zip:
	@echo "Generating tron.zip file to submit to Gradescope..."
	@zip -q -r tron.zip . -x .git/\* .vscode/\* .clang-format .gitignore tron
	@echo "Done. Please upload tron.zip to Gradescope."

format:
	@echo "Reformatting source code."
	@clang-format -i --style=file $(wildcard *.c) $(wildcard *.h)
	@echo "Done."

.PHONY: all clean zip format
