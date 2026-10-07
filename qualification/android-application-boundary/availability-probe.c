/* Isolate an ABI-independent Android API-level declaration requirement. */
int introduced_api(void)
    __attribute__((availability(android, introduced=26, strict)));
