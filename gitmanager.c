#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <ctype.h>

#define MAX_LINE_LENGTH 1024
#define MAX_PATH_LENGTH 256

typedef struct {
    char *repo;
    char *branch;
    char *path;
} Job;

typedef struct {
    Job *jobs;
    size_t count;
    size_t capacity;
} JobList;

static JobList jobList = {NULL, 0, 0};

static int is_safe_shell_char(char c) {
    if (isalnum((unsigned char)c)) return 1;
    if (strchr("./_:-@", c)) return 1;
    return 0;
}

static int is_string_safe(const char *str) {
    if (!str) return 1;
    for (int i = 0; str[i] != '\0'; i++) {
        if (!is_safe_shell_char(str[i])) return 0;
    }
    return 1;
}

static int dir_exists(const char *path) {
    struct stat info;
    return (stat(path, &info) == 0 && (info.st_mode & S_IFDIR));
}

static void add_job(const char *repo, const char *branch, const char *path) {
    if (jobList.count >= jobList.capacity) {
        size_t new_capacity = jobList.capacity == 0 ? 16 : jobList.capacity * 2;
        Job *new_jobs = (Job *)realloc(jobList.jobs, new_capacity * sizeof(Job));
        if (!new_jobs) exit(1);
        jobList.jobs = new_jobs;
        jobList.capacity = new_capacity;
    }

    Job *job = &jobList.jobs[jobList.count++];
    job->repo = strdup(repo);
    job->branch = branch ? strdup(branch) : NULL;
    job->path = strdup(path);
}

static void free_jobs(void) {
    for (size_t i = 0; i < jobList.count; i++) {
        free(jobList.jobs[i].repo);
        if (jobList.jobs[i].branch) free(jobList.jobs[i].branch);
        free(jobList.jobs[i].path);
    }
    free(jobList.jobs);
}

static char *extract_value(const char *line, const char *key) {
    const char *pos = strstr(line, key);
    if (!pos) return NULL;

    pos += strlen(key);
    const char *start_quote = strchr(pos, '"');
    if (!start_quote) return NULL;
    start_quote++;

    const char *end_quote = strchr(start_quote, '"');
    if (!end_quote) return NULL;

    size_t len = (size_t)(end_quote - start_quote);
    char *value = (char *)malloc(len + 1);
    if (value) {
        strncpy(value, start_quote, len);
        value[len] = '\0';
    }
    return value;
}

static void parse_config(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) return;

    char line[MAX_LINE_LENGTH];
    while (fgets(line, sizeof(line), file)) {
        char *p = line;
        while (isspace((unsigned char)*p)) p++;
        if (*p == '\0' || *p == '#') continue;

        char *repo = extract_value(line, "REPO");
        char *branch = extract_value(line, "BRANCH");
        char *path = extract_value(line, "PATH");

        if (!repo || !is_string_safe(repo) || !is_string_safe(branch) || !is_string_safe(path)) {
            free(repo); free(branch); free(path);
            continue;
        }

        if (!path) {
            const char *base = strrchr(repo, '/');
            base = base ? base + 1 : repo;
            const char *dotgit = strstr(base, ".git");
            size_t len = dotgit ? (size_t)(dotgit - base) : strlen(base);
            path = (char *)malloc(len + 3);
            if (path) {
                sprintf(path, "./%.*s", (int)len, base);
            }
        }

        int duplicate = 0;
        if (path) {
            for (size_t i = 0; i < jobList.count; i++) {
                if (strcmp(jobList.jobs[i].path, path) == 0) {
                    duplicate = 1; 
                    break;
                }
            }
        }

        if (!duplicate && path) {
            add_job(repo, branch, path);
        }

        free(repo); 
        free(branch); 
        free(path);
    }
    fclose(file);
}

static void process_jobs(void) {
    for (size_t i = 0; i < jobList.count; i++) {
        const Job *job = &jobList.jobs[i];
        if (dir_exists(job->path)) continue;

        char command[MAX_LINE_LENGTH * 2];
        if (job->branch) {
            snprintf(command, sizeof(command), "git clone -b %s %s %s", job->branch, job->repo, job->path);
        } else {
            snprintf(command, sizeof(command), "git clone %s %s", job->repo, job->path);
        }

        if (system(command) != 0) {
            fprintf(stderr, "Failed to clone %s\n", job->repo);
            exit(1);
        }
    }
}

int main(int argc, char * const * argv) {
    if (argc < 2 || strcmp(argv[1], "clone") != 0) return 1;
    parse_config("workspace.cfg");
    process_jobs();
    free_jobs();
    return 0;
}
