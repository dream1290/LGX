static int failures = 0;
int main(void) { int x = failures == 0; failures = 0; return x ? 0 : 1; }
