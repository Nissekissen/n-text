#include "file.h"

#include <fcntl.h>
#include <sys/stat.h>

void save_file(editor_t *editor, char *dst) {
    char *buf = malloc(1024);
    size_t buf_len = 0, buf_cap = 1024;
    rope_collect(&editor->root, &buf, &buf_len, &buf_cap);

    if (buf_len == 0 || buf[buf_len - 1] != '\n') {
        safe_append(&buf, &buf_len, &buf_cap, "\n", 1);
    }

    int fd = open(dst, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) return; // TODO! add error handling here
    write(fd, buf, buf_len);
    close(fd);

    free(buf);
}

void load_file(editor_t *editor, char *path) {
    int fd = open(path, O_RDONLY);
    if (fd == -1) return; // TODO! Handle error
    
    struct stat st;
    fstat(fd, &st);
    size_t size = st.st_size;

    char *buf = malloc(size);
    size_t total_read = 0;
    while (total_read < size) {
        ssize_t n = read(fd, buf + total_read, size - total_read);
        if (n <= 0) break;
        total_read += n;
    }

    close(fd);

    rope_insert(&editor->root, 0, buf, size);

    free(buf);
}

