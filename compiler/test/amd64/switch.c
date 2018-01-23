/*
 * A test for compiling switch statements
 * Fri Feb  7 14:28:08 EST 2014
 */

foo(u) {
	printf("foo %d\n", u);
	switch(u) {
		case 324:
			printf("yes\n");
			break;
		case 54:
			printf("is ");
		case 76:
			printf("dog\n");
			break;
		case 25:
			printf("hahaha\n");
			break;
		case 120:
			printf("hello\n");
			break;
		case 29:
			printf("this ");
			break;
		default:
			printf("\n");
	}
}

bar(u) {
	switch(u) {
		case 0:
			printf("that's teh way\n");
			return;
		case 3:
			printf("i liek it\n");
			return;
	}
	printf("aha\n");
}

baz(u) {
	switch(u) {
		case 0: {
			printf("yes but pepsi energy fields\n");
			if (1 == 1)
				break;
		}
		case 1:
			printf("bad compiler me very angry blob\n");
	}
}

donut(u) {
	switch(u) {
		case '0':
			printf("zero\n");
			break;
		case '1':
			printf("one\n");
			break;
	}
}

main() {
	int i;
	int ind[] = {-1, -2, 322, 324, 323, 325, 3, 20000, 10000, 120, 1, 2, 24, 25, 26, 29, 54};
	for (i = 0; i < sizeof(ind)/sizeof(int); ++i)
		foo(ind[i]);

	printf("ladies and gentlemen, salad and kings, anchovies, bacon\n");
	for (i = 0; i < 8; ++i)
		bar(i % 4);

	baz(0);

	donut('0'); donut('0'); donut('1'); donut('1');
}
