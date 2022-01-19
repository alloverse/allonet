
#ifndef alloclient_simple_api_h
#define alloclient_simple_api_h

int alloclient_simple_init(int threaded);
int alloclient_simple_free();
char *alloclient_simple_communicate(const char * const command);

#endif /* alloclient_simple_api_h */
