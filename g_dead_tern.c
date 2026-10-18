static int failures = 0;
static int never_called(void) { return 0; }
int main(void) { return (failures == 0) ? 0 : 1; }
