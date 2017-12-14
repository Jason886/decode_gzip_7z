#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>

// 1MB的buf
#define PIPE_BUF_SIZE 1024*1024

int gzip_d(char *in, size_t in_len, char **ou, size_t *ou_len) {
    int rc = -1;
    int tmp_n = 0, tmp_fd = -1;
    char tmp_name[128];
    char command[256];
    char *pipe_buf = 0;
    FILE *pipe_file = 0;

    if (0 != access("tmp", F_OK) != 0) {
        if (0 != mkdir("tmp", S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH)) {
            return -1;
        }
    }

cre_tmp:
    strcpy(tmp_name, "tmp/tmp_XXXXXX");
    tmp_fd = mkstemp(tmp_name);
    tmp_n++;
    if (tmp_fd < 0) {
        if (tmp_n < 3) {
            usleep(200000); // sleep 200ms
            goto cre_tmp;
        }
        return -1;
    }

    if (in && in_len > 0) {
        if (in_len !=  write(tmp_fd, in, in_len)) {
            goto end;
        }
    }

    close(tmp_fd);
    tmp_fd = -1;

    // 7za
    //snprintf(command, sizeof(command), "7za x %s -so 2>/dev/null", tmp_name);
    // gzip
    //snprintf(command, sizeof(command), "cat %s | gzip -cd 2>/dev/null", tmp_name);
    // zip
    snprintf(command, sizeof(command), "unzip -p %s 2>/dev/null", tmp_name);
    pipe_buf = malloc(PIPE_BUF_SIZE); 
    if (!pipe_buf) {
        goto end;
    }
    pipe_file = popen(command, "r");
    if (!pipe_file) {
        goto end;
    }

    *ou = 0;
    *ou_len = 0;
    int r = 0;
    while ((r = fread(pipe_buf, 1, PIPE_BUF_SIZE, pipe_file)) > 0) {
        if(!*ou) {
            *ou = malloc(r);
            if(!*ou) {
                goto end;
            }
            memcpy(*ou, pipe_buf, r);
        } else {
            char *p = realloc(*ou, *ou_len + r);
            if(!p) {
                free(*ou);
                *ou = 0;
                *ou_len = 0;
                goto end;
            }
            memcpy(p + *ou_len, pipe_buf, r);
            *ou = p;
        }
        *ou_len += r;
    }

    rc = 0;

end:
    if(tmp_fd > 2) { // 0-stdin 1-stdout 2-stderr
        close(tmp_fd);
    }
    if(pipe_buf) {
        free(pipe_buf);
    }
    if(pipe_file) {
        pclose(pipe_file);
    }
    unlink(tmp_name);
    return rc;
}

int main(int argc, char **argv) {
    if(argc < 2) {
        fprintf(stderr, "argc error\n");
        return -1;
    }
    FILE *file = fopen(argv[1], "r");
    if(!file) {
        fprintf(stderr, "open file failed: %s\n", argv[1]);
        return -1;
    }
    fseek(file, 0, SEEK_END);
    int len = ftell(file);
    fseek(file, 0, SEEK_SET);
    char *in = malloc(len);
    fread(in, 1, len, file);

    char *ou = 0;
    size_t ou_len = 0;

    int status = gzip_d(in, len, &ou, &ou_len);
    if(0 != status || !ou || ou_len <=0) {
        fprintf(stderr, "decode error\n");
        return -1;
    }
    write(1, ou, ou_len);

    return 0;
}
