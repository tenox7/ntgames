#include <stdio.h>
#include <string.h>
#include <io.h>      /* For _findfirst, _findnext */
#include <stdlib.h>  /* For qsort */
#include <search.h>  /* For _findfirst on some compilers */

#define MAX_LEVELS 100
#define MAX_PATH 260

/* String comparison function for qsort */
int CompareStrings(const void *a, const void *b) {
    return strcmp(*(const char **)a, *(const char **)b);
}

int main() {
    struct _finddata_t findData;
    long hFind; /* Changed from intptr_t to long for old Visual Studio */
    char searchPath[MAX_PATH];
    char levelFiles[MAX_LEVELS][MAX_PATH];
    char *levelFilePointers[MAX_LEVELS];
    int numLevels = 0;
    int i;
    FILE *rcFile, *headerFile;
    
    printf("Generating level resources...\n");
    
    /* Set search path */
    strcpy(searchPath, "levels\\*.sok");
    
    /* Find first .sok file */
    hFind = _findfirst(searchPath, &findData);
    if (hFind == -1) {
        printf("No level files found!\n");
        return 1;
    }
    
    /* Collect all level files */
    do {
        if (numLevels < MAX_LEVELS) {
            sprintf(levelFiles[numLevels], "levels\\%s", findData.name);
            levelFilePointers[numLevels] = levelFiles[numLevels];
            numLevels++;
        }
    } while (_findnext(hFind, &findData) == 0 && numLevels < MAX_LEVELS);
    
    _findclose(hFind);
    
    /* Sort the level filenames alphabetically */
    qsort(levelFilePointers, numLevels, sizeof(char *), CompareStrings);
    
    /* Create RC file for levels */
    rcFile = fopen("levels.rc", "w");
    if (!rcFile) {
        printf("Failed to create levels.rc!\n");
        return 1;
    }
    
    /* Create header file for level IDs */
    headerFile = fopen("levels.h", "w");
    if (!headerFile) {
        fclose(rcFile);
        printf("Failed to create levels.h!\n");
        return 1;
    }
    
    /* Write header file content */
    fprintf(headerFile, "/* Auto-generated level resource IDs */\n");
    fprintf(headerFile, "#ifndef LEVELS_H\n");
    fprintf(headerFile, "#define LEVELS_H\n\n");
    fprintf(headerFile, "#define IDR_LEVEL_BASE 3000\n");
    fprintf(headerFile, "#define NUM_LEVELS %d\n\n", numLevels);
    
    /* Write RC file content */
    fprintf(rcFile, "/* Auto-generated level resources */\n");
    
    /* Generate resource definitions for each level */
    for (i = 0; i < numLevels; i++) {
        /* For .h file, define both the individual resource IDs and the calculation method */
        fprintf(headerFile, "#define IDR_LEVEL_%d %d\n", i, 3000 + i);
        
        /* For .rc file, use the same resource identifiers - make sure full path uses backslashes */
        {
            char correctedPath[MAX_PATH];
            char *p;
            
            /* Copy the path */
            strcpy(correctedPath, levelFilePointers[i]);
            
            /* Convert any forward slashes to backslashes */
            for (p = correctedPath; *p; p++) {
                if (*p == '/') {
                    *p = '\\';
                }
            }
            
            fprintf(rcFile, "%d RCDATA \"%s\"\n", 3000 + i, correctedPath);
        }
    }
    
    fprintf(headerFile, "\n#endif /* LEVELS_H */\n");
    
    fclose(rcFile);
    fclose(headerFile);
    
    printf("Generated resources for %d level files\n", numLevels);
    return 0;
}