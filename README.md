# GitManager

**GitManager** is a lightweight CLI tool designed to manage multi-repository workspaces. It automates the cloning of repositories defined in a configuration file and handles recursive dependencies.

## Features

- **Workspace Management**: Clones multiple repositories defined in a `workspace.cfg` file.
- **Recursive Dependencies**: Automatically detects and processes `dependencies.cfg` files within cloned repositories.
- **Conflict Detection**: Ensures that multiple configurations do not attempt to clone different repositories into the same path.
- **Report Generation**: Produces a `dependencies.txt` file listing all cloned repositories and their paths.
- **Lightweight**: Written in C for speed and portability, with no external runtime dependencies beyond Git.

## Installation

1. Compile the source code:
   ```bash
   gcc gitmanager.c -o gitmanager.exe
   ```
2. Move `gitmanager.exe` to a directory in your system PATH (e.g., `C:\Users\YourUser\bin`).

## Usage

Navigate to a folder containing a `workspace.cfg` file and run:

```bash
gitmanager clone
```

### Configuration Format

Create a `workspace.cfg` (or `dependencies.cfg` in sub-repos) with the following syntax:

```ini
REPO "https://github.com/user/repo.git" [BRANCH "branch_name"] [PATH "./destination/path"]
```

- **REPO**: (Required) The Git repository URL.
- **BRANCH**: (Optional) The specific branch to clone. Defaults to `HEAD` (default branch) if omitted.
- **PATH**: (Optional) The local destination path. Defaults to `./<repo_name>` if omitted.
- Lines starting with `#` are ignored.


## Output

After execution, a `dependencies.txt` file is generated in the root directory, providing a snapshot of the workspace state.
