#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
	int i = 0;
	char* words[27] = {"equals", "keep", "play.\n", "X", "=", "work.\n",
			   "mouth", "shut.\n\n", "Y", "Z", "is", "formula",
			   "is", "+", "+", "then", "A", "Y", "Z.\n",
			   "success,", "\n\n If", "the", "A", "is", "X",
			   "is", "your"};

	if (argc != 28) {
		fprintf(stderr, "Usage: tester <int1> <int2> ... <int27>\n");
		exit(1);
	}
	
	for (i = 1; i < 28; ++i) {
		int idx = atoi(argv[i]);
		if (i >= 0 && i <= 27)
			printf("%s ", words[idx]);
	}

	return(0);
}
