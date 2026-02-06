#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

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

// Global Job List
JobList jobList = {NULL, 0, 0};

// Helper to check if file exists
int file_exists(const char *filename) {
    struct stat buffer;
    return (stat(filename, &buffer) == 0);
}

// Helper to check if directory exists
int dir_exists(const char *path) {
    struct stat info;
    if (stat(path, &info) != 0) {
        return 0;
    }
    return (info.st_mode & S_IFDIR) ? 1 : 0;
}

// Add job to list
void add_job(const char *repo, const char *branch, const char *path) {
    if (jobList.count >= jobList.capacity) {
        size_t new_capacity = jobList.capacity == 0 ? 16 : jobList.capacity * 2;
        Job *new_jobs = realloc(jobList.jobs, new_capacity * sizeof(Job));
        if (!new_jobs) {
            fprintf(stderr, "Memory allocation failed\n");
            exit(1);
        }
        jobList.jobs = new_jobs;
        jobList.capacity = new_capacity;
    }

    Job *job = &jobList.jobs[jobList.count++];
    job->repo = strdup(repo);
    job->branch = branch ? strdup(branch) : NULL;
    job->path = strdup(path);
}

// Free all allocated memory in the job list
void free_jobs() {
    for (size_t i = 0; i < jobList.count; i++) {
        free(jobList.jobs[i].repo);
        if (jobList.jobs[i].branch) free(jobList.jobs[i].branch);
        free(jobList.jobs[i].path);
    }
    free(jobList.jobs);
    jobList.jobs = NULL;
    jobList.count = 0;
    jobList.capacity = 0;
}

// Extract value for a key from a line
char *extract_value(const char *line, const char *key) {
    char *pos = strstr(line, key);
    if (!pos) return NULL;

    pos += strlen(key);
    char *start_quote = strchr(pos, '"');
    if (!start_quote) return NULL;
    start_quote++; 

    char *end_quote = strchr(start_quote, '"');
    if (!end_quote) return NULL;

    size_t len = end_quote - start_quote;
    char *value = malloc(len + 1);
    if (value) {
        strncpy(value, start_quote, len);
        value[len] = '\0';
    }
    return value;
}

// Parse a single config file
void parse_config(const char *filename) {
    printf("Parsing %s...\n", filename);
    FILE *file = fopen(filename, "r");
    if (!file) {
        printf("Warning: Could not open %s\n", filename);
        return;
    }

    char line[MAX_LINE_LENGTH];
    int line_num = 0;

    while (fgets(line, sizeof(line), file)) {
        line_num++;
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        
        if (*p == '\0' || *p == '#' || *p == '\n' || *p == '\r') continue;

        char *repo = extract_value(line, "REPO");
        char *branch = extract_value(line, "BRANCH");
        char *path = extract_value(line, "PATH");

        if (!repo) {
            fprintf(stderr, "Error in %s line %d: REPO not found.\n", filename, line_num);
            if (path) free(path);
            if (branch) free(branch);
            continue;
        }

        if (!path) {
            char *base = strrchr(repo, '/');
            base = base ? base + 1 : repo;
            char *dotgit = strstr(base, ".git");
            size_t base_len = dotgit ? (size_t)(dotgit - base) : strlen(base);
            path = malloc(base_len + 3);
            sprintf(path, "./%.*s", (int)base_len, base);
        }

        int is_duplicate = 0;
        for (size_t i = 0; i < jobList.count; i++) {
            if (strcmp(jobList.jobs[i].path, path) == 0) {
                int same_repo = strcmp(jobList.jobs[i].repo, repo) == 0;
                int same_branch = 0;
                
                if (!branch && !jobList.jobs[i].branch) same_branch = 1;
                else if (branch && jobList.jobs[i].branch) {
                    same_branch = strcmp(jobList.jobs[i].branch, branch) == 0;
                }

                if (!same_repo || !same_branch) {
                    fprintf(stderr, "Error: Conflict detected for path '%s'.\n", path);
                    exit(1);
                }
                is_duplicate = 1;
                break;
            }
        }

        if (!is_duplicate) {
            add_job(repo, branch, path);
        }
        
        free(repo);
        if (branch) free(branch);
        free(path);
    }
    fclose(file);
}

void process_jobs() {
    printf("Starting clone process...\n");
    for (size_t i = 0; i < jobList.count; i++) {
        Job *job = &jobList.jobs[i];
        printf("---------------------------------------------------\n");
        printf("Processing [%zu/%zu] %s...\n", i + 1, jobList.count, job->path);

        if (dir_exists(job->path)) {
            printf("Directory '%s' already exists. Skipping clone.\n", job->path);
        } else {
            char command[MAX_LINE_LENGTH * 2];
            if (job->branch) {
                snprintf(command, sizeof(command), "git clone -b %s %s %s", job->branch, job->repo, job->path);
            } else {
                snprintf(command, sizeof(command), "git clone %s %s", job->repo, job->path);
            }
            
            printf("Running: %s\n", command);
            if (system(command) != 0) {
                fprintf(stderr, "Error: Clone failed for %s\n", job->repo);
                exit(1);
            }
        }

        char dep_path[MAX_PATH_LENGTH];
        snprintf(dep_path, sizeof(dep_path), "%s/dependencies.cfg", job->path);
        if (file_exists(dep_path)) {
            parse_config(dep_path);
        }
    }
}

void save_dependencies() {
    FILE *f = fopen("dependencies.txt", "w");
    if (!f) return;
    
    for (size_t i = 0; i < jobList.count; i++) {
        fprintf(f, "REPO \"%s\" BRANCH \"%s\" PATH \"%s\"\n", 
            jobList.jobs[i].repo, 
            jobList.jobs[i].branch ? jobList.jobs[i].branch : "HEAD", 
            jobList.jobs[i].path);
    }
    fclose(f);
}

int main(int argc, char *argv[]) {
    if (argc < 2 || strcmp(argv[1], "clone") != 0) {
        printf("Usage: %s clone\n", argv[0]);
        return 1;
    }

    if (!file_exists("workspace.cfg")) {
        fprintf(stderr, "Error: workspace.cfg not found.\n");
        return 1;
    }
    
    parse_config("workspace.cfg");
    process_jobs();
    save_dependencies();
    
    free_jobs(); // Clean up memory before exiting
    return 0;
}
