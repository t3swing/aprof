#ifndef __READ_SYM_H__
#define __READ_SYM_H__
#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

char * addrToFunc(ADDR_T addr,ADDR_T * addrOut);
int loadElfSym(const char * elfPath,int pid);
int unloadElfSym();

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* __READ_SYM_H__ */