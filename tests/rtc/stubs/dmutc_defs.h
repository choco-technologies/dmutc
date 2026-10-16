#ifndef TEST_DMUTC_DEFS_H
#define TEST_DMUTC_DEFS_H
#define dmod_dmutc_api(v, ret, name, args) ret dmutc##name args
#define dmod_dmutc_api_declaration(v, ret, name, args) ret dmutc##name args
#endif
