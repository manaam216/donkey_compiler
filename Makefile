donkey: main.c lex_tokenization.c ast_algo_parser.c generate_asm.c
	cc -o donkey -g main.c lex_tokenization.c ast_algo_parser.c generate_asm.c

clean:
	rm -f donkey *.o