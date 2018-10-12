#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <elf.h>
#include <dirent.h>
#include "deps.h"
#include "misc.h"

#define INVALID                 (-1)
#define INCREASE_STEP           1000

#define SECTION_NAME_SHSTRTAB   ".shstrtab"

#define SECTION_NAME_SYMTAB     ".symtab"
#define SECTION_NAME_STRTAB     ".strtab"

#define SECTION_NAME_DYNSYM     ".dynsym"
#define SECTION_NAME_DYNSTR     ".dynstr"

#define SECTION_NAME_DYNAMIC    ".dynamic"

#define VDSO                    "vdso"

typedef struct
{
    FILE * fp;
    int procMemOffset;
    Elf32_Ehdr * ehdr;
    Elf32_Shdr * shdr;  /* include all section header*/
    char * strtab;      /* string table section */
} SYM_ELF_S;

typedef struct
{
    char * soPath;
    char * soName;
    char * name;        /* linkName */
    ADDR_T mapStart;    /* .text section(r-xp) start address in /proc/pid/maps */
    int size;           /* size of the .text section */
} SYM_SO_S;

typedef struct
{
    ADDR_T addr;
    int size;
    char * funcName;
} SYM_FUNC_S;

typedef struct
{
    ARRAY_PS symElfArray;    /* for SYM_ELF_S */
    ARRAY_PS symSoArray;     /* for SYM_SO_S */
    ARRAY_PS symFuncArray;   /* for SYM_FUNC_S */
} SYM_S;

SYM_S gSym = {0};

char * readFile(FILE * fp, int offset, int size)
{
    char * mem = NULL;

    mem = calloc(size, 1);
    CHECK_RTNM(mem == NULL, NULL, "malloc error,%s\n", strerror(errno));

    fseek(fp, offset, SEEK_SET);
    fread(mem, 1, size, fp);

    return mem;
}

int releaseSymElf(SYM_ELF_S * symElf)
{
    if (symElf->fp)
    {
        fclose(symElf->fp);
    }
    if (symElf->ehdr)
    {
        free(symElf->ehdr);
    }
    if (symElf->shdr)
    {
        free(symElf->shdr);
    }
    if (symElf->strtab)
    {
        free(symElf->strtab);
    }
    memset(symElf, 0, sizeof(SYM_ELF_S));

    return 0;
}

int releaseElf(ARRAY_PS symElfArray)
{
    int i = 0, nMemb = 0;
    CHECK_RTN((symElfArray == NULL), 0);

    SYM_ELF_S * symElf = arrayMutable(symElfArray, &nMemb);
    if (symElf)
    {
        for (i = 0; i < nMemb; i++, symElf++)
        {
            releaseSymElf(symElf);
        }
    }

    arrayDel(symElfArray);

    return 0;
}


int releaseSo(ARRAY_PS symSoArray)
{
    int i = 0, nMemb = 0;
    CHECK_RTN((symSoArray == NULL), 0);

    SYM_SO_S * symSo = arrayMutable(symSoArray, &nMemb);
    if (symSo)
    {
        for (i = 0; i < nMemb; i++, symSo++)
        {
            if (symSo->soPath)
            {
                free(symSo->soPath);
            }
            if (symSo->name)
            {
                free(symSo->soName);
            }
            if (symSo->name)
            {
                free(symSo->name);
            }
            memset(symSo, 0, sizeof(SYM_SO_S));
        }
    }

    arrayDel(symSoArray);

    return 0;
}

int releaseFunc(ARRAY_PS symFuncArray)
{
    int i = 0, nMemb = 0;
    CHECK_RTN((symFuncArray == NULL), 0);

    SYM_FUNC_S * symFunc = arrayMutable(symFuncArray, &nMemb);
    if (symFunc)
    {
        for (i = 0; i < nMemb; i++, symFunc++)
        {
            if (symFunc->funcName)
            {
                free(symFunc->funcName);
            }
            memset(symFunc, 0, sizeof(SYM_FUNC_S));
        }
    }
    arrayDel(symFuncArray);

    return 0;
}

Elf32_Shdr * getSHdrByName(SYM_ELF_S * symElf, const char * name, Elf32_Word sh_type)
{
    int i = 0, shnum = symElf->ehdr->e_shnum;
    char * nameStr = NULL;

    Elf32_Shdr * shdr = symElf->shdr;
    for (i = 0; i < shnum; i++, shdr++)
    {
        if (shdr->sh_type == sh_type)
        {
            nameStr = symElf->strtab + shdr->sh_name;
            if (nameStr && strcmp(nameStr, name) == 0)
            {
                return shdr;
            }
        }
    }

    DBG_ERR("Get section header failed name:%s sh_type:%d\n", name, sh_type);
    return NULL;
}

char * getStrTabByName(SYM_ELF_S * symElf, const char * name)
{
    int i = 0, shnum = symElf->ehdr->e_shnum;
    char * nameStr = NULL;

    Elf32_Shdr * shdr = symElf->shdr;
    for (i = 0; i < shnum; i++, shdr++)
    {
        if (shdr->sh_type == SHT_STRTAB)
        {
            nameStr = symElf->strtab + shdr->sh_name;
            if (nameStr && strcmp(nameStr, name) == 0)
            {
                char * strTab = readFile(symElf->fp, shdr->sh_offset, shdr->sh_size);
                CHECK_RTNM(symElf->strtab == NULL, NULL, "getStrTabByName error!\n");
                return strTab;
            }
        }
    }

    DBG_ERR("get StrTab failed name:%s \n", name);
    return NULL;
}

int getSHStrTab(SYM_ELF_S * symElf)
{
    int i = 0, shnum = symElf->ehdr->e_shnum;
    char * nameStr = NULL;

    CHECK_RTN((symElf->ehdr == NULL) || (symElf->shdr == NULL), -1)

    Elf32_Shdr * shdr = symElf->shdr;
    for (i = 0; i < shnum; i++, shdr++)
    {
        if (shdr->sh_type == SHT_STRTAB)
        {
            if (shdr->sh_name >= shdr->sh_size)
            {
                continue;
            }

            nameStr = readFile(symElf->fp, shdr->sh_offset + shdr->sh_name, strlen(SECTION_NAME_SHSTRTAB) + 1);
            CHECK_RTNM(nameStr == NULL, -1, "getSHStrTab read nameStr error!\n");
            if (strcmp(nameStr, SECTION_NAME_SHSTRTAB) == 0)
            {
                free(nameStr);
                symElf->strtab = readFile(symElf->fp, shdr->sh_offset, shdr->sh_size);
                CHECK_RTNM(symElf->strtab == NULL, -1, "getStrTabStr error!\n");
                return 0;
            }
            free(nameStr);
        }
    }

    return -1;
}

int getSectionHeaders(SYM_ELF_S * symElf)
{
    if (symElf->ehdr && symElf->shdr == NULL)
    {
        int shnum = symElf->ehdr->e_shnum;
        symElf->shdr = (Elf32_Shdr *)readFile(symElf->fp, symElf->ehdr->e_shoff, sizeof(Elf32_Shdr) * shnum);
        CHECK_RTNM(symElf->shdr == NULL, -1, "getSectionHeaders error!\n");
    }

    return 0;
}

int getElfHeader(SYM_ELF_S * symElf)
{
    symElf->ehdr = (Elf32_Ehdr *)readFile(symElf->fp, 0, sizeof(Elf32_Ehdr));
    CHECK_RTNM(symElf->ehdr == NULL, -1, "read Elf32_Ehdr error!\n");

    unsigned char * e_ident = symElf->ehdr->e_ident;
    if ((e_ident[EI_MAG0] != ELFMAG0) || (e_ident[EI_MAG1] != ELFMAG1) ||
            (e_ident[EI_MAG2] != ELFMAG2) || (e_ident[EI_MAG2] != ELFMAG2))
    {
        printf("Not elf format!\n");
        return -1;
    }

    return 0;
}

int getSoRealPath(char * soLink, char ** soName, char ** soPath)
{
    char realPath[256] = {0};
    char fullPath[256] = {0};
    char * defaultSoPath = "/lib:/usr/local/lib:/usr/lib:/lib/i386-linux-gnu";

    char * envLD = getenv("LD_LIBRARY_PATH");
    if (envLD == NULL)
    {
        envLD = defaultSoPath;
    }

    char * envSplit = strdup(envLD);
    char * split = envSplit;
    char * p = strchr(split, ':');
    while (split)
    {
        if (p)
        {
            *p = '\0';
        }

        sprintf(fullPath, "%s/%s", split, soLink);
        if (0 == access(fullPath, R_OK))
        {
            readlink(fullPath, realPath, 256);
            sprintf(fullPath, "%s/%s", split, realPath);
            if (soPath)
            {
                *soPath = strdup(fullPath);
            }
            if (soName)
            {
                *soName = strdup(realPath);
            }
            free(envSplit);
            return 0;
        }

        if (p == NULL)
        {
            break;
        }

        split = ++p;
        p = strchr(p, ':');
    }

    free(envSplit);
    return -1;
}

int releaseSymTabStr(void * sym, void * strTab)
{
    if (strTab)
    {
        free(strTab);
    }
    if (sym)
    {
        free(sym);
    }
    return 0;
}

int getShareObject(SYM_ELF_S * symElf, ARRAY_PS symSo)
{
    int i = 0;
    SYM_SO_S * symSO = NULL;

    Elf32_Shdr * shdrDynamic = getSHdrByName(symElf, SECTION_NAME_DYNAMIC, SHT_DYNAMIC);
    CHECK_RTNM(shdrDynamic == NULL, -1, "parseSymTab getSHdrByName faild\n");

    char * strTab = getStrTabByName(symElf, SECTION_NAME_DYNSTR);
    CHECK_RTNM(strTab == NULL, -1, "parseSymTab getStrTabByName faild\n");

    int dynamicCnt = shdrDynamic->sh_size / sizeof(Elf32_Dyn);
    Elf32_Dyn * dyn = (Elf32_Dyn *)readFile(symElf->fp, shdrDynamic->sh_offset , shdrDynamic->sh_size);
    CHECK_RTNM(dyn == NULL, -1, "parse sym faild:%s %s \n", SECTION_NAME_DYNAMIC, strTab);

    DBG_INFO("dynamicCnt:%d \n", dynamicCnt);
    for (i = 0; i < dynamicCnt; i++)
    {
        if (dyn[i].d_tag == DT_NEEDED)
        {
            symSO = arrayAdd(symSo, NULL);
            if (symSO)
            {
                symSO->name = strdup(strTab + dyn[i].d_un.d_val);
                getSoRealPath(symSO->name, &symSO->soName, &symSO->soPath);
            }
        }
    }
    releaseSymTabStr(dyn, strTab);

    return 0;
}

int loadSymTab(SYM_ELF_S * symElf, int offset, ARRAY_PS symFuncArray)
{
    int i = 0;
    SYM_FUNC_S * symFunc = NULL;

    Elf32_Shdr * shdrSymTab = getSHdrByName(symElf, SECTION_NAME_SYMTAB, SHT_SYMTAB);
    CHECK_RTNM(shdrSymTab == NULL, -1, "parseSymTab getSHdrByName faild\n");

    char * strTab = getStrTabByName(symElf, SECTION_NAME_STRTAB);
    CHECK_GOTOM(strTab == NULL, RELEASE, "parseSymTab getStrTabByName faild\n");

    int sym_cnt = shdrSymTab->sh_size / sizeof(Elf32_Sym);
    Elf32_Sym * sym = (Elf32_Sym *)readFile(symElf->fp, shdrSymTab->sh_offset , sym_cnt * sizeof(Elf32_Sym));
    CHECK_GOTOM(sym == NULL, RELEASE, "parse sym faild:%s %s \n", SECTION_NAME_SYMTAB, SECTION_NAME_STRTAB);

    DBG_INFO("sym_cnt:%d \n", sym_cnt);
    for (i = 0; i < sym_cnt; i++)
    {
        if (ELF32_ST_TYPE(sym[i].st_info) == STT_FUNC && sym[i].st_value)
        {
            symFunc = arrayAdd(symFuncArray, NULL);
            if (symFunc)
            {
                symFunc->addr = (ADDR_T)sym[i].st_value + offset;
                symFunc->size = sym[i].st_size;
                symFunc->funcName = strdup(strTab + sym[i].st_name);
            }
        }
    }

    releaseSymTabStr(sym, strTab);
    return 0;

RELEASE:
    releaseSymTabStr(sym, strTab);
    return -1;
}


int loadDynSym(SYM_ELF_S * symElf, int offset, ARRAY_PS symFuncArray)
{
    int i = 0;
    SYM_FUNC_S * symFunc = NULL;

    Elf32_Shdr * shdrSymTab = getSHdrByName(symElf, SECTION_NAME_DYNSYM, SHT_DYNSYM);
    CHECK_RTNM(shdrSymTab == NULL, -1, "parseSymTab getSHdrByName faild\n");

    char * strTab = getStrTabByName(symElf, SECTION_NAME_DYNSTR);
    CHECK_GOTOM(strTab == NULL, RELEASE, "parseSymTab getStrTabByName faild\n");

    int sym_cnt = shdrSymTab->sh_size / sizeof(Elf32_Sym);
    Elf32_Sym * sym = (Elf32_Sym *)readFile(symElf->fp, shdrSymTab->sh_offset , sym_cnt * sizeof(Elf32_Sym));
    CHECK_GOTOM(sym == NULL, RELEASE, "parse sym faild:%s %s \n", SECTION_NAME_DYNSYM, SECTION_NAME_DYNSTR);

    for (i = 0; i < sym_cnt; i++)
    {
        if (ELF32_ST_TYPE(sym[i].st_info) == STT_FUNC && sym[i].st_value)
        {
            symFunc = arrayAdd(symFuncArray, NULL);
            if (symFunc)
            {
                symFunc->addr = (ADDR_T)sym[i].st_value + offset;
                symFunc->size = sym[i].st_size;
                symFunc->funcName = strdup(strTab + sym[i].st_name);
            }
        }
    }
    releaseSymTabStr(sym, strTab);
    return 0;

RELEASE:
    releaseSymTabStr(sym, strTab);
    return -1;
}

int parseFile(SYM_ELF_S * symElf, const char * fileName)
{
    int ret = 0;

    symElf->fp = fopen(fileName, "rb");
    CHECK_RTNM(symElf->fp == NULL, -1, "file:%s open failed,%s\n", fileName, strerror(errno));

    /* parse elf header,it alse can check header */
    ret = getElfHeader(symElf);
    CHECK_RTNM(ret < 0, -1, "getElfHeader failed\n");

    ret = getSectionHeaders(symElf);
    CHECK_RTNM(ret < 0, -1, "getSectionHeaders failed\n");

    ret = getSHStrTab(symElf);
    CHECK_RTNM(ret < 0, -1, "getSHStrTab failed\n");

    return 0;
}

int getMapsPath(int tid, char * mapsPath, int size)
{
    char buf[256] = {0};

    /* it will find maps file follow this step */
    sprintf(buf, "/proc/%d/maps", tid);
    if (0 == access(buf, R_OK))
    {
        sstrcpy(mapsPath, buf, size);
        return 0;
    }

    /* should not excute */
    DIR * dir = opendir("/proc");
    CHECK_RTNM(dir == NULL, -1, "opendir proc failed,%s\n", strerror(errno));

    struct dirent * ptr;
    while ((ptr = readdir(dir)) != NULL)
    {
        if (ptr->d_type == DT_DIR)
        {
            sprintf(buf, "/proc/%s/task/%d/maps", ptr->d_name, tid);
            if (0 == access(buf, R_OK))
            {
                sstrcpy(mapsPath, buf, size);
                closedir(dir);
                return 0;
            }
        }
    }
    closedir(dir);

    return -1;
}

BOOL_E getSymSoOk(SYM_SO_S * symSo, char * addrBuff, char * soStr)
{
    ADDR_T begin, end;

    if (strstr(soStr, symSo->soName) == NULL)
    {
        return FALSE_E;
    }

    if (symSo->size == 0)
    {
        sscanf(addrBuff, "%lx-%lx", &begin, &end);
        symSo->mapStart = begin;
        symSo->size = end - begin;
        DBG_INFO("soName:%s mapStart:%#lx size:%d\n", symSo->soName, symSo->mapStart, symSo->size);
    }
    return TRUE_E;
}

int getSoOffset(int pid, ARRAY_PS symSoArray, ARRAY_PS symFuncArray)
{
    int ret = 0, i = 0, nMemb = 0;
    char mapsPath[256] = {0};
    char buf[1024] = {0};
    char * p = NULL;
    SYM_SO_S symVdso = {VDSO, VDSO, VDSO, 0L, 0};

    ret = getMapsPath(pid, mapsPath, 256);
    CHECK_RTNM(ret < 0, -1, "getMapsPath failed! pid:%d\n", pid);
    DBG_INFO("pid:%d mapsPath:%s ret:%d\n", pid, mapsPath, ret);

    FILE * fp = fopen(mapsPath, "r");
    CHECK_RTNM(fp == NULL, -1, "fopen %s failed! %s\n", mapsPath, strerror(errno));

    SYM_SO_S * symSo = arrayMutable(symSoArray, &nMemb);
    if (symSo == NULL)
    {
        fclose(fp);
        return 0;
    }

    while (fgets(buf, 1024, fp) != NULL)
    {
        p = strstr(buf, "r-xp");
        if (p == NULL)
        {
            continue;
        }
        *p = '\0';
        p++;

        /* check if this program use vdso */
        if (getSymSoOk(&symVdso, buf, p))
        {
            continue;
        }

        for (i = 0; i < nMemb; i++)
        {
            if (getSymSoOk(&symSo[i], buf, p))
            {
                break;
            }
        }
    }
    fclose(fp);

    /* add vdso to array */
    if (symVdso.mapStart > 0)
    {
        SYM_FUNC_S symFunc = {symVdso.mapStart, symVdso.size, VDSO};
        arrayAdd(symFuncArray, &symFunc);
    }

    return 0;
}

int sortCmp(const void * a, const void * b)
{
    return (*(ADDR_T *)a > *(ADDR_T *)b);
}

/* function has been sort by address,so can be use binary search */
char * addrToFunc(ADDR_T addr, ADDR_T * addrOut)
{
    int nMemb = 0, mid = 0, begin = 0, end = 0;
    SYM_FUNC_S * symFunc = NULL;

    CHECK_RTNM(addr == (ADDR_T)NULL, NULL, "Invalid addr");

    SYM_FUNC_S * symFuncTab = arrayMutable(gSym.symFuncArray, &nMemb);
    CHECK_RTNM(symFuncTab == NULL, NULL, "arrayMutable symFuncArray failed!\n");

    end = nMemb - 1;
    while (begin <= end)
    {
        mid = (begin + end) / 2;
        symFunc = &symFuncTab[mid];

        if (addr >= (symFunc->addr + symFunc->size))
        {
            begin = mid + 1;
            continue;
        }
        if (addr < symFunc->addr)
        {
            end = mid - 1;
            continue;
        }

        if (ISLIN(addr, symFunc->addr, symFunc->addr + symFunc->size))
        {
            if (addrOut)
            {
                *addrOut = symFunc->addr;
            }
            return symFunc->funcName;
        }
    }

    return NULL;
}

int loadElfSym(const char * elfPath, int pid)
{
    int ret = 0, i = 0, nMemb = 0;
    CHECK_RTNM(elfPath == NULL, -1, "elfPath invalid!\n");

    memset(&gSym, 0, sizeof(SYM_S));

    gSym.symElfArray = arrayNew(sizeof(SYM_ELF_S), 16);
    CHECK_GOTOM(gSym.symElfArray == NULL, RELEASE, "arrayNew symElf failed!\n");

    gSym.symSoArray = arrayNew(sizeof(SYM_SO_S), 16);
    CHECK_GOTOM(gSym.symSoArray == NULL, RELEASE, "arrayNew symElf failed!\n");

    gSym.symFuncArray = arrayNew(sizeof(SYM_FUNC_S), 1024);
    CHECK_GOTOM(gSym.symFuncArray == NULL, RELEASE, "arrayNew symElf failed!\n");

    SYM_ELF_S * symElf =  arrayAdd(gSym.symElfArray, NULL);
    CHECK_GOTOM(symElf == NULL, RELEASE, "arrayAdd symElf failed!\n");

    /* parse elf file and load symbol to symFuncArray */
    ret = parseFile(symElf, elfPath);
    CHECK_GOTOM(ret < 0 , RELEASE, "parseFile failed elfPath:%s!\n", elfPath);

    ret = loadSymTab(symElf, 0, gSym.symFuncArray);
    CHECK_GOTOM(ret < 0 , RELEASE, "loadSymTab failed!\n");

    ret = loadDynSym(symElf, 0, gSym.symFuncArray);
    CHECK_GOTOM(ret < 0 , RELEASE, "loadDynSym failed!\n");

    /* parse section .dynamic,get name of share object(.so) */
    ret = getShareObject(symElf, gSym.symSoArray);
    CHECK_GOTOM(ret < 0 , RELEASE, "getShareObject failed!\n");

    /* get each .so file offset by /proc/pid/maps */
    ret = getSoOffset(pid, gSym.symSoArray, gSym.symFuncArray);
    CHECK_GOTOM(ret < 0 , RELEASE, "getSoOffset failed!\n");

    /* free memory when load elf symbol completed */
    releaseSymElf(symElf);

    /* parse the found so file and load dynamic symbol */
    SYM_SO_S * symSo = arrayMutable(gSym.symSoArray, &nMemb);
    if (symSo)
    {
        for (i = 0; i < nMemb; i++, symSo++)
        {
            DBG_INFO("name:%s soName:%s soPath:%s addr:%#lx size:%d\n", symSo->name, symSo->soName, symSo->soPath, symSo->mapStart, symSo->size);
            symElf = arrayAdd(gSym.symElfArray, NULL);
            CHECK_GOTOM(symElf == NULL, RELEASE, "arrayAdd symElf failed!\n");

            ret = parseFile(symElf, symSo->soPath);
            CHECK_GOTOM(ret < 0 , RELEASE, "parseFile failed soPath:%s!\n", symSo->soPath);

            ret = loadDynSym(symElf, symSo->mapStart, gSym.symFuncArray);
            CHECK_GOTOM(ret < 0 , RELEASE, "loadDynSym failed!\n");

            releaseSymElf(symElf);
        }
    }
    /* just keep symbols in memory,release other completely */
    releaseElf(gSym.symElfArray);
    releaseSo(gSym.symSoArray);

    /* sort all function symbol by address */
    SYM_FUNC_S * symFunc =  arrayMutable(gSym.symFuncArray, &nMemb);
    CHECK_GOTOM(symFunc == NULL , RELEASE, "arrayMutable symFuncArray failed!\n");
    qsort(symFunc, nMemb, sizeof(SYM_FUNC_S), sortCmp);

#if 0
    for (i = 0; i < nMemb; i++, symFunc++)
    {
        printf("addr:%#lx size:%8d %s\n", symFunc->addr, symFunc->size, symFunc->funcName);
    }
#endif
    printf("symbol cnt:%d\n", nMemb);
    printf("loadElf finished!\n");
    return 0;

RELEASE:
    releaseElf(gSym.symElfArray);
    releaseElf(gSym.symSoArray);
    releaseElf(gSym.symFuncArray);

    return -1;
}

int unloadElfSym()
{
    releaseElf(gSym.symElfArray);
    releaseElf(gSym.symSoArray);
    releaseElf(gSym.symFuncArray);

    return 0;
}

